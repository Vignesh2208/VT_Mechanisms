#!/bin/bash

#sudo ip link add name tun-v1-bridge type bridge

#sudo ip link set v1-eth1 master tun-v1-bridge
#sudo ip link set tun-v1-eth1 master tun-v1-bridge


#sudo ip link set tun-v1-bridge up

sudo brctl addbr tun-v1-bridge
sudo brctl addif tun-v1-bridge v1-eth1
sudo brctl addif tun-v1-bridge tun-v1-eth1

sudo ip link set up dev tun-v1-bridge

sudo ifconfig v1-eth1 10.0.0.1/24