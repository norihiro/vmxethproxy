#! /bin/bash

vmxethproxy="$1"
data="$(dirname "$0")/data"
$vmxethproxy -c $data/server-dummy.json &
pid_dummy=$!
sleep 0.1

$vmxethproxy -c $data/local-ws.json &
pid_local_ws=$!
$vmxethproxy -c $data/autodiscovery.json &
pid_autodiscovery=$!
$vmxethproxy -c $data/local-secondary.json &
pid_local_secondary=$!

sleep 1
kill $pid_local_secondary $pid_autodiscovery $pid_local_ws
wait $pid_local_secondary $pid_autodiscovery $pid_local_ws
kill $pid_dummy
wait $pid_dummy
