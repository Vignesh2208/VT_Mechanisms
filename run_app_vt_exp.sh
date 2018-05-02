#!/bin/bash
pushd src/mininet
#for exp_id in 1 2 3 4 5
for exp_id in 4
do
	echo "Running EXP: " $exp_id
	sudo python experiment.py --mode APP_VT --manifest topo_APP_VT.json --nrounds 100000 --exp_no $exp_id

	sleep 5
done
popd
