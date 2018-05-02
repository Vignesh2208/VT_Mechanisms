#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ether.h>
#include <linux/if_packet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "../utils/linkedlist.h"
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <stdarg.h>

#define BUF_SIZE 2000
#define BUFSIZE BUF_SIZE
#define IP_ADDR_SIZE 100

#define MY_DEST_MAC0  0x00
#define MY_DEST_MAC1  0x11
#define MY_DEST_MAC2  0x22
#define MY_DEST_MAC3  0x33
#define MY_DEST_MAC4  0x44
#define MY_DEST_MAC5  0x55

#define SUCCESS 1
#define FAIL -1
#define DEBUG

llist arp_list;

struct pseudo_header
{
    u_int32_t source_address;
    u_int32_t dest_address;
    u_int8_t placeholder;
    u_int8_t protocol;
    u_int16_t udp_length;
};


unsigned short udp_csum(unsigned short *ptr,int nbytes) 
{
    register long sum;
    unsigned short oddbyte;
    register short answer;
 
    sum=0;
    while(nbytes>1) {
        sum+=*ptr++;
        nbytes-=2;
    }
    if(nbytes==1) {
        oddbyte=0;
        *((u_char*)&oddbyte)=*(u_char*)ptr;
        sum+=oddbyte;
    }
 
    sum = (sum>>16)+(sum & 0xffff);
    sum = sum + (sum>>16);
    answer=(short)~sum;
     
    return(answer);
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



typedef struct arp_table_entry{
	char ip_address[IP_ADDR_SIZE];
	int mac[6];

} arp_entry;

int str_to_i(char *s)
{
        int i,n;
        n = 0;
        for(i = 0; *(s+i) >= '0' && *(s+i) <= '9'; i++)
                n = 10*n + *(s+i) - '0';
        return n;
}


arp_entry * get_arp_entry(char * ip_addr, llist * arp_list){

	llist_elem * head = arp_list->head;
	arp_entry * entry;
	while(head != NULL) {
		entry = (arp_entry*) head->item;
		if(strcmp(entry->ip_address, ip_addr) == 0)
			return entry;

		head = head->next;
	}

	return NULL;
	

}


void print_arp_list(llist * arp_list) {

  llist_elem * head = arp_list->head;
  arp_entry * entry;
  do_debug("Active routing table:\n");
  while(head != NULL) {
    entry = (arp_entry*) head->item;
    do_debug("-> IP:%s, MAC=%x:%x:%x:%x:%x:%x\n",entry->ip_address, entry->mac[0],entry->mac[1],entry->mac[2],entry->mac[3],entry->mac[4],entry->mac[5]);
    head = head->next;
  }
  do_debug("\n");
}

int populate_arp_list(char * f_name){

  FILE * fp;
  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  char tmp[BUFSIZE];
  int i = 0;
  int if_idx;
  int j = 0;

  arp_entry * new_entry = NULL;

  fp = fopen(f_name, "r");
  if (fp == NULL){
    printf("Failed to open file: %s\n", f_name);
    exit(FAIL);
  }

  while ((read = getline(&line, &len, fp)) != -1) {

      if(len > 1) {
        //do_debug("Retrieved line of length %zu :\n", read);
        //do_debug("%s\n", line);

        new_entry = (arp_entry *)malloc(sizeof(arp_entry));
        if(!new_entry){
          printf("NO MEM ERROR\n");
          exit(FAIL);
        }

        flush_buffer(tmp, BUFSIZE);
        strncpy(tmp,line,len);
        if_idx = 0;
        for(i = 0; i < len; i++){
          if(tmp[i] == ' ' || tmp[i] == '\t' || tmp[i] == '\n'){
            

            if(tmp[i] != '\n')
              if_idx = i + 1;
            
            tmp[i] = '\0';
          }
        }


        i = if_idx;
        j = 0;
        //printf("%s\n", tmp + if_idx);
        while(tmp[i] != '\0'){
        	if(tmp[i] == ':'){
        		tmp[i] = '\0';
        		//printf("%s\n", tmp + i - 2);
        		new_entry->mac[j] = (int)strtol(tmp + i-2, NULL, 16);
        		j++;
        		if(j == 5){
        			new_entry->mac[j] = (int)strtol(tmp + i + 1, NULL, 16);
        			break;
        		}
        	}
        	
        	i++;
        }
        strcpy(new_entry->ip_address, tmp);
        
        llist_append(&arp_list, new_entry);

      }
  }

  print_arp_list(&arp_list);

  return SUCCESS;



}


void flush_buffer(char * buf, int size){
	int i = 0;
	for(i = 0; i < size; i++)
		buf[i] = '\0';
}

unsigned short csum(unsigned short *buf, int nwords)
{
    unsigned long sum;
    for(sum=0; nwords>0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum &0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

int construct_packet(char * sendbuf, struct ifreq * if_mac, struct ifreq * if_idx, struct ifreq * if_ip, char * dst_IP, llist * arp_list){

	int tx_len = 0;
	struct ether_header *eh = (struct ether_header *) sendbuf;
	arp_entry * entry = NULL;
	char * data;
	char * pseudogram;
	struct pseudo_header psh;


	//memset(sendbuf, 0, BUF_SIZE);
	flush_buffer(sendbuf,BUF_SIZE);

	/* Ethernet header */
	eh->ether_shost[0] = if_mac->ifr_hwaddr.sa_data[0];
	eh->ether_shost[1] = if_mac->ifr_hwaddr.sa_data[1];
	eh->ether_shost[2] = if_mac->ifr_hwaddr.sa_data[2];
	eh->ether_shost[3] = if_mac->ifr_hwaddr.sa_data[3];
	eh->ether_shost[4] = if_mac->ifr_hwaddr.sa_data[4];
	eh->ether_shost[5] = if_mac->ifr_hwaddr.sa_data[5];

	data = sendbuf + sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct udphdr);
	strcpy(data , "HelloWorld");

	do_debug("src mac: %x:%x:%x:%x:%x:%x\n", eh->ether_shost[0], eh->ether_shost[1], eh->ether_shost[2], eh->ether_shost[3] , eh->ether_shost[4], eh->ether_shost[5]);

	entry = get_arp_entry(dst_IP, arp_list);

	if(!entry){
		eh->ether_dhost[0] = MY_DEST_MAC0;
		eh->ether_dhost[1] = MY_DEST_MAC1;
		eh->ether_dhost[2] = MY_DEST_MAC2;
		eh->ether_dhost[3] = MY_DEST_MAC3;
		eh->ether_dhost[4] = MY_DEST_MAC4;
		eh->ether_dhost[5] = MY_DEST_MAC5;
	}
	else{
		eh->ether_dhost[0] = entry->mac[0];
		eh->ether_dhost[1] = entry->mac[1];
		eh->ether_dhost[2] = entry->mac[2];
		eh->ether_dhost[3] = entry->mac[3];
		eh->ether_dhost[4] = entry->mac[4];
		eh->ether_dhost[5] = entry->mac[5];
	}

	do_debug("dst mac: %x:%x:%x:%x:%x:%x\n", eh->ether_dhost[0], eh->ether_dhost[1], eh->ether_dhost[2], eh->ether_dhost[3] , eh->ether_dhost[4], eh->ether_dhost[5]);


	eh->ether_type = htons(ETH_P_IP);
	tx_len += sizeof(struct ether_header);

	struct iphdr *iph = (struct iphdr *) (sendbuf + sizeof(struct ether_header));
	/* IP Header */
	iph->ihl = 5;
	iph->version = 4;
	iph->tos = 0; // Low delay
	iph->id = htons(54321);
	iph->ttl = 64; // hops
	iph->protocol = 17; // UDP
	iph->check = 0;
	/* Source IP address, can be spoofed */
	iph->saddr = inet_addr(inet_ntoa(((struct sockaddr_in *)&if_ip->ifr_addr)->sin_addr));
	// iph->saddr = inet_addr("192.168.0.112");
	/* Destination IP address */
	iph->daddr = inet_addr(dst_IP);
	tx_len += sizeof(struct iphdr);

	struct udphdr *udph = (struct udphdr *) (sendbuf + sizeof(struct iphdr) + sizeof(struct ether_header));
	udph->source = htons(3423);
	udph->dest = htons(12000);
	udph->check = 0; // skip
	tx_len += sizeof(struct udphdr);

	/*
	sendbuf[tx_len++] = 'H';
	sendbuf[tx_len++] = 'e';
	sendbuf[tx_len++] = 'l';
	sendbuf[tx_len++] = 'l';
	sendbuf[tx_len++] = 'o';
	*/

	tx_len += strlen(data);

	/* Length of UDP payload and header */
	udph->len = htons(tx_len - sizeof(struct ether_header) - sizeof(struct iphdr));
	/* Length of IP payload and header */
	iph->tot_len = htons(tx_len - sizeof(struct ether_header));
	/* Calculate IP checksum on completed header */
	//iph->check = csum((unsigned short *)(sendbuf+sizeof(struct ether_header)), sizeof(struct iphdr)/2);
	iph->check = udp_csum((unsigned short *)(sendbuf+sizeof(struct ether_header)), sizeof(struct iphdr));


	psh.source_address = inet_addr(inet_ntoa(((struct sockaddr_in *)&if_ip->ifr_addr)->sin_addr));
    psh.dest_address = inet_addr(dst_IP);
    psh.placeholder = 0;
    psh.protocol = IPPROTO_UDP;
    psh.udp_length = htons(sizeof(struct udphdr) + strlen(data) );
     
    int psize = sizeof(struct pseudo_header) + sizeof(struct udphdr) + strlen(data);
    pseudogram = malloc(psize);
     
    memcpy(pseudogram , (char*) &psh , sizeof (struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header) , udph , sizeof(struct udphdr) + strlen(data));
     
    udph->check = udp_csum( (unsigned short*) pseudogram , psize);

    free(pseudogram);

    return tx_len;


}

int send_next_packet(int sock, char * sendbuf, int tx_len,  struct ifreq * if_idx){

	struct ether_header *eh = (struct ether_header *) sendbuf;
	
	/* Destination address */
	struct sockaddr_ll socket_address;
	/* Index of the network device */
	socket_address.sll_ifindex = if_idx->ifr_ifindex;
	/* Address length*/
	socket_address.sll_halen = ETH_ALEN;
	/* Destination MAC */
	socket_address.sll_addr[0] = eh->ether_dhost[0];
	socket_address.sll_addr[1] = eh->ether_dhost[1];
	socket_address.sll_addr[2] = eh->ether_dhost[2];
	socket_address.sll_addr[3] = eh->ether_dhost[3];
	socket_address.sll_addr[4] = eh->ether_dhost[4];
	socket_address.sll_addr[5] = eh->ether_dhost[5];

	if (sendto(sock, sendbuf, tx_len, 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0)
    	printf("Send failed\n");
    else
    	printf("Send succeeded!\n");

    return 1;
}

void usage(char * progname) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "%s -i <intf_name> -d <dst_ip_addr> -f <arp table file>\n", progname);
  fprintf(stderr, "%s -h\n", progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "-i <intf_name>: Name of interface to bind to for send.(mandatory)\n");
  fprintf(stderr, "-d <dst_ip_addr to send> (mandatory)\n");
  fprintf(stderr, "-f <file path containing static arp table entries>\n");
  fprintf(stderr, "-h: prints this help text\n");
  exit(1);
}

int main(int argc, char * argv[]){

	int sock;
	char * dst_IP_ptr;
	char dst_IP[IP_ADDR_SIZE];

	int option;
	char if_name[IFNAMSIZ];
	char arp_file_path[100];
	char sendbuf[BUF_SIZE];




	flush_buffer(dst_IP, IP_ADDR_SIZE);
	flush_buffer(if_name, IFNAMSIZ);


	/* Check command line options */
	while((option = getopt(argc, argv, "i:d:f:")) > 0) {
		switch(option) {
		  case 'h':
		    usage(argv[0]);
		    break;
		  case 'i':
		    strncpy(if_name,optarg, IFNAMSIZ-1);
		    break;
		  case 'd':
		    strcpy(dst_IP, optarg);
		    break;
		  case 'f' :
		  	flush_buffer(arp_file_path, 100);
		  	strncpy(arp_file_path, optarg, 100);
		  	break;
		  default:
		    my_err("Unknown option %c\n", option);
		    usage(argv[0]);
		}
	}


	llist_init(&arp_list);
	populate_arp_list(arp_file_path);
	//exit(SUCCESS);

	if ((sock = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
	    perror("socket");
	}


	struct ifreq if_idx;
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, if_name, IFNAMSIZ-1);
	if (ioctl(sock, SIOCGIFINDEX, &if_idx) < 0)
	    perror("SIOCGIFINDEX");

	struct ifreq if_mac;
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, if_name, IFNAMSIZ-1);
	if (ioctl(sock, SIOCGIFHWADDR, &if_mac) < 0)
	    perror("SIOCGIFHWADDR");

	struct ifreq if_ip;
	memset(&if_ip, 0, sizeof(struct ifreq));
	strncpy(if_ip.ifr_name, if_name, IFNAMSIZ-1);
	if (ioctl(sock, SIOCGIFADDR, &if_ip) < 0)
	    perror("SIOCGIFADDR");

	int i = 0;
	int tx_len = construct_packet(sendbuf,  &if_mac, &if_idx, &if_ip, dst_IP, &arp_list);
	//for(i = 0; i < 100; i++){
	while(1){
		printf("Sending packet: %d\n", i);
		fflush(stdout);
		//send_next_packet(sock, &if_mac, &if_idx, &if_ip, dst_IP, &arp_list);
		send_next_packet(sock, sendbuf, tx_len, &if_idx);
		fflush(stdout);
		usleep(1000000);
		i++;
	}

	llist_destroy(&arp_list);
}