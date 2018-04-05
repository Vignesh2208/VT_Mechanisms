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

from mininet.net import Mininet
from mininet.node import Switch, Host
from mininet.log import setLogLevel, info, error, debug
from mininet.moduledeps import pathCheck
from sys import exit
from time import sleep
import os
import tempfile
import socket

import os
script_dir =  os.path.dirname(os.path.realpath(__file__))
#vt_mech_dir = script_dir + "/../../"

from os.path import dirname, abspath
vt_mech_dir = dirname(dirname(dirname(abspath(__file__)))) + "/"

print "VTMech Dir: ", vt_mech_dir

class VTHost(Host):
    def config(self, **params):
        r = super(VTHost, self).config(**params)

        for off in ["rx", "tx", "sg"]:
            cmd = "/sbin/ethtool --offload %s %s off" % (self.defaultIntf().name, off)
            self.cmd(cmd)

        # disable IPv6
        self.cmd("sysctl -w net.ipv6.conf.all.disable_ipv6=1")
        self.cmd("sysctl -w net.ipv6.conf.default.disable_ipv6=1")
        self.cmd("sysctl -w net.ipv6.conf.lo.disable_ipv6=1")

        return r

    def describe(self, sw_addr=None, sw_mac=None):
        pass
        #print "**********"
        #print "Network configuration for: %s" % self.name
        #print "Default interface: %s\t%s\t%s" %(
        #    self.defaultIntf().name,
        #    self.defaultIntf().IP(),
        #    self.defaultIntf().MAC()
        #)
        #if sw_addr is not None or sw_mac is not None:
            #print "Default route to switch: %s (%s)" % (sw_addr, sw_mac)
        #print "**********"

class VTSwitch(Switch):
    """VT virtual switch"""
   

    def __init__(self, name, 
                 operating_mode = None, 
                 log_file = None,
                 device_id = 0,
                 **kwargs):
        Switch.__init__(self, name, **kwargs)
        
        if log_file == None :
            self.log_file = '/tmp/' + str(name) + ".log"
        else:
            self.log_file = log_file

        if operating_mode != "INS_VT" :

            if operating_mode == "APP_VT" :
                self.exec_path = vt_mech_dir + "src/routing/router_app_vt "
            else:
                self.exec_path = vt_mech_dir + "src/routing/router_n "
            self.router_exec_path = self.exec_path
        else:
            self.exec_path = "/usr/bin/tracer"
            self.router_exec_path = vt_mech_dir + "src/routing/router_n "

        self.operating_mode = operating_mode

        self.device_id = device_id
        

    @classmethod
    def setup(cls):
        pass


    def start(self, controllers):
    
        #print "Starting VT switch {}.\n".format(self.name)

        cmd_to_run = self.exec_path
        n_intfs = 0
        for port, intf in self.intfs.items():
            if not intf.IP():
                n_intfs += 1
        if self.operating_mode == "INS_VT" :

            cmd_file_path  = "/tmp/cmds_" + str(self.name) + ".txt"
            routes_file_path = vt_mech_dir + "src/routing/routes/" + str(self.name) + "_routes.txt"
            with open(cmd_file_path,"w") as f :
                f.write(self.router_exec_path + "-i " + str(self.name) + " -n " + str(n_intfs) +" -f " + routes_file_path + "\n")
            n_round_insns = 100000
            rel_cpu_speed = 1.0
            create_spinner = 0
            cmd_to_run = "sudo " + cmd_to_run + " " + str(self.device_id) + " " + str(cmd_file_path) + " " + str(rel_cpu_speed) + " " + str(n_round_insns) + " " + str(create_spinner)

        else:
            routes_file_path = vt_mech_dir + "src/routing/routes/" + str(self.name) + "_routes.txt"
            cmd_to_run = cmd_to_run + "-i " + str(self.name) + " -n " + str(n_intfs) +" -f " + routes_file_path

        
        self.cmd_pid = None
        with tempfile.NamedTemporaryFile() as f:

            self.cmd("sysctl -w net.ipv6.conf.all.disable_ipv6=1")
            self.cmd("sysctl -w net.ipv6.conf.default.disable_ipv6=1")
            self.cmd("sysctl -w net.ipv6.conf.lo.disable_ipv6=1")
            
            self.cmd(cmd_to_run + ' >' + self.log_file + ' 2>&1 & echo $! >> ' + f.name)
            self.cmd_pid = int(f.read())
        #print "P4 switch {} PID is {}.\n".format(self.name, self.cmd_pid)
        sleep(1)
        
        #print "P4 switch {} has been started.\n".format(self.name)

    def stop(self):
        "Terminate P4 switch."
        self.cmd('kill %' + str(self.cmd_pid))
        #self.cmd('wait')
        self.deleteIntfs()

    def attach(self, intf):
        "Connect a data port"
        assert(0)

    def detach(self, intf):
        "Disconnect a data port"
        assert(0)
