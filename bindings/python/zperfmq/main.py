#!/usr/bin/env python3
import os
import json
import time
import click
import psutil
import numpy as np
import subprocess
import matplotlib.pyplot as plt
from collections import defaultdict, namedtuple

known_meaures = 'RLAT SLAT RTHR STHR RECV SEND ECHO YODEL'.split()


def kmgt(number):
    unum = (len(str(number))-1)//3
    return "%.0f%s" %(number/10**(3*unum), " kMGT"[unum])

def parse_kmgt(text):
    text = text.lower()
    if text.endswith('k'):
        return int(float(text[:-1])*1e3)
    if text.endswith('m'):
        return int(float(text[:-1])*1e6)
    if text.endswith('g'):
        return int(float(text[:-1])*1e9)
    if text.endswith('t'):
        return int(float(text[:-1])*1e12)
    return int(text)

def map_msgsize(msgsizes, size_units):
    if not msgsizes:
        raise ValueError("must supply at least one message size")

    smeth = lambda x: int(float(x))
    if size_units == "log2":
        smeth = lambda x: int(2**float(x))
    if size_units == "log10":
        smeth = lambda x: int(10**float(x))

    return list(map(smeth, msgsizes))
    
    

@click.group()
def cli():
    pass

@cli.command("test")
def test():
    '''
    Run zperf built-in tests
    '''
    from zperfmq import Zperf
    Zperf.test(1)

@cli.command("plan")
@click.option('-t', '--niothreads', default=1,
              help="Number of ZeroMQ I/O threads to use (def=1)")
@click.option('-j', '--nconnects', default=1,
              help="Number of simultaneous socket connections (def=1)")
@click.option('-a', '--address', default="127.0.0.1",
              help="The IP address to bind and connect")
@click.option('-p', '--port', default="5678",
              help="The IP port")
@click.option('-m', '--measurement', default="latency",
              type=click.Choice(['clat','zlat','cthr','zthr']),
              help="Measurement (def=latency)")
@click.option('--reverse/--no-reverse', 
              help="Reverse makes src bind and dst connect")
@click.option('--min-nmsgs', default=2000,
              help="Lower bound on the number of messages in one measure")
@click.option('--max-nmsgs', default=int(2**20),
              help="Upper bound on the number of messages in one measure")
@click.option('-M', '--size-metric', default='log2',
              type=click.Choice(['linear','log2','log10']),
              help="Interpretation of message size in bytes (def='log2')")
@click.option('-T', '--total-data', default="10G",
              help="Approximate total data for each test, may use k/M/G multipliers, (def 10G)")
@click.option('-o', '--output', type=click.File('wb'), default='-',
              help="Output file")
