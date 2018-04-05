/************* UDP CLIENT CODE *******************/

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
#include <math.h>

#define BUF_SIZE 20000
#define DEBUG
#define IP_ADDR_SIZE 100

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
  fprintf(stderr, "%s -n <num_pkts> -d <dst_ip_addr>\n", progname);
  fprintf(stderr, "%s -h\n", progname);
  fprintf(stderr, "\n");
  exit(1);
}

void flush_buffer(char * buf, int size){
  int i = 0;
  for(i = 0 ; i < size; i++)
      buf[i] = '\0';
}



int main(int argc, char * argv[]){
  int clientSocket, portNum, nBytes;
  char buffer[BUF_SIZE];
  struct sockaddr_in serverAddr;
  struct sockaddr rcvaddr;
  int len;
  socklen_t addr_size;
  float avg_transmit_time = 0.0;
  float std_dev_transmit_time = 0.0;
  float avg_recv_rate = 0.0;
  float std_dev_recv_rate = 0.0;

  float nBytes_received = 0.0;



  int sock;
  char * dst_IP_ptr;
  char dst_IP[IP_ADDR_SIZE];

  int option;
  char sendbuf[BUF_SIZE];
  int n_pkts;
  int pkt_no = 0;

  memset(dst_IP, 0, IP_ADDR_SIZE);

  

  /* Check command line options */
  while((option = getopt(argc, argv, "n:d:h:")) > 0) {
    switch(option) {
      case 'h':
        usage(argv[0]);
        break;
      case 'n':
        n_pkts = atoi(optarg);
        break;
      case 'd':
        strcpy(dst_IP, optarg);
        break;
      default:
        my_err("Unknown option %c\n", option);
        usage(argv[0]);
    }
  }


  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
      perror("socket");
  }



  /*Configure settings in address struct*/
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(12000);
  serverAddr.sin_addr.s_addr = inet_addr(dst_IP);
  //memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);  

  /*Initialize size variable to be used later on*/
  addr_size = sizeof(serverAddr);
  len = sizeof(rcvaddr);

  usleep(1000000);

  struct timeval StartTimeStamp;
  gettimeofday(&StartTimeStamp, NULL);
  long StartTS = StartTimeStamp.tv_sec * 1000000 + StartTimeStamp.tv_usec;

  
  while(1){

    flush_buffer(buffer, BUF_SIZE);
    sprintf(buffer,"REQUEST: %d", pkt_no + 1);

    nBytes = strlen(buffer) + 1;
    
    struct timeval SendTimeStamp;
    struct timeval RecvTimeStamp;

    
    gettimeofday(&SendTimeStamp, NULL);
    do_debug("Sending REQUEST no %d\n", pkt_no + 1);

    /*Send message to server*/
    sendto(sock,buffer,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);
   

    flush_buffer(buffer, BUF_SIZE);
     do_debug("Sent REQUEST no %d, sock = %d\n", pkt_no + 1, sock);
    /*Receive message from server*/
    nBytes = recvfrom(sock,buffer,BUF_SIZE,0,((struct sockaddr *)&serverAddr), &addr_size);

    do_debug("Received from server: %d bytes\n",nBytes);

    if(nBytes > 0){
    gettimeofday(&RecvTimeStamp, NULL);

    nBytes_received += (float)nBytes;


    long RecvTS = RecvTimeStamp.tv_sec * 1000000 + RecvTimeStamp.tv_usec;
    long SendTS = SendTimeStamp.tv_sec * 1000000 + SendTimeStamp.tv_usec;

    //do_debug("Received from server: %s bytes\n",nBytes);

    pkt_no ++;


    double transit_time = (float)(RecvTS - SendTS)/(float)1000000.0; 
    avg_transmit_time += transit_time;
    std_dev_transmit_time += transit_time*transit_time;

    avg_recv_rate =  ((float)nBytes_received/(float)(RecvTS - StartTS))*8.0;

    double stdev = std_dev_transmit_time/pkt_no - ((avg_transmit_time/pkt_no)*(avg_transmit_time/pkt_no));
    if(stdev <= 0.0){
        stdev = 0.0;
    }
    else{
      stdev = sqrt(stdev);
    }


    do_debug("Start Time (sec) : %lu, Recv Time: %lu\n", StartTS, RecvTS);
    do_debug("Avg delay (sec) : %f\n", avg_transmit_time/pkt_no);
    do_debug("Std delay (sec) : %f\n", stdev);
    do_debug("Avg throughput : %f (Mbps), nBytes_received: %f\n", avg_recv_rate, nBytes_received);
    do_debug("#####################################################\n"); 

    }

  }

  return 0;
}