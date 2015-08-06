#!/bin/sh

REMOTE_SSH="192.168.14.2"
SERVER_ADDR="192.168.14.2"
#MEMSLAP="${HOME}/workspace/libmemcached-1.0.18/clients/memslap"
MEMSLAP="/usr/bin/memslap"
CACHELOT="/home/rider/cachelot/bin/cachelotd"
MEMCACHED="/usr/bin/memcached"
NUM_ITEMS=100000
NUM_WARMUP_ITERATIONS=2
NUM_REPEATS=16
MAX_CONCURRENCY=16


function ssh_command() {
    ssh "${REMOTE_SSH}" "$*"
    return $?
}


function ssh_bg_command() {
    local remote_command=$(echo "nohup $* > /dev/null 2>&1 < /dev/null &")
    echo "running \"${remote_command}\" ..."
    ssh "${REMOTE_SSH}" "${remote_command}"
}


function memslap() {
    local servers="$1"
    local the_test="$2"
    local times="$3"
    local concurrency="$4"
    # Unwrap quoted arguments
    local memslap_cmd=$(echo "${MEMSLAP} ${servers} --test=${the_test} --execute-number=${times} --concurrency=${concurrency}")
    echo "running \"${memslap_cmd}\""
    local memslap_output=$(eval "${memslap_cmd}" 2>&1)
    retcode=$?
    echo "${memslap_output}"
    [[ ${retcode} != 0 ]] && { echo "memslap returned non-zero exit code"; exit 1; }
    echo "${memslap_output}" | grep -q "CONNECTION FAILURE" && exit 1
    echo "${memslap_output}" | grep -q "Failure on read" && exit 1
}


function stop_cachelot_and_memcached() {
    ssh_command killall $(basename ${CACHELOT}) 2>/dev/null
    ssh_command killall $(basename ${MEMCACHED}) 2>/dev/null
    sleep 3
}


function warmup() {
    local servers="$*"
    echo "Warmup ${servers} ..."
    for run in $(seq 1 ${NUM_WARMUP_ITERATIONS}); do
        memslap "${servers}" "set" "${NUM_ITEMS}" "16" > /dev/null 2>&1
    done
    echo "---"
}


function run_test_for() {
    local servers="$1"
    sleep 10  # Give server some time to init TCP/UDP
    warmup "${servers}"
    for concurrency in $(seq 2 2 ${MAX_CONCURRENCY}); do
        # memory consumption is too high
        [ ${concurrency} -gt 16 ] && [ ${NUM_ITEMS} -gt 10000 ] && let "NUM_ITEMS = 10000"
        echo " *** BEGIN: set ${NUM_ITEMS} items, concurrency: ${concurrency}"
        for t in $(seq 1 ${NUM_REPEATS}); do
            memslap "${servers}" "set" "${NUM_ITEMS}"  "${concurrency}"
        done
        echo " *** END: set ${NUM_ITEMS} items, concurrency: ${concurrency}"
        echo ""
        echo " *** BEGIN: get ${NUM_ITEMS} items, concurrency: ${concurrency}"
        for t in $(seq 1 ${NUM_REPEATS}); do
            memslap "${servers}" "get" "${NUM_ITEMS}"  "${concurrency}"
        done
        echo " *** END: get ${NUM_ITEMS} items, concurrency: ${concurrency}"
        echo ""
    done
}


function print_title() {
    local title=$1
    echo "********************************************************"
    echo "+++ ${title}"
    echo "********************************************************"
}

## Main ----------------------------------------------------------
trap "stop_cachelot_and_memcached" SIGHUP SIGINT SIGTERM
stop_cachelot_and_memcached

print_title "cachelot"
ssh_bg_command ${CACHELOT} -p 11212
run_test_for "-s ${SERVER_ADDR}:11212"
stop_cachelot_and_memcached
sleep 60

print_title "memcached"
ssh_bg_command ${MEMCACHED} -p 11211
run_test_for "-s ${SERVER_ADDR}:11211"
stop_cachelot_and_memcached
sleep 60

#print_title "cachelot cluster"
#ssh_bg_command ${CACHELOT} -p 11215
#ssh_bg_command ${CACHELOT} -p 11216
#run_test_for "-s ${SERVER_ADDR}:11215 -s ${SERVER_ADDR}:11216"
#stop_cachelot_and_memcached

#print_title "memcached cluster"
#ssh_bg_command ${MEMCACHED} -p 11213 -t 1
#ssh_bg_command ${MEMCACHED} -p 11214 -t 1
#run_test_for "-s ${SERVER_ADDR}:11213 -s ${SERVER_ADDR}:11214"
#stop_cachelot_and_memcached
#sleep 60

