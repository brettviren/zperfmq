#!/usr/bin/env python3

import json
import click

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

@cli.command("lat-plan")
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
    if not msgsizes:
        raise ValueError("must supply at least one message size")
    msgsizes = map(float, msgsizes)

    smeth = lambda x: int(x)
    if size_units == "log2":
        smeth = lambda x: int(2**x)
    if size_units == "log10":
        smeth = lambda x: int(10**x)

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
        msgsize = smeth(msgsize)
        eplan.append(dict(edef, msgsize=msgsize))
        yplan.append(dict(ydef, msgsize=msgsize))

    echo.write(json.dumps(eplan, indent=4).encode())
    yodel.write(json.dumps(yplan, indent=4).encode())

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
        perf = Zperf(getattr(zmq, plan['socket'].upper()))
        if plan.get('connect'):
            url = plan.get('connect').encode()
            for iconn in range(plan.get('nconnects') or 1):
                perf.connect(url)
        else:
            perf.bind(plan.get('bind').encode())
        return perf

    last_plan = None
    def zperf_differs(plan):
        if last_plan is None:
            return True
        for key in ['connect','bind','nconnects','socket']:
            if last_plan.get(key) != plan.get(key):
                return True
        return False

    perf = None
    def check_plan(plan):
        if zperf_differs(plan):
            print("new zperf")
            nonlocal perf
            perf = make_perf(plan)
        nonlocal last_plan
        last_plan = plan

    ret = list()
    for plan in plans:
        check_plan(plan)
        assert(perf)

        nmsgs, msgsize = plan['nmsgs'], plan['msgsize']
        meth = dict(echo  = lambda : perf.echo(nmsgs),
                    yodel = lambda : perf.yodel(nmsgs, msgsize),
                    send  = lambda : perf.send(nmsgs, msgsize),
                    recv  = lambda : perf.recv(nmsgs))[plan['measurement']]
        res = dict(time_us = meth())
        res["noos"] = perf.noos();
        res["nbytes"] = perf.bytes();
        res["cpu_us"] = perf.cpu();
        ret.append(res)
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
@click.argument('outfile', type=click.File('wb'), default='-')
def run(planfile, outfile):
    '''
    Execute a measurement plan.
    '''
    plan = json.loads(planfile.read())
    res = run_plan(plan)
    ret = dict(results=res, plan=plan, sysinfo=get_sysinfo())
    outfile.write(json.dumps(ret, indent=4).encode())


def main():
    cli()
    
