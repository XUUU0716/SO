#!/bin/bash

for i in {1..5}; do
    ./monitor 10 10 &
    sleep 0.5 
    ./miner 15 4 &
    ./miner 15 4 &
    ./miner 15 4 &
    wait
    echo "=== Run $i done ==="
done