@click.argument("msgsizes", nargs=-1)
def plan(niothreads, nconnects, address, port, measurement, reverse,
         min_nmsgs, max_nmsgs, size_metric, total_data,
         output, msgsizes):
    '''
    Generate a plan for a sequence of measurements.

    The plan can then be used to run the sequence and to assist in
    making plots of their results.

    If running from bash, a conveient way to supply message size is
    with brace expansion, eg: -M log2 {2..18}
    '''

    # normally src (echo,sthr,send) binds and dst (yodel,rthr,recv) connects
    # reverse has src connect and dst bind.

    if measurement.startswith("zthr"): # libzmq throughput
        peers = dict (dst = dict(measure='RTHR', socket = 'PULL'),
                      src = dict(measure='STHR', socket = 'PUSH'))
    elif measurement.startswith("cthr"): # czmq throughput
        peers = dict (dst = dict(measure='RECV', socket = 'PULL'),
                      src = dict(measure='SEND', socket = 'PUSH'))
    elif measurement.startswith("zlat"): # libzmq latency
        peers = dict (dst = dict(measure='RLAT', socket = 'REQ'),
                      src = dict(measure='SLAT', socket = 'REP'))
    elif measurement.startswith("clat"): # czmq latency
        peers = dict (dst = dict(measure='YODEL', socket = 'REQ'),
                      src = dict(measure='ECHO',  socket = 'REP'))
    else:
        raise RuntimeErrot("unknown measurement: %s" % measurement)
    # the "sender" should always wait.
    peers['src']['zyre'] = '%s-%s' % (measurement, peers['src']['measure'].lower())
    peers['dst']['zyre'] = '%s-%s' % (measurement, peers['dst']['measure'].lower())
    peers['src']['wait_for'] = peers['dst']['zyre']

    peers['src']['borc'] = 'bind'
    peers['dst']['borc'] = 'connect'
    if reverse:
        peers['src']['borc'] = 'connect'
        peers['dst']['borc'] = 'bind'

    argpat = \
        '--niothreads {niothreads} --nconnects {nconnects} ' + \
        '--{borc} tcp://{address}:{port} --name {zyre} --measurement {measure} ' + \
        '--socket-type {socket}'

    src_args = argpat.format(**peers['src'], **locals())
    dst_args = argpat.format(**peers['dst'], **locals())
    if 'wait_for' in peers['src']:
        src_args += ' --wait %s' % peers['src']['wait_for']
    if 'wait_for' in peers['dst']:
        dst_args += ' --wait %s' % peers['dst']['wait_for']
    

    peers['src']['args'] = src_args
    peers['dst']['args'] = dst_args

    del(src_args)
    del(dst_args)
    del(argpat)

    msgsizes = map_msgsize(msgsizes, size_metric)

    total_data = parse_kmgt(total_data)

    res = dict(locals());
    res.pop("output")

    output.write(( json.dumps(res, indent=4) + "\n").encode() )




def get_sysinfo():
    'Return dictionary describing system'

    lspci = defaultdict(list)
    for line in subprocess.check_output("lspci").decode().split('\n'):
        line = line.strip()
        if not line: continue
        addr, rest = line.split(' ',1)
        cat, desc = rest.split(':',1)
        lspci[cat].append(dict(addr=addr, desc=desc.strip()))

    if_info = defaultdict(dict)
    for nic, addrs in psutil.net_if_addrs().items():
        protos = dict()
        for addr in addrs:
            protos[addr.family.name] = dict(address = addr.address,
                                            netmask = addr.netmask,
                                            broadcast = addr.broadcast)
        if_info[nic]['protos'] = protos
    for nic, stats in psutil.net_if_stats().items():
        if_info[nic]['isup'] = stats.isup
        if_info[nic]['duplex'] = stats.duplex.value
        if_info[nic]['speed'] = stats.speed
        if_info[nic]['mtu'] = stats.mtu
        
        

    u = os.uname()
    ret = dict(uid=os.getuid(), gid=os.getgid(),
               uname = dict(
                   sysname=u.sysname, nodename=u.nodename,
                   release=u.release, version=u.version, machine=u.machine),
               nproc = len(os.sched_getaffinity(0)),
               memory = os.sysconf('SC_PAGE_SIZE') * os.sysconf('SC_PHYS_PAGES'),
               nics = if_info,
               lspci = lspci,
               lscpu = {k:v.strip() for line in subprocess.check_output("lscpu").decode().split('\n') if line.strip()
                        for k,v in (line.split(":"),)},
    )
    return ret


@cli.command('sysinfo')
@click.option('--ssh', default="",
              help='SSH connection string, default uses the "address" in the plan')
@click.option('-o', '--output', type=click.File('wb'), default='-',
              help="Output file")
def sysinfo(ssh, output):
    '''
    Produce JSON output describing system
    '''
    if not ssh:
        res = get_sysinfo()
        output.write(json.dumps(res, indent=4).encode())
        return
    
    sshcmd = '''ssh %s bash --login -c "'zperf sysinfo'"''' % ssh
    res = subprocess.check_output(sshcmd, shell = True)
    output.write(res);
    return

def parse_run_return(text):
    text = text.decode()
    lines = text.split('\n')
    while lines and not lines[0].startswith('{'):
        lines.pop(0)
    return json.loads("\n".join(lines))


