#!/bin/bash

# This performs network configuration and network performance tests.
#
# It must run in an account which upon login may:
#
# 1. ssh to root@ both test hosts w/out password
#
# 2. ssh to test user@ both test hosts w/out password
#
# 3. upon login to user@ the zperf and zperfcli commands are available
#

local_nic=enp24s0f1
local_addr=10.0.1.117
remote_nic=enp179s0f1
remote_addr=10.0.1.115

do_init () {
    ssh root@${remote_addr} mst start || exit 1
    ssh root@${remote_addr} mlxlink -d net-${remote_nic} || exit 1
    ssh root@${local_addr} mst start || exit 1
    ssh root@${local_addr} mlxlink -d net-${local_nic} || exit 1
}

do_set_mtu () {
    mtu=${1:-1500}; shift
    tgt=$(echo "$mtu - 28" | bc)

    ssh root@${remote_addr} ip link set dev ${remote_nic} up mtu $mtu || exit 1
    ssh root@${local_addr} ip link set dev ${local_nic} up mtu $mtu || exit 1
    ping -c1 -M do -s $tgt $remote_addr > /dev/null || exit 1
    ssh root@${remote_addr} ip link show dev ${remote_nic} | head -1
    ssh root@${local_addr} ip link show dev ${local_nic} | head -1
    
}

do_plans () {
    args="-a ${remote_addr} -p 5200"

    for thr in zthr cthr
    do
        targs="$args --min-nmsgs=100 --max-nmsgs=1000000 --total-data 10G"
        zperf plan -o zperf-plan-${thr}-1-1.json -t1 -j1 $targs -m ${thr} {1..30} 
        zperf plan -o zperf-plan-${thr}-10-100.json -t10 -j100 $targs -m ${thr} {1..30}
    done
    for lat in clat
    do
        largs="$args --min-nmsgs=1000 --max-nmsgs=1000000 --total-data 1G"
        zperf plan -o zperf-plan-${lat}.json -t1 -j1 $largs -m ${lat} {1..20}
    done
}

do_run_one () {
    mtu=$1 ; shift
    planfile=$1 ; shift
    name=$(basename $planfile .json)
    resfile="${name}-results-mtu${mtu}.json"
    if [ -f $resfile ] ; then
        echo "skipping existing result file, to rerun do"
        echo "rm $resfile"
    else
        zperf run -o $resfile $planfile
    fi
}

do_run () {
    mtu=$1
    do_set_mtu $mtu
    for planfile in zperf-plan-*.json
    do
        do_run_one $mtu $planfile
    done
}

do_plots () {
    mtu=$1 ; shift
    ext="${1:-svg}"; shift

    for thr in zthr cthr
    do
        if [ "$thr" = "zthr" ] ; then
            recv="RTHR"
            send="STHR"
        else
            recv="RECV"
            send="SEND"
        fi
        for resfile in zperf-plan-${thr}-*-results-mtu${mtu}.json
        do
            name=$(basename $resfile .json)
            
            zperf plot-thr -m $send $resfile "${name}-plot-thr-${send}.${ext}"
            zperf plot-cpu -m $send $resfile "${name}-plot-cpu-${send}.${ext}"
            zperf plot-thr -m $recv $resfile "${name}-plot-thr-${recv}.${ext}"
            zperf plot-cpu -m $recv $resfile "${name}-plot-cpu-${recv}.${ext}"
        done
    done

    for lat in clat
    do
        if [ "$lat" = "clat" ] ; then
            send="YODEL"
            recv="ECHO"
        fi
        for resfile in zperf-plan-${lat}-results-mtu${mtu}.json
        do
            zperf plot-lat -m $send $resfile "${name}-plot-lat-${send}.${ext}"
            zperf plot-cpu -m $send $resfile "${name}-plot-cpu-${send}.${ext}"
            zperf plot-lat -m $recv $resfile "${name}-plot-lat-${recv}.${ext}"
            zperf plot-cpu -m $recv $resfile "${name}-plot-cpu-${recv}.${ext}"
        done
    done
}

do_everything () {
    do_plans
    do_run 1500
    do_run 9000
    do_plots 1500
    do_plots 9000
}



cmd=$1 ; shift
do_${cmd} $@
