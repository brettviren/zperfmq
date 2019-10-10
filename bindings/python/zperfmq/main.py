#!/usr/bin/env python3

import json
import time
import click
import matplotlib.pyplot as plt
import numpy as np

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
@click.option('-n', '--nmsgs', default=1000,
              help="Number of messages (def=1000)")
@click.option('-s', '--msgsize', default=1024,
              help="Message size in bytes (def=1024)")
@click.option('-b', '--bind', default="tcp://127.0.0.1:*",
              help="Address to bind")
@click.option('-c', '--connect', type=str,
              help="Address to connect")
@click.option('-m', '--measurement', default='echo',
              help="Measurement (def=echo)")
@click.option('-S', '--socket', default='REP',
              help="Socket type (def=REP)")
@click.option('-o', '--output', type=click.File('wb'), default='-',
              help="Output file")
def plan(niothreads, nconnects, nmsgs, msgsize, bind, connect, measurement, socket, output):
    '''
    Generate a plan for a single measurement.
    '''
    res = dict(locals());
    res.pop("output")
    if res['connect']:
        res.pop('bind')
    else:
        res.pop('connect')
    output.write(json.dumps(res, indent=4).encode())


def map_msgsize(msgsizes, size_units):
    if not msgsizes:
        raise ValueError("must supply at least one message size")
    msgsizes = map(float, msgsizes)

    smeth = lambda x: int(x)
    if size_units == "log2":
        smeth = lambda x: int(2**x)
    if size_units == "log10":
        smeth = lambda x: int(10**x)

    return map(smeth, msgsizes)
    

@cli.command("plan-lat")
@click.option('-t', '--niothreads', default=1,
              help="Number of ZeroMQ I/O threads to use (def=1)")
@click.option('-j', '--nconnects', default=1,
              help="Number of simultaneous socket connections (def=1)")
@click.option('-n', '--nmsgs', default=1000,
              help="Number of messages (def=1000)")
@click.option('-e', '--endpoint', default="tcp://127.0.0.1:5678",
              help="Fully qualiifed ZeroMQ address (def=tcp://127.0.0.1:5678")
@click.option('--reverse/--no-reverse', 
              help="Reverse makes echo connect and yodel bind")
@click.option('-M', '--size-units', default='log2',
              type=click.Choice(['linear','log2','log10']),
              help="Interpretation of message size in bytes (def='linear')")
@click.option('--echo', type=click.File('wb'), default='-',
              help="Plan file for 'echo' measurement")
@click.option('--yodel', type=click.File('wb'), default='-',
              help="Plan file for 'yodel' measurement")
@click.argument("msgsizes", nargs=-1)
def lat_plan(niothreads, nconnects, nmsgs, endpoint, reverse, size_units, echo, yodel, msgsizes):
    '''
    Produce two output files for a latency plan.
    '''
    msgsizes = map_msgsize(msgsizes, size_units)

    common  = dict(niothreads=niothreads,
                   nconnects=nconnects,
                   nmsgs=nmsgs)
    edef = dict(common, socket='REP', measurement='echo')
    ydef = dict(common, socket='REQ', measurement='yodel')
    if reverse:
        edef['connect'] = endpoint
        ydef['bind'] = endpoint
    else:
        edef['bind'] = endpoint
        ydef['connect'] = endpoint

    eplan=list()
    yplan=list()
    for msgsize in msgsizes:
        eplan.append(dict(edef, msgsize=msgsize))
        yplan.append(dict(ydef, msgsize=msgsize))

    echo.write(json.dumps(eplan, indent=4).encode())
    yodel.write(json.dumps(yplan, indent=4).encode())

@cli.command("plan-thr")
@click.option('-t', '--niothreads', default=1,
              help="Number of ZeroMQ I/O threads to use (def=1)")
@click.option('-j', '--nconnects', default=1,
              help="Number of simultaneous socket connections (def=1)")
