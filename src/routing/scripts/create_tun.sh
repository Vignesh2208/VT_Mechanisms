#!/bin/bash

sudo tunctl -t tun-v1-eth1
sudo ip link set tun-v1-eth1 up