def remote_popen(ssh, cmd):
    sshcmd = '''ssh %s bash --login -c "'%s'"''' %(ssh, cmd)
    print(sshcmd)
    return subprocess.Popen(sshcmd, shell = True, stdout = subprocess.PIPE)

def remote_sysinfo(ssh):
    sshpat = '''ssh %s bash --login -c "'zperf sysinfo'"'''
    sshcmd = sshpat % ssh
    return json.loads(subprocess.check_output(sshcmd, shell = True))

@cli.command('run')
@click.option('-s', '--src-ssh', type=str, default="127.0.0.1",
              help='SSH connection string for src')
@click.option('-d', '--dst-ssh', type=str, default="127.0.0.1",
              help='SSH connection string for dst')
@click.option('-o', '--output', type=click.File('wb'), default='-',
              help="Output file")
@click.argument('planfile', type=click.File('rb'))
def run(src_ssh, dst_ssh, output, planfile):
    '''
    Execute a measurement plan.
    '''

    sysinfo = dict(src = remote_sysinfo(src_ssh),
                   dst = remote_sysinfo(dst_ssh))

    plan = json.loads(planfile.read())
    print (list(plan.keys()))
    min_nmsgs = plan['min_nmsgs']
    max_nmsgs = plan['max_nmsgs']

    src_plan = plan['peers']['src']
    dst_plan = plan['peers']['dst']

    results = list()
    for msgsize in plan['msgsizes']:
        nmsgs = int(plan['total_data']//msgsize)
        if nmsgs == 0:
            print("Not enough data volume for messages of size %d" % msgsize)
            continue
        if nmsgs > max_nmsgs:
            nmsgs = max_nmsgs
            print("truncating number of messages to %d" % nmsgs)
        if nmsgs < min_nmsgs:
            nmsgs = min_nmsgs
            print("uplifting number of messages to %d" % nmsgs)
        args = " --nmsgs {nmsgs} --msgsize {msgsize} -o -".format(nmsgs=nmsgs, msgsize=msgsize)

        src_cmd = "zperfcli %s %s" % (src_plan['args'], args)
        dst_cmd = "zperfcli %s %s" % (dst_plan['args'], args)

        src_proc = remote_popen(src_ssh, src_cmd)
        dst_proc = remote_popen(dst_ssh, dst_cmd)
        src_proc.wait()
        dst_proc.wait()

        src_dat = parse_run_return(src_proc.stdout.read())
        dst_dat = parse_run_return(dst_proc.stdout.read())

        src_dat['nmsgs'] = nmsgs
        dst_dat['nmsgs'] = nmsgs

        src_dat['msgsize'] = msgsize
        dst_dat['msgsize'] = msgsize

        results.append(dict(src = src_dat, dst = dst_dat))
    final = dict(sysinfo=sysinfo, results=results, plan = plan)
    output.write(( json.dumps(final, indent=4) + "\n").encode() )


def flatten_results(plans, results):
    '''
    Convert from list of objects to object of arrays
    '''
    dat = list()
    for p,r in zip(plans, results):
        dat.append((p['msgsize'], p['nmsgs'],
                    r['time_us'], r['cpu_us'], r['nbytes'], r['noos']))
    dat.sort()
    arr = np.asarray(dat).T
    return dict(
        msgsize = arr[0],
        nmsgs = arr[1],
        time_us = arr[2],
        cpu_us = arr[3],
        nbytes = arr[4],
        noos = arr[5])


def attachment(plan):
    if type(plan) == list:
        plan = plan[0]
    if 'bind' in plan:
        return 'bind', plan['bind']
    return 'connect', plan['connect']



def get_net_info(nics, subnet):
    # quick and dirty, and only good for /24 subnets.
    subnet = subnet.split('.')[:-1]
    ret = dict()
    for nic, dat in nics.items():
        for pd in dat['protos'].values():
            sn = pd['address'].split('.')[:-1]
            if sn == subnet:
                ret[nic] = dat
    return ret

def result_arrays(dat, measurement="RECV"):
    arrs = list()
    for res in dat['results']:
        for sd in ['src','dst']:
            r = res[sd]
            if r['measurement'] != measurement:
                continue
            arrs.append((r['nmsgs'], r['msgsize'],
                         r['time_us'], r['cpu_us'], 
                         r['nbytes'], r['noos']))
    return np.asarray(arrs).T

def get_mtu(results):
    plan = results['plan']
    si = results['sysinfo']
    src_ni = get_net_info(si['src']['nics'], plan['address'])
    dst_ni = get_net_info(si['dst']['nics'], plan['address'])
    mtus = [n['mtu'] for d,n in list(src_ni.items()) + list(dst_ni.items())]
    assert (len(mtus) == 2)
    return min(map(int, mtus))
        
def get_speed(results):
    plan = results['plan']
    si = results['sysinfo']
    src_ni = get_net_info(si['src']['nics'], plan['address'])
    dst_ni = get_net_info(si['dst']['nics'], plan['address'])
    speeds = [n['speed'] for d,n in list(src_ni.items()) + list(dst_ni.items())]
    assert (len(speeds) == 2)
    return min(map(int, speeds))
        
    
def results_object(results, measurement):
    arrs = result_arrays(results, measurement)
    #nmsgs,msgsize,time_us,cpu_us,nbytes,noos = arrs
    mtu = get_mtu(results)
    speed = get_speed(results)
    plan = results['plan']
    Results = namedtuple("Results","niothreads nconnects mtu speed nmsgs msgsize time_us cpu_us nbytes noos")
    return Results(plan['niothreads'], plan['nconnects'], mtu, speed, *arrs);

@cli.command('plot-lat')
@click.option('-m', '--measure', type=click.Choice(known_meaures),
              help='Set the measure for which the CPU is taken')
@click.argument('results', type=click.File('rb'), default='-')
@click.argument('pltfile', type=click.Path(), default='-')
def plot_lat(measure, results, pltfile):
    '''
    Generate latency result plot
    '''
    results = json.loads(results.read())
    robj = results_object(results, measure)

    name = dict(SLAT = 'libzmq echo',
                RLAT = 'libzmq yodel',
                ECHO = 'CZMQ echo',
                YODEL= 'CZMQ yodel')[measure]

    title = "%s TCP latency (mtu:%d #th/conn:%d/%d)" % \
        (name, robj.mtu, robj.niothreads, robj.nconnects)

    plt.loglog(robj.msgsize, robj.time_us/(2.0*robj.nmsgs),
               marker='o', color='tab:red')
    
    plt.xlabel('Message size [B]')
    plt.ylabel('One-way latency [us]')
    plt.grid(True)
    plt.title(title)
    plt.savefig(pltfile)
    print(pltfile)

@cli.command('plot-czlat')
@click.argument('cresults', type=click.File('rb'), default='-')
@click.argument('zresults', type=click.File('rb'), default='-')
@click.argument('pltfile', type=click.Path(), default='-')
def plot_czlat(cresults, zresults, pltfile):
    '''
    Generate latency result plot for note
    '''
    cresults = json.loads(cresults.read())
    cobj = results_object(cresults, 'YODEL')
    zresults = json.loads(zresults.read())
    zobj = results_object(zresults, 'RLAT')

    assert(cobj.mtu == zobj.mtu)

    title = "TCP Latency (mtu:%d)" % \
        (cobj.mtu, )

    plt.loglog(cobj.msgsize, cobj.time_us/(2.0*cobj.nmsgs),
               label='CZMQ', marker='.', color='tab:olive')
    plt.loglog(zobj.msgsize, zobj.time_us/(2.0*zobj.nmsgs),
               label='libzmq', marker='o', color='tab:red')
    
    plt.xlabel('Message size [B]')
    plt.ylabel('One-way latency [us]')
    plt.legend()
    plt.grid(True)
    plt.title(title)
    plt.savefig(pltfile)
    print(pltfile)

@cli.command('plot-rate')
@click.argument('results', type=click.File('rb'), default='-')
@click.argument('pltfile', type=click.Path(), default='-')
def plot_rate(results, pltfile):
    '''
    Generate message rate result plot for note
    '''
    measure = 'YODEL'
    results = json.loads(results.read())
    robj = results_object(results, measure)

    title = "CZMQ echo rate (mtu:%d)" % \
        (robj.mtu, )

    plt.loglog(robj.msgsize, 1e6*robj.nmsgs/robj.time_us,
               marker='o', color='tab:red')
    
    plt.xlabel('Message size [B]')
    plt.ylabel('Synchronous message rate [Hz]')
    plt.grid(True)
    plt.title(title)
    plt.savefig(pltfile)
    print(pltfile)



@cli.command('plot-thr')
@click.option('-m', '--measure',
              type=click.Choice(known_meaures),
              help='Set the measure for which the CPU is taken')
@click.argument('results', type=click.File('rb'), default='-')
@click.argument('pltfile', type=click.Path(), default='-')
def plot_thr(measure, results, pltfile):
    '''
    Generate throughput result plot
    '''
    results = json.loads(results.read())
    robj = results_object(results, measure)

    name = dict(RECV = 'libzmq recv',
                RTHR = 'CZMQ recv',
                SEND = 'libzmq send',
                STHR = 'CZMQ send')[measure]

    title = "%s TCP throughput (mtu:%d #th/conn:%d/%d)" % \
        (name, robj.mtu, robj.niothreads, robj.nconnects)



    plt.loglog(robj.msgsize, 8e-3*robj.nbytes/robj.time_us,
               marker='o', color='tab:blue')
    
    plt.xlabel('Message size [B]')
    plt.ylabel('Throughput [Gbps]')
    plt.grid(True)
    plt.title(title)
    plt.savefig(pltfile)
    print(pltfile)

@cli.command('plot-thr-cz')
@click.option('-s', '--side',
              type=click.Choice(['send','recv']),
              default='recv',
              help='Set the measure for which the CPU is taken')
@click.option('--scale',
              type=click.Choice(['loglog','semilogx']),
              default='loglog',
              help='Set plot scale')
@click.option('--msgsizes', nargs=2, type=float, default=(1,1<<30))
@click.argument('cresults', type=click.File('rb'), default='-')
@click.argument('zresults', type=click.File('rb'), default='-')
@click.argument('pltfile', type=click.Path(), default='-')
def plot_thr_cz(side, scale, msgsizes,
                cresults, zresults, pltfile):
    '''
    Generate throughput result plot, CZMQ/libzmq comparison
    '''
    zresults = json.loads(zresults.read())
    cresults = json.loads(cresults.read())
    if side == 'recv':
        zobj = results_object(zresults, 'RTHR')
        cobj = results_object(cresults, 'RECV')
    else:
        zobj = results_object(zresults, 'STHR')
        cobj = results_object(cresults, 'SEND')
        

    assert(cobj.mtu == zobj.mtu)
    assert(cobj.niothreads == zobj.niothreads)
    assert(cobj.nconnects == zobj.nconnects)


    stmt = '1 I/O thread'
    if cobj.niothreads > 1:
        stmt = '%d I/O threads' % cobj.niothreads

    title = "TCP throughput (%s, mtu:%d)" % \
        (stmt, cobj.mtu)

    pmeth = getattr(plt, scale)

    pts = zobj.msgsize>0
    if msgsizes:
        pts1 = zobj.msgsize>=msgsizes[0]
        pts2 = zobj.msgsize<=msgsizes[1]
        pts = pts1 * pts2

    pmeth(zobj.msgsize[pts], 8e-3*zobj.nbytes[pts]/zobj.time_us[pts],
          label='libzmq', marker='o', color='tab:blue')
    pmeth(cobj.msgsize[pts], 8e-3*cobj.nbytes[pts]/cobj.time_us[pts],
          label='CZMQ', marker='.', color='tab:olive')
    
    plt.xlabel('Message size [B]')
    plt.ylabel('Throughput [Gbps]')
    plt.legend()
    plt.grid(True)
    plt.title(title)
    plt.savefig(pltfile)
    print(pltfile)

@cli.command('plot-zthr-sm')
@click.option('-s', '--side',
              type=click.Choice(['send','recv']),
              default='recv',
              help='Set the measure for which the CPU is taken')
@click.option('--scale',
              type=click.Choice(['loglog','semilogx']),
              default='loglog',
              help='Set plot scale')
@click.option('--msgsizes', nargs=2, type=float, default=(1,1<<30))
@click.argument('sresults', type=click.File('rb'), default='-')
@click.argument('mresults', type=click.File('rb'), default='-')
@click.argument('pltfile', type=click.Path(), default='-')
def plot_zthr_sm(side, scale, msgsizes,
                 sresults, mresults, pltfile):
    '''
    Generate throughput result plot, single/multi thread comparison
    '''
    sresults = json.loads(sresults.read())
    mresults = json.loads(mresults.read())
    if side == 'recv':
        sobj = results_object(sresults, 'RTHR')
        mobj = results_object(mresults, 'RTHR')
    else:
        sobj = results_object(sresults, 'STHR')
        mobj = results_object(mresults, 'STHR')
        
    assert(sobj.mtu == mobj.mtu)
    assert(sobj.niothreads == 1)
    assert(sobj.nconnects == 1)
    assert(sobj.niothreads != mobj.niothreads)
    assert(sobj.nconnects != mobj.nconnects)

    title = "libzmq throughput (mtu:%d)" % \
        (sobj.mtu,)

    pmeth = getattr(plt, scale)

    pts = sobj.msgsize>0
    if msgsizes:
        pts1 = sobj.msgsize>=msgsizes[0]
        pts2 = sobj.msgsize<=msgsizes[1]
        pts = pts1 * pts2

    pmeth(sobj.msgsize[pts], 8e-3*sobj.nbytes[pts]/sobj.time_us[pts],
          label='single', marker='o', color='tab:blue')
    pmeth(mobj.msgsize[pts], 8e-3*mobj.nbytes[pts]/mobj.time_us[pts],
          label='multi', marker='.', color='tab:purple')
    
    plt.xlabel('Message size [B]')
    plt.ylabel('Throughput [Gbps]')
    plt.legend()
    plt.grid(True)
    plt.title(title)
    plt.savefig(pltfile)
    print(pltfile)

@cli.command('plot-zthr-mtu')
@click.option('-s', '--side',
              type=click.Choice(['send','recv']),
              default='recv',
              help='Set the measure for which the CPU is taken')
@click.option('--scale',
              type=click.Choice(['loglog','semilogx']),
              default='loglog',
              help='Set plot scale')
@click.option('--msgsizes', nargs=2, type=float, default=(1,1<<30))
@click.argument('nresults', type=click.File('rb'), default='-')
@click.argument('jresults', type=click.File('rb'), default='-')
@click.argument('pltfile', type=click.Path(), default='-')
def plot_zthr_mtu(side, scale, msgsizes,
                  nresults, jresults, pltfile):
    '''
    Generate throughput result plot, nominal/jumbo MTU comparison
    '''
    nresults = json.loads(nresults.read())
    jresults = json.loads(jresults.read())
    if side == 'recv':
        nobj = results_object(nresults, 'RTHR')
        jobj = results_object(jresults, 'RTHR')
    else:
        nobj = results_object(nresults, 'STHR')
        jobj = results_object(jresults, 'STHR')
        
    assert(nobj.mtu == 1500)
    assert(jobj.mtu == 9000)
    assert(nobj.niothreads == jobj.niothreads)
    assert(nobj.nconnects == jobj.nconnects)

    # title = "libzmq TCP throughput (%s side) (#th/conn:%d/%d)" % \
    #     (side, nobj.niothreads, nobj.nconnects)
    title ="libzmq throughput (%d threads)" % nobj.niothreads

    pmeth = getattr(plt, scale)

    pts = nobj.msgsize>0
    if msgsizes:
        pts1 = nobj.msgsize>=msgsizes[0]
        pts2 = nobj.msgsize<=msgsizes[1]
        pts = pts1 * pts2

    pmeth(nobj.msgsize[pts], 8e-3*nobj.nbytes[pts]/nobj.time_us[pts],
          label='nominal', marker='o', color='tab:blue')
    pmeth(jobj.msgsize[pts], 8e-3*jobj.nbytes[pts]/jobj.time_us[pts],
          label='jumbo', marker='.', color='tab:purple')
    
    plt.xlabel('Message size [B]')
    plt.ylabel('Throughput [Gbps]')
    plt.legend()
    plt.grid(True)
    plt.title(title)
    plt.savefig(pltfile)
    print(pltfile)

@cli.command('plot-cpu')
@click.option('-m', '--measure',
              type=click.Choice(known_meaures),
              help='Set the measure for which the CPU is taken')
@click.argument('results', type=click.File('rb'), default='-')
@click.argument('pltfile', type=click.Path(), default='-')
def plot_cpu(measure, results, pltfile):
    '''
    Plot CPU usage.
    '''
    results = json.loads(results.read())
    robj = results_object(results, measure)

    name = dict(RECV = 'libzmq recv',
                RTHR = 'CZMQ recv',
                SEND = 'libzmq send',
                STHR = 'CZMQ send',
                ECHO = 'CZMQ echo',
                YODEL= 'CZMQ yodel')[measure]

    title = "%s CPU usage (mtu:%d #th/conn:%d/%d)" % \
        (name, robj.mtu, robj.niothreads, robj.nconnects)

    plt.semilogx(robj.msgsize, 100.0*robj.cpu_us/robj.time_us,
                 marker='o', color='tab:orange')

    plt.xlabel('Message size [B]')
    plt.ylabel('CPU [%]');
    plt.grid(True)
    plt.title(title)
    plt.tight_layout()  # otherwise the right y-label is slightly clippe
    plt.savefig(pltfile)
    print(pltfile)

@cli.command('plot-thr-cpu-cz')
@click.option('--msgsizes', nargs=2, type=float, default=(1,1<<30))
@click.argument('cresults', type=click.File('rb'), default='-')
@click.argument('zresults', type=click.File('rb'), default='-')
@click.argument('pltfile', type=click.Path(), default='-')
def plot_cpu_cz(msgsizes, cresults, zresults, pltfile):
    '''
    Plot CPU usage.
    '''
    zresults = json.loads(zresults.read())
    cresults = json.loads(cresults.read())
    zobj = results_object(zresults, 'RTHR')
    cobj = results_object(cresults, 'RECV')

    assert(cobj.mtu == zobj.mtu)
    assert(cobj.niothreads == zobj.niothreads)
    assert(cobj.nconnects == zobj.nconnects)

    # title = "Throughput CPU usage, recv side (mtu:%d #th/conn:%d/%d)" % \
    #     (cobj.mtu, cobj.niothreads, cobj.nconnects)
    title = 'CPU Usage'

    pts = cobj.msgsize>0
    if msgsizes:
        pts1 = cobj.msgsize>=msgsizes[0]
        pts2 = cobj.msgsize<=msgsizes[1]
        pts = pts1 * pts2

    plt.semilogx(cobj.msgsize[pts], 100.0*cobj.cpu_us[pts]/cobj.time_us[pts],
                 label='CZMQ', marker='.', color='tab:orange')
    plt.semilogx(zobj.msgsize[pts], 100.0*zobj.cpu_us[pts]/zobj.time_us[pts],
                 label='libzmq', marker='o', color='tab:red')

    plt.xlabel('Message size [B]')
    plt.ylabel('CPU [%]');
    plt.grid(True)
    plt.legend()
    plt.title(title)
    plt.tight_layout()  # otherwise the right y-label is slightly clippe
    plt.savefig(pltfile)
    print(pltfile)
    
def main():
    cli()
    
