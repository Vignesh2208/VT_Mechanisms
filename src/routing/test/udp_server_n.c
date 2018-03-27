/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
       #include <net/if.h>
#include <errno.h>
#include <stdarg.h>
#include <linux/if_ether.h>

#include <linux/if_packet.h>

#include "pkt_info.h"

#define BUFSIZE 2000
#define DEBUG
#define SUCCESS 1
#define FAIL -1

int setPromisc(char *enterface, int *sock)
{
    struct ifreq ifr;
    strcpy(ifr.ifr_name, enterface);
    ifr.ifr_flags=IFF_UP|IFF_PROMISC|IFF_BROADCAST|IFF_RUNNING;
  
    if(ioctl(*sock,SIOCSIFFLAGS,&ifr)==-1)
    {
        do_debug("Set %s to promisc model failed\n", enterface); 
        pthread_exit(NULL);
    }
    printf("set '%s' to promisc successed!\n",enterface);
    return SUCCESS;
}

int rawSocket(char * bind_intf)
{
    int sock;
    struct ifreq ifr;
    int one = 1;
    const int *val = &one;
    struct sockaddr_ll sock_address;
 

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), bind_intf);

    
    sock=socket(AF_INET,SOCK_DGRAM,0);
    //sock=socket(AF_INET,SOCK_RAW,IPPROTO_UDP);
    //sock=socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL));

    if(sock<0)
    {
        do_debug("Create raw socket failed:%s\n", strerror(errno));
        pthread_exit(NULL);
    }
    
    printf("Raw socket created successfully!\n");


	/*
    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) < 0) {

        do_debug("Failed to bind socket to interface: %s\n",bind_intf);
        pthread_exit(NULL);
    }
    */

    /*
    memset(&sock_address, 0, sizeof(sock_address));
    sock_address.sll_family = PF_PACKET;
    sock_address.sll_protocol = htons(ETH_P_ALL);
    sock_address.sll_ifindex = if_nametoindex(bind_intf);
    if (bind(sock, (struct sockaddr*) &sock_address, sizeof(sock_address)) < 0) {
      perror("bind failed\n");
      close(sock);
      pthread_exit(NULL);
    }
    */
	
	 
    //setPromisc(bind_intf, &sock);
    
    
    return sock;
}

void do_debug(char *msg, ...){
  
  va_list argp;
  
  #ifdef DEBUG
	va_start(argp, msg);
	vfprintf(stderr, msg, argp);
	va_end(argp);
  #endif


}

/**************************************************************************
 * my_err: prints custom error messages on stderr.                        *
 **************************************************************************/
void my_err(char *msg, ...) {

  va_list argp;
  
  va_start(argp, msg);
  vfprintf(stderr, msg, argp);
  va_end(argp);
}




/*
 * error - wrapper for perror
 */
void error(char *msg)
{
	perror(msg);
	exit(1);
}

int main(int argc, char **argv)
{
	int sockfd;		/* socket */
	int portno;		/* port to listen on */
	int clientlen;		/* byte size of client's address */
	struct sockaddr_in serveraddr;	/* server's addr */
	struct sockaddr_in clientaddr;	/* client addr */
	struct hostent *hostp;	/* client host info */
	char *hostaddrp;	/* dotted decimal host addr string */
	int optval;		/* flag value for setsockopt */
	int n;			/* message byte size */
	char * bind_intf;
	char * bind_ip;
	char buf[BUFSIZE];

	/* 
	 * check command line arguments 
	 */
	if (argc != 3) {
		fprintf(stderr, "usage: %s <bind intf name> <bind ip addr>\n", argv[0]);
		exit(1);
	}
	bind_intf = argv[1];
	bind_ip = argv[2];
	portno = 12000;

	/* 
	 * socket: create the parent socket 
	 */
	sockfd = rawSocket(bind_intf);
	if (sockfd < 0)
		error("ERROR opening socket");

	/* setsockopt: Handy debugging trick that lets 
	 * us rerun the server immediately after we kill it; 
	 * otherwise we have to wait about 20 secs. 
	 * Eliminates "ERROR on binding: Address already in use" error. 
	 */
	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
		   (const void *)&optval, sizeof(int));

	/*
	 * build the server's Internet address
	 */
	bzero((char *)&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	//serveraddr.sin_addr.s_addr = INADDR_ANY;
	serveraddr.sin_addr.s_addr = inet_addr(bind_ip);
	serveraddr.sin_port = htons((unsigned short)portno);

	/* 
	 * bind: associate the parent socket with a port 
	 */
	if (bind(sockfd, (struct sockaddr *)&serveraddr,
		 sizeof(serveraddr)) < 0)
		error("ERROR on binding");
		

	/* 
	 * main loop: wait for a datagram, then echo it
	 */
	clientlen = sizeof(clientaddr);
	while (1) {

		/*
		 * recvfrom: receive a UDP datagram from a client
		 */
		memset(buf,0,BUFSIZE);
		do_debug("Server: Waiting for data ...\n");
		fflush(stdout);
		n = recvfrom(sockfd, buf, BUFSIZE, 0,
			     (struct sockaddr *)&clientaddr, &clientlen);
		if (n < 0)
			error("ERROR in recvfrom");


		

		if(n > 0){
			do_debug("Received new packet containing : %d bytes\n", n);
			fflush(stdout);
			//ProcessPacket(buf, n);
		}
		
		
	}
}