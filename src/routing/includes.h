#ifndef __INCLUDES_H
#define __INCLUDES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h> 
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <stdarg.h>
#include "linkedlist.h"
#include "hashmap.h"
#include <poll.h>
#include <linux/if_packet.h>
 #include <net/if.h>

/* buffer for reading from tun/tap interface, must be >= 1500 */
#define BUFSIZE 2000   
#define CLIENT 0
#define SERVER 1
#define PORT 55555

#define DEBUG
#define TAP_NAME 100
#define IF_NAME_SIZE 100
#define N_MAX_TAPS 100
#define IP_ADDR_SIZE 20
#define MAC_ADDR_SIZE 20

#define SUCCESS 1
#define FAIL -1

#define US_TO_NS 1000

#define THREAD_BLOCKED -1
#define THREAD_READY 0
#define THREAD_EXITING -2

#ifdef ROUTER_APP_VT
#define APP_VT
#endif


typedef struct routing_table_entry{
	char ip_address[IP_ADDR_SIZE];
	char output_if[IF_NAME_SIZE];

} routing_entry;

typedef struct thread_aruments {
	int thread_no;
	char bind_if[IF_NAME_SIZE];
	//char *intf_names[N_MAX_TAPS];
	char **intf_names;
	int n_intfs;
	int fds_main_to_thread[2];
	int fds_thread_to_main[2];
	int is_blocked;
	int stop;
} thread_args;

#endif