@click.option('-v', '--volume', default="1G",
              help="Target data volume in bytes (def=1G, also can use M and k)")
@click.option('-e', '--endpoint', default="tcp://127.0.0.1:5678",
              help="Fully qualiifed ZeroMQ address (def=tcp://127.0.0.1:5678")
@click.option('--reverse/--no-reverse', 
              help="Reverse makes echo connect and yodel bind")
@click.option('-M', '--size-units', default='log2',
              type=click.Choice(['linear','log2','log10']),
              help="Interpretation of message size in bytes (def='linear')")
@click.option('--send', type=click.File('wb'), default='-',
              help="Plan file for 'send' measurement")
@click.option('--recv', type=click.File('wb'), default='-',
              help="Plan file for 'recv' measurement")
@click.argument("msgsizes", nargs=-1)
def thr_plan(niothreads, nconnects, volume, endpoint, reverse, size_units, send, recv, msgsizes):
    '''
    Produce two output files for a latency plan.
    '''
    msgsizes = map_msgsize(msgsizes, size_units)
    volume = parse_kmgt(volume)

    common  = dict(niothreads=niothreads,
                   nconnects=nconnects)
    sdef = dict(common, socket='PUSH', measurement='send')
    ddef = dict(common, socket='PULL', measurement='recv')
    if reverse:
        ddef['connect'] = endpoint
        sdef['bind'] = endpoint
    else:
        ddef['bind'] = endpoint
        sdef['connect'] = endpoint

    splan=list()
    dplan=list()
    for msgsize in msgsizes:
        nmsgs = int(volume//msgsize)
        if nmsgs == 0:
            print("Not enough data volume for messages of size %d" % msgsize)
            continue
        splan.append(dict(sdef, nmsgs=nmsgs, msgsize=msgsize))
        dplan.append(dict(ddef, nmsgs=nmsgs, msgsize=msgsize))

    send.write(json.dumps(splan, indent=4).encode())
    recv.write(json.dumps(dplan, indent=4).encode())


def get_sysinfo():
    'Return dictionary describing system'
    import os
    import psutil
    import subprocess
    from collections import defaultdict

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

def run_plan(plans):
    '''
    Run a measurement plan
    '''
    plural = True
    if type(plans) == dict:
        plural = False
        plans = [plans]

    import os
    import zmq

    # not sure if this will work....
    nthreads = plans[0].get('nthreads') or 1
    os.environ['ZSYS_IO_THREADS'] = str(nthreads)
    zmq.Context(nthreads)

    from zperfmq import Zperf


    def make_perf(plan):
        p = Zperf(getattr(zmq, plan['socket'].upper()))
        if plan.get('connect'):
            url = plan.get('connect').encode()
            for iconn in range(plan.get('nconnects') or 1):
                p.connect(url)
        else:
            p.bind(plan.get('bind').encode())
        return p

    # last_plan = None
    # def zperf_differs(plan):
    #     if last_plan is None:
    #         return True
    #     for key in ['connect','bind','nconnects','socket']:
    #         if last_plan.get(key) != plan.get(key):
    #             return True
    #     return False

    # perf = None
    # def check_plan(plan):
    #     if zperf_differs(plan):
    #         print("new zperf")
    #         nonlocal perf
    #         perf = make_perf(plan)
    #     nonlocal last_plan
    #     last_plan = plan

    def stringify(dat):
        t = 'connect'
        if 'bind' in dat:
            t = 'bind'
        a = dat[t]
        return '{measurement} {socket} {nmsgs} {att_type} {att_addr} {msgsize} {time_us}'.format(att_type=t, att_addr=a, **dat)

    ret = list()
    for plan in plans:
        # check_plan(plan)
        # assert(perf)
        perf = make_perf(plan)

        nmsgs, msgsize = plan['nmsgs'], plan['msgsize']
        meth = dict(echo  = lambda : perf.echo(nmsgs),
                    yodel = lambda : perf.yodel(nmsgs, msgsize),
                    send  = lambda : perf.send(nmsgs, msgsize),
                    recv  = lambda : perf.recv(nmsgs))[plan['measurement']]
        res = dict(time_us = meth())
        res["noos"] = perf.noos();
        res["nbytes"] = perf.bytes();
        res["cpu_us"] = perf.cpu();

        dat = dict(plan, **res);
        print (stringify(dat))

        ret.append(res)
        del(perf)

        print ("sleeping")
        time.sleep(3)

    if plural:
        return ret

    return ret[0]

@cli.command('sysinfo')
@click.option('-o', '--output', type=click.File('wb'), default='-',
              help="Output file")
def sysinfo(output):
    '''
    Produce JSON output describing system
    '''
    res = get_sysinfo()
    output.write(json.dumps(res, indent=4).encode())

@cli.command('run')
@click.argument('planfile', type=click.File('rb'), default='-')
@click.argument('runfile', type=click.File('wb'), default='-')
def run(planfile, runfile):
    '''
    Execute a measurement plan.
    '''
    plan = json.loads(planfile.read())
    res = run_plan(plan)
    ret = dict(results=res, plan=plan, sysinfo=get_sysinfo())
    runfile.write(json.dumps(ret, indent=4).encode())


def get_net_info(nics, addr):
    ip = addr.split('//')[1].split(':')[0]
    for nic, dat in nics.items():
        for pd in dat['protos'].values():
            if pd['address'] == ip:
                return dat
    return None

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



# def junk():
    # res = dat['results']
    # if type(res) == dict:
    #     res = [res]
    # plan = dat['plan']
    # if type(plan) == dict:
    #     plan = [plan]
    # si = dat['sysinfo']

    # ni = get_net_info(si['nics'], plan[0].get('bind') or plan[0].get('connect'));
    # if ni and ni['speed']:

def attachment(plan):
    if type(plan) == list:
        plan = plan[0]
    if 'bind' in plan:
        return 'bind', plan['bind']
    return 'connect', plan['connect']

def kmgt(number):
    unum = (len(str(number))-1)//3
    return "%.0f%s" %(number/10**(3*unum), " kMGT"[unum])

def parse_kmgt(text):
    text = text.lower()
    if text.endswith('k'):
        return float(text[:-1])*1e3
    if text.endswith('m'):
        return float(text[:-1])*1e6
    if text.endswith('g'):
        return float(text[:-1])*1e9
    if text.endswith('t'):
        return float(text[:-1])*1e12
    return float(text)

@cli.command('plot-thr')
@click.argument('sendfile', type=click.File('rb'), default='-')
@click.argument('recvfile', type=click.File('rb'), default='-')
@click.argument('pltfile', type=click.Path(), default='-')
def plot_thr(sendfile, recvfile, pltfile):
    '''
    Generate throughput result plot
    '''
    sdat = json.loads(sendfile.read())
    ddat = json.loads(recvfile.read())
    sres = sdat['results']
    dres = ddat['results']
    spln = sdat['plan']
    dpln = ddat['plan']
    sarr = flatten_results(spln, sres)
    darr = flatten_results(dpln, dres)

    fig, ax1 = plt.subplots()

    title='throughput'

    # PPS axis
    color = 'tab:red'
    ax1.set_xlabel('Message size [B]')
    ax1.set_ylabel('PPS [Mmsg/s]', color=color)
    ax1.semilogx(darr['msgsize'], darr['nmsgs'] / 1e6, label='PPS [Mmsg/s]', marker='x', color=color)
    ax1.tick_params(axis='y', labelcolor=color)

    # GBPS axis
    color = 'tab:blue'
    ax2 = ax1.twinx()  # instantiate a second axes that shares the same x-axis
    ax2.set_ylabel('Throughput [Gb/s]', color=color)
    ax2.semilogx(darr['msgsize'], 1e-3*darr['nbytes']/darr['time_us'], label='Throughput [Gb/s]', marker='o')
    # if is_tcp:
    #     ax2.set_yticks(np.arange(0, TCP_LINK_GPBS + 1, TCP_LINK_GPBS/10)) 
    ax2.tick_params(axis='y', labelcolor=color)
    ax2.grid(True)
    
    plt.title(title)
    fig.tight_layout()  # otherwise the right y-label is slightly clippe
    plt.savefig(pltfile)
    


@cli.command('plot-lat')
@click.argument('echofile', type=click.File('rb'), default='-')
@click.argument('yodelfile', type=click.File('rb'), default='-')
@click.argument('pltfile', type=click.Path(), default='-')
def plot_lat(echofile, yodelfile, pltfile):
    '''
    Generate latency result plot
    '''
    edat = json.loads(echofile.read())
    ydat = json.loads(yodelfile.read())
    eres = edat['results']
    yres = ydat['results']
    epln = edat['plan']
    ypln = ydat['plan']

    si = ydat['sysinfo']
    ni = get_net_info(si['nics'], attachment(ypln)[1])
    print (ni)
    speed = ""
    if ni and ni['speed']:
        speed = ", %sbps" % kmgt(ni['speed'])

    eatt,eaddr = attachment(epln)
    yatt,yaddr = attachment(ypln)
    scheme, rest = eaddr.split(':',1)

    earr = flatten_results(epln, eres)
    yarr = flatten_results(ypln, yres)

    title = "ZeroMQ %s latency %s/%s" % \
        (scheme.upper(),
         epln[0]['measurement'],
         ypln[0]['measurement']
         )

    # divide by 2 to convert round-trip to one-way
    plt.semilogx(yarr['msgsize'], yarr['time_us']/(2.0*yarr['nmsgs']),
                 label='yodel', marker='o', color='tab:purple')
    plt.semilogx(earr['msgsize'], earr['time_us']/(2.0*earr['nmsgs']),
                 label='echo', marker='.', color='tab:green')
    
    plt.xlabel('Message size [B]')
    plt.ylabel('One-way latency [us]')
    plt.grid(True)
    plt.legend()
    plt.title(title)
    plt.savefig(pltfile)

@cli.command('plot-cpu')
@click.argument('resfile1', type=click.File('rb'), default='-')
@click.argument('resfile2', type=click.File('rb'), default='-')
@click.argument('pltfile', type=click.Path(), default='-')
def plot_cpu(resfile1, resfile2, pltfile):
    '''
    Plot CPU usage.
    '''
    dats = [json.loads(resfile1.read()), json.loads(resfile2.read())]
    ress = [d['results'] for d in dats]
    plns = [d['plan'] for d in dats]
    arrs = [flatten_results(p, r) for p,r in zip(plns, ress)]

    nmsgs = kmgt(plns[0][0]['nmsgs'])

    title = "ZeroMQ CPU usage for %s/%s" % \
        (plns[0][0]['measurement'], plns[1][0]['measurement'])

    colors = ['tab:orange','tab:brown']
    markers = ['.','o']
    for which in [1,0]:
        arr = arrs[which]
        pln = plns[which]
        plt.semilogx(arr['msgsize'], 100.0*arr['cpu_us']/arr['time_us'],
                     label=pln[0]['measurement'], marker=markers[which], color=colors[which])
    plt.xlabel('Message size [B] (%s msgs per point)' % (nmsgs,))
    plt.ylabel('CPU [%]');
    plt.legend()
    # nperc = int(max(cpu))
    # ax3.set_yticks(numpy.arange(0, 100*(nperc + 1), 100)) 
    # ax3.tick_params(axis='y', labelcolor=color)
    plt.grid(True)
    plt.title(title)
    plt.tight_layout()  # otherwise the right y-label is slightly clippe
    plt.savefig(pltfile)


def main():
    cli()
    
