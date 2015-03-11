#!/bin/sh

REMOTE_SSH="centos-dev"
SERVER_ADDR="10.37.129.3"
MEMSLAP="$HOME/workspace/libmemcached-1.0.18/clients/memslap"
CACHELOT="/home/rider/workspace/cachelot/bin/release/cachelot"
MEMCACHED="/usr/bin/memcached"
NUM_ITEMS=100000
NUM_WARMUP_ITERATIONS=2
NUM_ITERATIONS=10

function ssh_command() {
    ssh "$REMOTE_SSH" "$*"
    return $?
}

function ssh_bg_command() {
    echo "running \"$*\" ..."
    ssh -n -f "$REMOTE_SSH" "sh -c 'nohup $* > /dev/null 2>&1 < /dev/null &' "
}

function memslap() {
    local servers="$1"
    local the_test="$2"
    local times="$3"
    local concurrency="$4"
    # Behold! Mighty bash
    $MEMSLAP -s $(echo "$servers" | sed 's/\([^ :]\+\)[ :]\([0-9]\+\),\?/\"\1\" \"\2\" /'g) "--test=$the_test" "--execute-number=$times" "--concurrency=$concurrency" || exit 1
    # Ok seriously, memslap requires that -s host port were 3 different arguments, so I quote them
}


function stop_cachelot_and_memcached() {
    ssh_command killall "memcached" 2>/dev/null
    ssh_command killall "cachelot" 2>/dev/null
    sleep 20  # socket stays used sometimes
}

function warmup() {
    local servers="$*"
    echo "Warmup $servers ..."
    for run in $(seq 1 $NUM_WARMUP_ITERATIONS); do
        memslap "$servers" "set" "$NUM_ITEMS" "4" > /dev/null 2>&1
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
    for t in $(seq 1 $NUM_ITERATIONS); do
        memslap "$servers" "set" "$execute_times"  "$concurrency"
    done
    echo " * get $execute_times items, concurrency: $concurrency"
    for t in $(seq 1 $NUM_ITERATIONS); do
        memslap "$servers" "get" "$execute_times"  "$concurrency"
    done
}

function execute_benchmark() {
    local concurrency=$1
    echo "*** CONCURRENCY: $concurrency ******************************"
    echo "+++ memcached"
    ssh_bg_command $MEMCACHED -p 11211
    run_test_for "$SERVER_ADDR 11211" "$concurrency"
    stop_cachelot_and_memcached
    echo "+++ cachelot"
    ssh_bg_command $CACHELOT -p 11211
    run_test_for "$SERVER_ADDR 11211" "$concurrency"
    stop_cachelot_and_memcached
    echo "+++ memcached cluster"
    ssh_bg_command $MEMCACHED -p 11211
    ssh_bg_command $MEMCACHED -p 11212
    run_test_for "$SERVER_ADDR 11211,$SERVER_ADDR 11212" "$concurrency"
    stop_cachelot_and_memcached
    echo "+++ cachelot cluster"
    ssh_bg_command $CACHELOT -p 11211
    ssh_bg_command $CACHELOT -p 11212
    run_test_for "$SERVER_ADDR 11211,$SERVER_ADDR 11212" "$concurrency"
    stop_cachelot_and_memcached
}

trap "stop_cachelot_and_memcached; exit" SIGHUP SIGINT SIGTERM

# concurrency: 1, 2, 4, 6, ... 
for concurrency in $(echo "1" && seq 2 2 32); do
    execute_benchmark $concurrency
done
    
