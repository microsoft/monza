#!/bin/bash

CNET_BENCH=/home/t-hlefeuvre/cnet-benchmark
CNET_HOST=/home/t-hlefeuvre/cnet-host
CNET_IMG=/home/t-hlefeuvre/qemu-io-udp-cnet-benchmark.img

usage()
{
    echo "Usage: $0 <cpu0> [<cpu1> ...]"
    exit 1
}

if [ "$#" -eq 0 ]; then
    usage
fi

argvsize=${#BASH_ARGV[@]}

cleanup()
{
    pkill -9 qemu &> /dev/null
    pkill -SIGINT $CNET_HOST &> /dev/null
    pkill -9 $CNET_BENCH &> /dev/null
    pkill -9 cnet-benchmark &> /dev/null
    pkill -9 cnet-host &> /dev/null

    killall -9 qemu &> /dev/null
    killall -SIGINT $CNET_HOST &> /dev/null
    killall -9 $CNET_BENCH &> /dev/null
    killall -9 cnet-benchmark &> /dev/null
    killall -9 cnet-host &> /dev/null

    pkill -P $$ &> /dev/null

    rm -rf /dev/shm/cnet_*
}

run()
{
    cleanup

    $CNET_HOST -k $CNET_IMG &> /dev/null &
    sleep 4

    local n_index=$(( $argvsize - 1 ))
    for i in $(seq ${n_index} -1 0)
    do
        core=${BASH_ARGV[$i]}

        case $core in
            ''|*[!0-9]*) usage ;;
        esac

        if [ "$i" -eq 0 ]; then
            sleep 3
            taskset -c $core $CNET_BENCH -l 20 -s $1 -b $2 -m $i
        else
            taskset -c $core $CNET_BENCH -l 20 -s $1 -b $2 -m $i &> /dev/null &
        fi

        sleep 0.5
    done
}

for batchsize in 1 8 32 48 64; do
  for buffersize in 10 100 1000 5000 10000 30000 60000; do
    run $buffersize $batchsize
  done
done

cleanup
