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
import timekeeper_functions
from timekeeper_functions import *
import old_tk_functions
from old_tk_functions import *
import math

import json


script_dir = os.path.dirname(os.path.realpath(__file__))
vt_mech_dir = script_dir + "/../../"
experiments_dir = vt_mech_dir + "experiments/"




parser = argparse.ArgumentParser(description='WSC 2018 Experiment')
parser.add_argument('--cli', help="start the mininet cli", action="store_true")
parser.add_argument('--manifest', help='Path to JSON config file',
                    type=str, action="store", required=True)
parser.add_argument('--mode', help='Operating Mode', dest='mode',
                    type=str, action="store", required=False, default="normal")
parser.add_argument('--tdf', help='tdf', dest='tdf',
                    type=int, action="store", required=False, default=1)
parser.add_argument('--nrounds', help='nrounds', dest='nrounds',
                    type=int, action="store", required=False, default=100)
parser.add_argument('--exp_no', help='exp_no', dest='exp_no',
                    type=int, action="store", required=False, default=1)


args = parser.parse_args()

operating_mode = args.mode
tdf = args.tdf
device_id = 0
n_rounds = args.nrounds
exp_no = args.exp_no

ENABLE_TIMEKEEPER = 0
TIMESLICE = 100000 

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
    params = conf['parameters'] if 'parameters' in conf else {}
    AppTopo = apptopo.AppTopo

    n_switches = int(conf['n_switches'])
    n_hosts = int(conf['n_hosts'])


    if operating_mode == "APP_VT" or operating_mode == "INS_VT" :
        ret = initializeExp(5);
        if ret < 0 :
            print "Initialize Exp Failed!"
            sys.exit(-1)
    
    def formatParams(s):
        for param in params:
            s = re.sub('\$'+param+'(\W|$)', str(params[param]) + r'\1', s)
            s = s.replace('${'+param+'}', str(params[param]))
        return s

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
        #print "Host: ", h.name, "Running Command: ", cmd_to_run


        h.cmd(cmd_to_run)        
        pid = get_pids_with_cmd(host['cmd'][0:150], expected_no_of_pids=1)[0]
        host_procs.append((pid, h))

    for i in xrange(1,n_switches + 1) :
        switch_name = "s" + str(i)
        sw = net.get(switch_name)

        sw_pid = sw.cmd_pid
        switch_procs.append((sw_pid,sw))

    print "HOST Processes :"
    #print host_procs
    for host_pid, h in host_procs:
        print host_pid

    print "SWITCH Processes :" 
    #print switch_procs
    for sw_pid, sw in switch_procs:
        print sw_pid

    exp_stats = {}
    

    if ENABLE_TIMEKEEPER == 0 :
        print "TIMEKEEPER DISABLED. RUNNING EXPERIMENT FOR 60 secs ..."
        exp_stats["start_time"] = time.time()
        sleep(60)
        exp_stats["end_time"] = time.time()
    else:

        if operating_mode == "VT" :

            print "RUNNING in ", operating_mode, " MODE .."
            for host_pid, host in host_procs :
                o_dilate_all(host_pid, tdf)
                o_addToExp(host_pid)

            for sw_pid, sw in switch_procs :
                o_dilate_all(sw_pid, tdf)
                o_addToExp(sw_pid)

            o_set_cbe_experiment_timeslice(TIMESLICE*my_tdf)
            print "Sychronize and Freeze"
            o_synchronizeAndFreeze()
            sleep(2)
            print "Starting Experiment"
            o_startExp()

            n_rounds_per_sec = 1000000000/TIMESLICE

            print "Progress Experiment for ", n_rounds, " Rounds !"
            exp_stats["start_time"] = time.time()
            o_progress_n_rounds(n_rounds)
            exp_stats["end_time"] = time.time()

            stats = o_get_experiment_stats()

            if stats[0] < 0 :
                print "ERROR collecting stats "
            else:
                exp_stats["total_rounds"] = float(stats[2])
                exp_stats["total_round_error"] = float(stats[0])
                exp_stats["total_round_error_sq"] = float(stats[1])

                if exp_stats["total_rounds"] > 0 :
                    exp_stats["mu_round_error"] = float(stats[0])/float(exp_stats["total_rounds"])
                    exp_stats["std_round_error"] = math.sqrt(float(stats[1])/float(exp_stats["total_rounds"]) - (exp_stats["mu_round_error"]**2))
                    exp_stats["total_round_error"] = float(stats[0])
                    exp_stats["total_round_error_sq"] = float(stats[1])
                    exp_stats["total_rounds"] = float(stats[2])
                else:
                    exp_stats["mu_round_error"] = 0.0
                    exp_stats["std_round_error"] = 0.0

            stat_file = operating_mode + "_" + str(exp_no) + ".json" 
            with open(experiments_dir + stat_file, 'w') as fp:
                json.dump(exp_stats, fp, sort_keys=True, indent=4)
            
            print "Stopping Experiment"
            o_resume_exp_cbe()
            sleep(2)
            o_stopExp()
            sleep(10)
            



        elif operating_mode == "APP_VT" or operating_mode == "INS_VT":


            print "RUNNING in ", operating_mode, " MODE .."
            print "Synchronize and Freezing"
            sleep(2)

            while synchronizeAndFreeze(len(host_procs) + len(switch_procs)) <= 0 :
                print "Sync and Freeze Failed. Retrying in 1 sec"
                sleep(1)
            

            print "Synchronize and Freeze succeeded !"
            sleep(1)

            print "Setting Net dev owners for hosts:"
            for host_pid, host in host_procs :
                for name in host.intfNames():
                    if name != "lo" :
                        #print name
                        set_netdevice_owner(host_pid,name)

            sleep(1)

            print "Setting Net dev owners for switches: "
            for sw_pid, sw in switch_procs :
                for name in sw.intfNames():
                    if name != "lo" :
                        #print name
                        set_netdevice_owner(sw_pid,name)

            sleep(1)

            print "Progress Experiment for ", n_rounds, " Rounds !"
            exp_stats["start_time"] = time.time()
            progress_n_rounds(n_rounds)
            exp_stats["end_time"] = time.time()

            print "Copying files ..."
            os.system("cp /tmp/*.log " + str(experiments_dir))

            sleep(1)

            print "Getting stats ..."
            stats = get_experiment_stats()

            if stats[0] < 0 :
                print "ERROR collecting stats "
            else:
                exp_stats["total_rounds"] = float(stats[2])
                exp_stats["total_round_error"] = float(stats[0])
                exp_stats["total_round_error_sq"] = float(stats[1])

                if exp_stats["total_rounds"] > 0 :
                    exp_stats["mu_round_error"] = float(stats[0])/float(exp_stats["total_rounds"])
                    #exp_stats["std_round_error"] = math.sqrt(float(stats[1])/float(exp_stats["total_rounds"]) - (exp_stats["mu_round_error"]**2))
                    exp_stats["total_round_error"] = float(stats[0])
                    exp_stats["total_round_error_sq"] = float(stats[1])
                    exp_stats["total_rounds"] = float(stats[2])
                else:
                    exp_stats["mu_round_error"] = 0.0
                    exp_stats["std_round_error"] = 0.0



            stat_file = operating_mode + "_" +  str(exp_no) + ".json" 
            with open(experiments_dir + stat_file, 'w') as fp:
                json.dump(exp_stats, fp, sort_keys=True, indent=4)
            
            print "Stopping Experiment"
            sleep(2)
            stopExp()

        else:
            print "UNKNOWN MODE: ", operating_mode

    print "STOPPING EXPERIMENT ..."
    net.stop()


if __name__ == '__main__':
    main()
