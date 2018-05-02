#
# File  : timekeeper_functions.py
# 
# Brief : Exposes TimeKeeper API in python
#
# authors : Vignesh Babu
#


import os
from signal import SIGSTOP, SIGCONT
import time
import subprocess


TIMEKEEPER_FILE_NAME = "/proc/dilation/status"
DILATE = 'A'
DILATE_ALL = 'B'
FREEZE_OR_UNFREEZE = 'C'
FREEZE_OR_UNFREEZE_ALL = 'D'
LEAP = 'E'

ADD_TO_EXP_CBE = 'F'
ADD_TO_EXP_CS = 'G'
SYNC_AND_FREEZE = 'H'
START_EXP = 'I'
SET_INTERVAL = 'J'
PROGRESS = 'K'
FIX_TIMELINE = 'L'
RESET = 'M'
STOP_EXP = 'N'
SET_CBE_EXP_TIMESLICE = 'T'
SET_NETDEVICE_OWNER  = 'U'
PROGRESS_EXP_CBE = 'V'
RESUME_CBE = 'W'






def o_is_root() :

	if os.geteuid() == 0 :
		return 1
	print "Needs to be run as root"
	return 0


def o_is_Module_Loaded() :

	if os.path.isfile(TIMEKEEPER_FILE_NAME) == True:
		return 1
	else :
		print "Timekeeper module is not loaded"
		return 0


# Associates the given pid with a net_device
def o_set_netdevice_owner(pid,dev_name) :

	if o_is_root() == 0 or o_is_Module_Loaded() == 0 :
		print "ERROR in Set Netdevice Owner. TimeKeeper is not loaded!"
		return -1
	cmd = SET_NETDEVICE_OWNER + "," + str(pid) + "," + str(dev_name)

	return o_send_to_timekeeper(cmd)




# Converts the double into a integer, makes computation easier in kernel land
# ie .5 is converted to -2000, 2 to 2000

def o_fixDilation(dilation) :
        
	dil = 0

	if dilation < 0 :
		print "Negative dilation does not make sense";
		return -1
        
	if dilation < 1.0 and dilation > 0.0 :
		dil = (int)((1/dilation)*1000.0)
		dil = dil*-1
        
	elif dilation == 1.0 or dilation == -1.0 :
                dil = 1000.0
	else:
		dil = (int)(dilation*1000.0)
        
        return dil;



def o_send_to_timekeeper(cmd) :

	if o_is_root()== 0 or o_is_Module_Loaded() == 0 :
		print "ERROR sending cmd to timekeeper"
		return -1

	with open(TIMEKEEPER_FILE_NAME,"w") as f :
		f.write(cmd)
	return 1

# timeslice in nanosecs
def o_set_cbe_experiment_timeslice(timeslice) :


	if o_is_root() == 0 or o_is_Module_Loaded() == 0 :
		print "ERROR setting timeslice value"
		return -1
	
	timeslice = int(timeslice)
	cmd = SET_CBE_EXP_TIMESLICE + "," + str(timeslice)

	return o_send_to_timekeeper(cmd)


#
#Given a pid, add that container to a CBE experiment.
#

def o_addToExp(pid) :

	if o_is_root() == 0 or o_is_Module_Loaded() == 0 :
		return -1 
	cmd = ADD_TO_EXP_CBE + "," + str(pid)
	return o_send_to_timekeeper(cmd)



#
#Starts a CBE Experiment
#

def o_startExp():

	if o_is_root() == 0 or o_is_Module_Loaded() == 0 :
		print "ERROR starting CBE Experiment"
		return -1
	cmd = START_EXP
	return o_send_to_timekeeper(cmd)


#
#Given all Pids added to experiment, will set all their virtual times to be the same, then freeze them all (CBE and CS)
#

def o_synchronizeAndFreeze() :

	if o_is_root() == 0 or o_is_Module_Loaded() == 0 :
		print "ERROR in Synchronize and Freeze"
		return -1
	cmd = SYNC_AND_FREEZE
	return o_send_to_timekeeper(cmd)





#
#Stop a running experiment (CBE or CS) **Do not call stopExp if you are waiting for a s3fProgress to return!!**
#

def o_stopExp() :

	if o_is_root() == 0 or o_is_Module_Loaded() == 0 :
		print "ERROR in Synchronize and Freeze"
		return -1
	cmd = STOP_EXP
	return o_send_to_timekeeper(cmd)



#
#Sets the TDF of the given pid
#

def o_dilate(pid,dilation) :

	if o_is_root() == 0 or o_is_Module_Loaded() == 0 :
		print "ERROR in setting dilation for pid :", pid
		return -1
	dil = o_fixDilation(dilation)
	if dil == - 1 :
		return - 1
	elif dil < 0 :
		cmd = DILATE + "," + str(pid) + ",1," + str(-1*dil)
	else :
		cmd = DILATE + "," + str(pid) + "," + str(dil)

	return o_send_to_timekeeper(cmd)

#
# Will set the TDF of a LXC and all of its children
#

def o_dilate_all(pid,dilation) :

	if o_is_root() == 0 or o_is_Module_Loaded() == 0 :
		print "ERROR in setting dilation for pid :", pid
		return -1
	dil = o_fixDilation(dilation)
	if dil == - 1 :
		return - 1
	elif dil < 0 :
		cmd = DILATE_ALL + "," + str(pid) + ",1," + str(-1*dil)
	else :
		cmd = DILATE_ALL + "," + str(pid) + "," + str(dil)

	return o_send_to_timekeeper(cmd)



def o_progress(timeline,mypid, force) :
	if o_is_root() and o_is_Module_Loaded() and timeline >= 0:
		cmd = PROGRESS + "," + str(timeline) + "," + str(mypid) + "," + str(force)
		return o_send_to_timekeeper(cmd)
	else:
		return -1

def o_progress_exp_cbe(n_rounds) :
	if o_is_root() and o_is_Module_Loaded() and n_rounds > 0 :
		cmd = PROGRESS_EXP_CBE + "," + str(n_rounds)
		return o_send_to_timekeeper(cmd)
	else:
		return -1

def o_resume_exp_cbe() :
	if o_is_root() and o_is_Module_Loaded() :
		cmd = RESUME_CBE
		return o_send_to_timekeeper(cmd)
	else:
		return -1

