#!/bin/bash
echo "Starting daemons..."
java -jar lib/daemon.jar -Listen 50100 -Connections $1 &> dlog/d1.log &
java -jar lib/daemon.jar -Listen 50200 -Connections $1 &> dlog/d2.log &
java -jar lib/daemon.jar -Listen 50300 -Connections $1 &> dlog/d3.log &
