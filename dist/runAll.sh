#!/bin/bash
NODES=3
./startDaemons.sh $NODES
echo "Wait 2s until daemons are started..."
sleep 2

cd IDR1
for i in `seq 1 $NODES`
do
    echo "Node $i:"
    cd ../IDR$i
    ./runAll.sh
done
