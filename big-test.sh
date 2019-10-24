#!/bin/bash

# This performs network configuration and network performance tests.
#
# It has three hosts involved: a "launch" host and two tests hosts
# refered to as "src" and "dst".
#
# This script runs from launch and writes to the current working
# directory.  The launching account must have ability to run "zperf"
# and to log in via SSH to "src" and "dst" as a normal user which may
# run "zperfcli" and as the root user which may run NIC and network
# configuration commands.  The "launch" host may be one of the test
# hosts.
#
# The "src" node is the "sender" of test messages and the "dst" is the
# "receiver".  In the case of latency tests there is a two-way
# communication and "src" runs the ECHO "server" and "dst" runs the
# YODEL "client".  Otherwise, either end may bind or connect.
#

# 2x32x2.1GHz
src_nic=enp24s0f1
src_addr=10.0.1.117
src_ssh=$src_addr

# 1x16x3.0GHz
dst_nic=enp179s0f1
dst_addr=10.0.1.115
dst_ssh=$dst_addr

do_init () {
    ssh root@${dst_ssh} mst start || exit 1
    ssh root@${dst_ssh} mlxlink -d net-${dst_nic} || exit 1
    ssh root@${src_ssh} mst start || exit 1
    ssh root@${src_ssh} mlxlink -d net-${src_nic} || exit 1
}

do_set_mtu () {
    mtu=${1:-1500}; shift
    tgt=$(echo "$mtu - 28" | bc)

    echo $src_ssh
    ssh root@${src_ssh} ip link set dev ${src_nic} up mtu $mtu || exit 1
    ssh root@${src_ssh} ip link show dev ${src_nic} | head -1
    echo $dst_ssh
    ssh root@${dst_ssh} ip link set dev ${dst_nic} up mtu $mtu || exit 1
    ssh root@${dst_ssh} ip link show dev ${dst_nic} | head -1

    ssh root@${src_ssh} ping -c1 -M do -s $tgt $dst_addr > /dev/null || exit 1
}

do_one_plan () {
    planfile="$1" ; shift
    if [ -f $planfile ] ; then
        echo "planfile exists: $planfile"
    else
        echo zperf plan -o "$planfile" $@
        zperf plan -o "$planfile" $@
    fi
}

do_plans () {

    # by default the "src" binds
    args="--address ${src_addr} -p 5200"
    # or switch so "dst" binds
    # args="--address ${dst_addr} --reverse -p 5200"

    for thr in zthr cthr
    do
        targs="$args --min-nmsgs=1000 --max-nmsgs=10000000 --total-data 10G"
        do_one_plan zperf-plan-${thr}-1-1.json -t1 -j1 $targs -m ${thr} {1..30} 
        do_one_plan zperf-plan-${thr}-10-100.json -t10 -j100 $targs -m ${thr} {1..30}
    done
    for lat in zlat clat
    do
        largs="$args --min-nmsgs=1000 --max-nmsgs=10000 --total-data 1G"
        do_one_plan zperf-plan-${lat}.json -t1 -j1 $largs -m ${lat} {1..20}
    done
}

do_run_one () {
    mtu=$1 ; shift
    planfile=$1 ; shift
    name=$(basename $planfile .json | sed -e 's/^.*plan-//')
    resfile="zperf-results-${name}-mtu${mtu}.json"
    if [ -f $resfile ] ; then
        echo "skipping existing result file, to rerun do"
        echo "rm $resfile"
    else
        echo zperf run -s $src_ssh -d $dst_ssh -o $resfile $planfile
        zperf run -s $src_ssh -d $dst_ssh -o $resfile $planfile
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
            if [ ! -f $resfile ] ; then
                echo "no such file: $resfile"
                continue;
            fi
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
            if [ ! -f $resfile ] ; then
                echo "no such file: $resfile"
                continue;
            fi
            name=$(basename $resfile .json)
            zperf plot-lat -m $send $resfile "${name}-plot-lat-${send}.${ext}"
            zperf plot-cpu -m $send $resfile "${name}-plot-cpu-${send}.${ext}"
            zperf plot-lat -m $recv $resfile "${name}-plot-lat-${recv}.${ext}"
            zperf plot-cpu -m $recv $resfile "${name}-plot-cpu-${recv}.${ext}"
        done
    done
}

do_plot_one () {
    plot=$1 ; shift
    name=$1 ; shift
    plotfile="note-${plot}-${name}.pdf"
    plotcmd="plot-${plot}"
    if [ -f "$plotfile" ] ; then
        echo "already have:  $plotfile"
        return
    fi
    echo zperf $plotcmd $@ $plotfile
    zperf $plotcmd $@ $plotfile
}

do_note_plots () {

    do_plot_one czlat mtu9000 zperf-results-{c,z}lat-mtu9000.json

    return

    for side in recv send
    do

        for thcn in 1-1 10-100
        do
            do_plot_one zthr-mtu ${thcn}-${side} \
                        --side $side \
                        zperf-plan-zthr-${thcn}-results-mtu{1500,9000}.json                  
        done

        for mtu in 1500 9000
        do
            do_plot_one zthr-sm mtu${mtu}-${side} \
                        --side $side \
                        zperf-plan-zthr-{1-1,10-100}-results-mtu${mtu}.json                  
        done
    done
    for thcn in 1-1 10-100
    do
        for mtu in 1500 9000
        do
            for range in lo hi
            do
                if [ "$range" == "lo" ] ; then
                    msgsizes='1 2e3'
                else
                    msgsizes='2e3 2e9'
                fi
                
                do_plot_one thr-cz ${thcn}-mtu${mtu}-thr-recv-${range} \
                            --msgsizes $msgsizes --scale semilogx \
                            zperf-plan-{c,z}thr-${thcn}-results-mtu${mtu}.json
                      

                do_plot_one thr-cpu-cz ${thcn}-mtu${mtu}-thr-recv-cpu-${range} \
                            --msgsizes $msgsizes \
                            zperf-plan-{c,z}thr-${thcn}-results-mtu${mtu}.json
            done
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
