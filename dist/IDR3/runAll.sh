#!/bin/bash
echo "Starting privacy peers..."
cd privacypeer01
./runPrivacyPeer01.sh &> ../logs/privpeer1 &

cd ../privacypeer02
./runPrivacyPeer02.sh &> ../logs/privpeer2 &

cd ../privacypeer03
./runPrivacyPeer03.sh &> ../logs/privpeer3 &

echo "Wait 2s until peers are started..."
sleep 2

cd ../peer01
./runPeer01.sh &> ../logs/peer &

