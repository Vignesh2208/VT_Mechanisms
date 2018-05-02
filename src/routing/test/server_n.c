#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
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
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <stdarg.h>

#define BUF_SIZE 1000
#define DEBUG
#define IP_ADDR_SIZE 100

struct timeval start_sendto;
struct timeval stop_sendto;
struct timeval start_flush;
struct timeval stop_flush;
struct timeval start_fill;
struct timeval stop_fill;

int n_sendto = 0;
int n_flush = 0;
int n_fill = 0;

float avg_sendto = 0.0;
float avg_flush = 0.0;
float avg_fill = 0.0;


void do_debug(char *msg, ...){
  
  va_list argp;
  
  #ifdef DEBUG
  va_start(argp, msg);
  vfprintf(stderr, msg, argp);
  va_end(argp);
  #endif

  fflush(stdout);


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


void usage(char * progname) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "%s -d <bind ip addr>\n", progname);
  fprintf(stderr, "%s -h\n", progname);
  fprintf(stderr, "\n");
  exit(1);
}


void fill_buffer(char * buffer, int size){

  int i = 0;
  for(i = 0 ; i < size; i++){
    buffer[i] = 'A';
  }

}

void flush_buffer(char * buf, int size){
  int i = 0;
  for(i = 0 ; i < size; i++)
      buf[i] = '\0';
}

int main(int argc, char * argv[]){
  int udpSocket, nBytes;
  char buffer[BUF_SIZE];
  struct sockaddr_in serverAddr, clientAddr;
  struct sockaddr_storage serverStorage;
  socklen_t addr_size, client_addr_size;
  int i;

  char dst_IP[IP_ADDR_SIZE];

  int option;
  int n_pkts;
  int pkt_no = 0;

  memset(dst_IP,0, IP_ADDR_SIZE);


  /* Check command line options */
  while((option = getopt(argc, argv, "d:h:")) > 0) {
    switch(option) {
      case 'h':
        usage(argv[0]);
        break;
      case 'd':
        strcpy(dst_IP, optarg);
        break;
      default:
        my_err("Unknown option %c\n", option);
        usage(argv[0]);
    }
  }


  
  /*Create UDP socket*/
  udpSocket = socket(AF_INET, SOCK_DGRAM, 0);

  /*Configure settings in address struct*/
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(12000);
  serverAddr.sin_addr.s_addr = inet_addr(dst_IP);
  memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);  

  /*Bind socket with address struct*/
  bind(udpSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));

  int optval = 1;
  setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

  /*Initialize size variable to be used later on*/
  addr_size = sizeof(serverStorage);

   printf("My process ID : %d\n", getpid());

  while(1){

    gettimeofday(&start_flush,NULL);
    flush_buffer(buffer, BUF_SIZE);
    gettimeofday(&stop_flush,NULL);

    n_flush ++;
    /* Try to receive any incoming UDP datagram. Address and port of 
      requesting client will be stored on serverStorage variable */
    //do_debug("Waiting for request\n", nBytes);
    nBytes = recvfrom(udpSocket,buffer,BUF_SIZE,0,(struct sockaddr *)&serverStorage, &addr_size);

    if(nBytes){

      //do_debug("Received :%d bytes from client\n", nBytes);
      /*Convert message received to uppercase*/

      gettimeofday(&start_fill,NULL);
      memset(buffer,0,BUF_SIZE);
      fill_buffer(buffer, BUF_SIZE - 1);
      gettimeofday(&stop_fill,NULL);
      n_fill ++;


      gettimeofday(&start_sendto,NULL);
      /*Send uppercase message back to client, using serverStorage as the address*/
      sendto(udpSocket,buffer,BUF_SIZE-1,0,(struct sockaddr *)&serverStorage,addr_size);

      gettimeofday(&stop_sendto,NULL);

      n_sendto ++;

      avg_fill += (stop_fill.tv_sec * 1000000 + stop_fill.tv_usec) - (start_fill.tv_sec * 1000000 + start_fill.tv_usec);
      avg_flush += (stop_flush.tv_sec * 1000000 + stop_flush.tv_usec) - (start_flush.tv_sec * 1000000 + start_flush.tv_usec);
      avg_sendto += (stop_sendto.tv_sec * 1000000 + stop_sendto.tv_usec) - (start_sendto.tv_sec * 1000000 + start_sendto.tv_usec);

      /*
      do_debug("Avg fill : %f (us)\n", avg_fill/n_fill);
      do_debug("Avg sendto : %f (us)\n", avg_sendto/n_sendto);
      do_debug("Avg flush : %f (us)\n", avg_flush/n_flush);
      do_debug("#####################################################\n"); 
      */




    }
  }

  return 0;
}