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

known_meaures = 'RTHR STHR RECV SEND ECHO YODEL'.split()


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
              help="The remote IP address")
@click.option('-p', '--port', default="5678",
              help="The IP port")
@click.option('-m', '--measurement', default="latency",
              type=click.Choice(['clat','zlat','cthr','zthr']),
              help="Measurement (def=latency)")
@click.option('--reverse/--no-reverse', 
              help="Reverse makes echo/send local connect and yodel/recv remote bind")
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

    # local allways connects, remote always binds
    # normally, local yodel/recv, remote is echo/send
    # reverse reverses this

    if measurement.startswith("zthr"):
        peers = dict (local  = dict(measure='RTHR', socket = 'PULL'),
                      remote = dict(measure='STHR', socket = 'PUSH'))
    elif measurement.startswith("cthr"):
        peers = dict (local  = dict(measure='RECV', socket = 'PULL'),
                      remote = dict(measure='SEND', socket = 'PUSH'))
    elif measurement.startswith("clat"):
        peers = dict (local  = dict(measure='YODEL', socket = 'REQ'),
                      remote = dict(measure='ECHO',  socket = 'REP'))
    if reverse:
        peers = dict(local=peers['remote'], remote=peers['local'])

    common = '--niothreads {niothreads} --nconnects {nconnects}'
    largs = (common + ' --connect tcp://{address}:{port}').format(**locals())
    rargs = (common + ' --bind tcp://{address}:{port} ').format(**locals())
    del(common)

    largs += ' --measurement {measure} --socket-type {socket}'.format(**peers['local'])
    rargs += ' --measurement {measure} --socket-type {socket}'.format(**peers['remote'])

    peers['local']['args'] = largs
    peers['remote']['args'] = rargs

    del(largs)
    del(rargs)

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
@click.option('-r', '--remote', default="",
              help="Remote SSH URL")
@click.option('-o', '--output', type=click.File('wb'), default='-',
              help="Output file")
def sysinfo(remote, output):
    '''
    Produce JSON output describing system
    '''
    if not remote:
        res = get_sysinfo()
        output.write(json.dumps(res, indent=4).encode())
        return
    
    sshcmd = '''ssh %s bash --login -c "'zperf sysinfo'"''' % remote
    res = subprocess.check_output(sshcmd, shell = True)
    output.write(res);
    return

def parse_run_return(text):
    text = text.decode()
    lines = text.split('\n')
    while lines and not lines[0].startswith('{'):
        lines.pop(0)
    return json.loads("\n".join(lines))


@cli.command('run')
@click.option('-o', '--output', type=click.File('wb'), default='-',
              help="Output file")
@click.argument('planfile', type=click.File('rb'))
@click.argument('remote', type=str, default="")
def run(output, planfile, remote):
    '''
    Execute a measurement plan.
    '''

    plan = json.loads(planfile.read())
    if not remote:
        remote = plan['address'] 
        print("Warning, using address <%s> for remote" % (remote,))

    lplan = plan['peers']['local']
    rplan = plan['peers']['remote']

    sshcmd = '''ssh %s bash --login -c "'zperf sysinfo'"''' % remote
    sysinfo = dict(local = get_sysinfo(),
                   remote = json.loads(subprocess.check_output(sshcmd, shell = True)))

    min_nmsgs = plan['min_nmsgs']
    max_nmsgs = plan['max_nmsgs']

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

        lcmd = "zperfcli %s %s" % (lplan['args'], args)
        rcmd = "zperfcli %s %s" % (rplan['args'], args)
        rcmd = '''ssh %s bash --login -c "'%s'"''' % (remote, rcmd)
        print (lcmd)
        lproc = subprocess.Popen(lcmd, shell = True, stdout = subprocess.PIPE)
        print (rcmd)
        rproc = subprocess.Popen(rcmd, shell = True, stdout = subprocess.PIPE)
        lproc.wait()
        rproc.wait()

        ldat = parse_run_return(lproc.stdout.read())
        rdat = parse_run_return(rproc.stdout.read())

        ldat['nmsgs'] = nmsgs
        rdat['nmsgs'] = nmsgs

        ldat['msgsize'] = msgsize
        rdat['msgsize'] = msgsize

        results.append(dict(local = ldat, remote=rdat))
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
        for lc in ['local','remote']:
            r = res[lc]
            if r['measurement'] != measurement:
                continue
            arrs.append((r['nmsgs'], r['msgsize'],
                         r['time_us'], r['cpu_us'], 
                         r['nbytes'], r['noos']))
    return np.asarray(arrs).T

def get_mtu(results):
    plan = results['plan']
    si = results['sysinfo']
    lni = get_net_info(si['local']['nics'], plan['address'])
    rni = get_net_info(si['remote']['nics'], plan['address'])
    mtus = [n['mtu'] for d,n in list(lni.items()) + list(rni.items())]
    assert (len(mtus) == 2)
    return min(map(int, mtus))
        
def get_speed(results):
    plan = results['plan']
    si = results['sysinfo']
    lni = get_net_info(si['local']['nics'], plan['address'])
    rni = get_net_info(si['remote']['nics'], plan['address'])
    speeds = [n['speed'] for d,n in list(lni.items()) + list(rni.items())]
    assert (len(speeds) == 2)
    return min(map(int, speeds))
        
    
def results_object(results, measurement):
    arrs = result_arrays(results, measurement)
    print (arrs.shape)
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

    title = "ZeroMQ %s TCP latency (mtu:%d #th/conn:%d/%d)" % (measure, robj.mtu, robj.niothreads, robj.nconnects)

    plt.loglog(robj.msgsize, robj.time_us/(2.0*robj.nmsgs),
               marker='o', color='tab:red')
    
    plt.xlabel('Message size [B]')
    plt.ylabel('One-way latency [us]')
    plt.grid(True)
    plt.title(title)
    plt.savefig(pltfile)

@cli.command('plot-thr')
@click.option('-m', '--measure',
              type=click.Choice(known_meaures),
              help='Set the measure for which the CPU is taken')
@click.argument('results', type=click.File('rb'), default='-')
@click.argument('pltfile', type=click.Path(), default='-')
def plot_lat(measure, results, pltfile):
    '''
    Generate throughput result plot
    '''
    results = json.loads(results.read())
    robj = results_object(results, measure)

    title = "ZeroMQ %s TCP throughput (mtu:%d #th/conn:%d/%d)" % (measure, robj.mtu, robj.niothreads, robj.nconnects)

    plt.loglog(robj.msgsize, 8e-3*robj.nbytes/robj.time_us,
               marker='o', color='tab:blue')
    
    plt.xlabel('Message size [B]')
    plt.ylabel('Throughput [Gbps]')
    plt.grid(True)
    plt.title(title)
    plt.savefig(pltfile)

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

    title = "ZeroMQ %s CPU usage (mtu:%d #th/conn:%d/%d)" % (measure, robj.mtu, robj.niothreads, robj.nconnects)

    plt.semilogx(robj.msgsize, 100.0*robj.cpu_us/robj.time_us,
                 marker='o', color='tab:orange')

    plt.xlabel('Message size [B]')
    plt.ylabel('CPU [%]');
    plt.grid(True)
    plt.title(title)
    plt.tight_layout()  # otherwise the right y-label is slightly clippe
    plt.savefig(pltfile)


def main():
    cli()
    
