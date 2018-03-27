#ifndef __PKT_INFO_H
#define __PKT_INFO_H

#include<stdio.h> //For standard things
#include<stdlib.h>    //malloc
#include<string.h>    //memset
#include<netinet/ip_icmp.h>   //Provides declarations for icmp header
#include<netinet/udp.h>   //Provides declarations for udp header
#include<netinet/tcp.h>   //Provides declarations for tcp header
#include<netinet/ip.h>    //Provides declarations for ip header
#include<sys/socket.h>
#include<arpa/inet.h>
#include<net/ethernet.h>
 
void ProcessPacket( char* , int);
void print_ip_header( char* , int);
void print_tcp_packet( char* , int);
void print_udp_packet( char * , int);
void print_icmp_packet( char* , int);
void PrintData ( char* , int);

#endif