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

sleep 0.1

{
	echo 'RQ1 05000000 10'
	sleep 0.2
	echo 'RQ1 05000000 10'
} | "$(dirname "$0")/ws.py" &
"$(dirname "$0")/ws.py" <<< 'DT1 05000000 20 20 20 20 20 20'

kill $pid_local_secondary $pid_autodiscovery $pid_local_ws
wait $pid_local_secondary $pid_autodiscovery $pid_local_ws
sleep 1.1
kill $pid_dummy
wait $pid_dummy
