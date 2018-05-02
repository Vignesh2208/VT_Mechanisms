#ifndef __UTILS_H
#define __UTILS_H
#include "includes.h"


int tun_alloc(char *dev, int flags);
int cread(int fd, char *buf, int n);
int cwrite(int fd, char *buf, int n);
int read_n(int fd, char *buf, int n);
void do_debug(char *msg, ...);
void my_err(char *msg, ...) ;
void flush_buffer(char * buf, int size);
int intf_name_to_sock_idx(char * intf, char * intf_names, int n_intfs);
int get_dst_sock_idx(char * pkt, int n_intfs, char * intf_names[N_MAX_TAPS]);
int send_packet(char * pkt,int size, int sock, char * intf_name);
void interface_thread(void * args);
void process_thread(void * args);


#endif