#!/bin/bash


sudo ip link set tun-v1-eth1 down
sudo tunctl -d tun-v1-eth1 