#!/usr/bin/env python2

# Copyright 2013-present Barefoot Networks, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import signal
import os
import sys
import subprocess
import argparse
import json
import importlib
import re
from time import sleep
import time

from mininet.net import Mininet
from mininet.topo import Topo
from mininet.link import TCLink
from mininet.log import setLogLevel, info
from mininet.cli import CLI

from VT_mininet import VTSwitch, VTHost
import apptopo

parser = argparse.ArgumentParser(description='WSC 2018 Experiment')
parser.add_argument('--cli', help="start the mininet cli", action="store_true")
parser.add_argument('--manifest', help='Path to JSON config file',
                    type=str, action="store", required=True)
parser.add_argument('--mode', help='Operating Mode', dest='mode',
                    type=str, action="store", required=False, default="normal")


args = parser.parse_args()

operating_mode = args.mode
device_id = 0

ENABLE_TIMEKEEPER = 0

def run_command(command):
    return os.WEXITSTATUS(os.system(command))

def configureVTSwitch(**switch_args):
    class ConfiguredVTSwitch(VTSwitch):
        def __init__(self, *opts, **kwargs):
            global device_id
            kwargs.update(switch_args)

            device_id += 1
            log_file = "/tmp/s" + str(device_id) + ".log"

            kwargs['name'] = "s" + str(device_id)
            kwargs['operating_mode'] = operating_mode
            kwargs['device_id'] = device_id
            kwargs['log_file'] = log_file
            VTSwitch.__init__(self, **kwargs)
    return ConfiguredVTSwitch

def procStatus(pid):
    for line in open("/proc/%d/status" % pid).readlines():
        if line.startswith("State:"):
            return line.split(":",1)[1].strip().split(' ')[0]
    return None


def get_pids_with_cmd(cmd, expected_no_of_pids=1) :
    pid_list = []

    while  len(pid_list) < expected_no_of_pids :
        time.sleep(0.5)
        pid_list = []
        try:
            ps_output = subprocess.check_output("ps -e -o command:200,pid | grep '^" + cmd + "'", shell=True)
        except subprocess.CalledProcessError:
            ps_output = ""

        if len(ps_output) > 0 :
            for p in ps_output.split('\n'):
                p_tokens = p.split()
                if not p_tokens:
                    continue
                pid = int(p_tokens[len(p_tokens)-1])
                if procStatus(pid) != 'D' :
                    pid_list.append(pid)



    return pid_list


def main():

    global operating_mode

    os.system("sudo service avahi-daemon stop")

    if operating_mode == "normal" :
        operating_mode = "VT"
        ENABLE_TIMEKEEPER = 0
    else:
        ENABLE_TIMEKEEPER = 1

    with open(args.manifest, 'r') as f:
        manifest = json.load(f)

    conf = manifest['targets']["multiswitch"]
    AppTopo = apptopo.AppTopo
    

    links = [l[:2] for l in conf['links']]
    latencies = dict([(''.join(sorted(l[:2])), l[2]) for l in conf['links'] if len(l)>=3])
    bws = dict([(''.join(sorted(l[:2])), l[3]) for l in conf['links'] if len(l)>=4])

    for host_name in sorted(conf['hosts'].keys()):
        host = conf['hosts'][host_name]
        if 'latency' not in host: continue
        for a, b in links:
            if a != host_name and b != host_name: continue
            other = a if a != host_name else b
            latencies[host_name+other] = host['latency']

    for l in latencies:
        if isinstance(latencies[l], (str, unicode)):
            latencies[l] = formatParams(latencies[l])
        else:
            latencies[l] = str(latencies[l]) + "ms"


    topo = AppTopo(links, latencies, manifest=manifest, target="multiswitch", bws=bws)
    switchClass = configureVTSwitch()
    net = Mininet(topo = topo,
                  link = TCLink,
                  host = VTHost,
                  switch = switchClass,
                  controller = None,
                  autoStaticArp=True)


    net.start()
    sleep(1)

    
    for h in net.hosts:
        h.describe()

    if args.cli or ('cli' in conf and conf['cli']):
        CLI(net)

    n_switches = int(conf['n_switches'])
    n_hosts = int(conf['n_hosts'])

    stdout_files = dict()
    return_codes = []
    host_procs = []
    switch_procs = []


    def _wait_for_exit(p, host):
        print p.communicate()
        if p.returncode is None:
            p.wait()
            print p.communicate()
        return_codes.append(p.returncode)
        if host_name in stdout_files:
            stdout_files[host_name].flush()
            stdout_files[host_name].close()


    for host_name in sorted(conf['hosts'].keys()):
        host = conf['hosts'][host_name]
        if 'cmd' not in host: continue

        h = net.get(host_name)
        stdout_filename = os.path.join("/tmp/", h.name + '.log')
        stdout_files[h.name] = open(stdout_filename, 'w')
        #cmd = formatCmd(host['cmd'])
        cmd_to_run = host['cmd'] + " >> " + stdout_filename + " 2>&1" + " &"
        print "Host: ", h.name, "Running Command: ", cmd_to_run


        h.cmd(cmd_to_run)        
        pid = get_pids_with_cmd(host['cmd'][0:150], expected_no_of_pids=1)[0]
        host_procs.append((pid, host_name))

    for i in xrange(1,n_switches + 1) :
        switch_name = "s" + str(i)
        sw = net.get(switch_name)

        sw_pid = sw.cmd_pid
        switch_procs.append((sw_pid, switch_name))

    print "HOST Processes :"
    print host_procs

    print "SWITCH Processes :" 
    print switch_procs


    print "RUNNING EXPERIMENT FOR 5 secs ..."

    if ENABLE_TIMEKEEPER == 0 :
        while 1 :
            sleep(5)
    print "STOPPING EXPERIMENT ..."
    net.stop()


if __name__ == '__main__':
    main()
