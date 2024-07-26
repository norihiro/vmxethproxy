#! /bin/bash

vmxethproxy="$(readlink -f "$1")"
data="$(dirname "$0")/data"

timeout 4 \
$vmxethproxy -c $data/server-dummy-autoport.json &
pid_dummy=$!

timeout 2 \
$vmxethproxy -c $data/autodiscovery.json &
pid_autodiscovery=$!

sleep 1
kill $pid_autodiscovery
sleep 2
kill $pid_dummy
wait $pid_autodiscovery $pid_dummy
