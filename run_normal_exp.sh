#!/bin/bash
pushd src/mininet
for exp_id in 1 #2 3 4 5
do

	echo "Running EXP: " $exp_id
	sudo python experiment.py --mode normal --manifest topo_VT.json --exp_no $exp_id
	sleep 5
	sudo mn -c
	sudo killall python
done
popd