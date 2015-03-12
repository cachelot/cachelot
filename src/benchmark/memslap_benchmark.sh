#!/bin/sh

REMOTE_SSH="servantus"
SERVER_ADDR="192.168.0.114"
MEMSLAP="$HOME/workspace/libmemcached-1.0.18/clients/memslap"
CACHELOT="/home/rider/workspace/cachelot/bin/release/cachelot"
MEMCACHED="/usr/bin/memcached"
NUM_ITEMS=100000
NUM_WARMUP_ITERATIONS=2
NUM_REPEATS=16

#TODO: Re-order tests, in a way to minimize cachelot / memcached restarts
#      (Huge time economy on warmups)

function ssh_command() {
    ssh "$REMOTE_SSH" "$*"
    return $?
}

function ssh_bg_command() {
    local remote_command=$(echo "nohup $* > /dev/null 2>&1 < /dev/null & ; sleep .3 ; ps -p \$! > /dev/null || exit 1")
    echo "running \"$remote_command\" ..."
    ssh "$REMOTE_SSH" "$remote_command" || exit 1
    sleep 2
}

function memslap() {
    local servers="$1"
    local the_test="$2"
    local times="$3"
    local concurrency="$4"
    # Unwrap quoted arguments
    local memslap_cmd=$(echo "$MEMSLAP $servers --test=$the_test --execute-number=$times --concurrency=$concurrency")
    echo "running $memslap_cmd"
    eval $memslap_cmd 2>&1 | tee /dev/tty | grep --color "CONNECTION FAILURE" && exit 1
}


function stop_cachelot_and_memcached() {
    ssh_command killall "memcached" 2>/dev/null
    ssh_command killall "cachelot" 2>/dev/null
    sleep 2
}

function warmup() {
    local servers="$*"
    echo "Warmup $servers ..."
    for run in $(seq 1 $NUM_WARMUP_ITERATIONS); do
        memslap "$servers" "set" "$NUM_ITEMS" "16" > /dev/null 2>&1
    done
    echo "---"
}


function run_test_for() {
    local servers="$1"
    local concurrency="$2"
    local execute_times=$NUM_ITEMS
    # memory consumption is too much
    [ $concurrency -gt 16 ] && [ $execute_times -gt 10000 ] && let "execute_times = 10000"
    warmup "$servers"
    echo " * set $execute_times items, concurrency: $concurrency"
    for t in $(seq 1 $NUM_REPEATS); do
        memslap "$servers" "set" "$execute_times"  "$concurrency"
    done
    echo " * get $execute_times items, concurrency: $concurrency"
    for t in $(seq 1 $NUM_REPEATS); do
        memslap "$servers" "get" "$execute_times"  "$concurrency"
    done
}

function execute_benchmark() {
    local concurrency=$1
    echo "*** CONCURRENCY: $concurrency ******************************"
    stop_cachelot_and_memcached
    echo "+++ cachelot"
    ssh_bg_command $CACHELOT -p 11212
    run_test_for "-s $SERVER_ADDR:11212" "$concurrency"
    stop_cachelot_and_memcached
    echo "+++ memcached"
    ssh_bg_command $MEMCACHED -p 11211
    run_test_for "-s $SERVER_ADDR:11211" "$concurrency"
    stop_cachelot_and_memcached
    echo "+++ memcached cluster"
    ssh_bg_command $MEMCACHED -p 11213 -t 1
    ssh_bg_command $MEMCACHED -p 11214 -t 1
    run_test_for "-s $SERVER_ADDR:11213 -s $SERVER_ADDR:11214" "$concurrency"
    stop_cachelot_and_memcached
    echo "+++ cachelot cluster"
    ssh_bg_command $CACHELOT -p 11215
    ssh_bg_command $CACHELOT -p 11216
    run_test_for "-s $SERVER_ADDR:11215 -s $SERVER_ADDR:11216" "$concurrency"
    stop_cachelot_and_memcached
    echo "+++ memcached single thread"
    ssh_bg_command $MEMCACHED -p 11217 -t 1
    run_test_for "-s $SERVER_ADDR:11217" "$concurrency"
    stop_cachelot_and_memcached
}

trap "stop_cachelot_and_memcached; exit" SIGHUP SIGINT SIGTERM

# concurrency: 1, 2, 4, 6, ...
for concurrency in $(echo "1" && seq 2 2 32); do
    execute_benchmark $concurrency
done

