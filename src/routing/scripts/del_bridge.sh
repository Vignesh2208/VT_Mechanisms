#!/bin/bash

#sudo ip link set v1-eth1 nomaster
#sudo ip link set tun-v1-eth1 nomaster

#sudo ip link delete tun-v1-bridge type bridge

sudo ip link set down dev tun-v1-bridge
sudo brctl delbr tun-v1-bridge