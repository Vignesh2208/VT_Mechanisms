#!/bin/bash
pushd src/mininet
for exp_id in 1 #2 3 4 5
do
for tdf in 1 #2 5
do
	echo "Running EXP: " $exp_id
	echo "TDF: " $tdf
	sudo python experiment.py --mode VT --manifest topo_VT.json --nrounds 100000 --exp_no $exp_id --tdf $tdf

	sleep 5
	sudo mn -c
	sudo killall python
done
done
popd
