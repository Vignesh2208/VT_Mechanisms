/************* UDP SERVER CODE *******************/

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
#include "linkedlist.h"
#include "hashmap.h"

#define BUF_SIZE 20000
#define DEBUG
#define IP_ADDR_SIZE 100
#define SUCCESS 1
#define FAIL -1


#define THREAD_BLOCKED -1
#define THREAD_READY 0
#define THREAD_EXITING -2
#define BUFSIZE 2000  

int SENDTO_US = 50;
int FLUSH_BUFFER_US = 50;
int FILL_BUFFER_US = 50;

hashmap func_times_us;

void open_pipe(int * fd){
  int result = pipe(fd);
  if (result < 0){
        perror("pipe ");
        exit(1);
   }

}

void pipe_send(int * fd, int msg){


  char msg_buf[BUFSIZE];
  flush_buffer(msg_buf, BUFSIZE);
  sprintf(msg_buf,"%d", msg);
  int ret = write(fd[1], msg_buf, strlen(msg_buf));
  if(ret != strlen(msg_buf)){
    perror("Write error\n");
    exit(0);
  }
}


int  pipe_read(int *fd) {
  char buf[BUFSIZE];
  flush_buffer(buf, BUFSIZE);
  read(fd[0], buf, BUFSIZE);

  return atoi(buf);
}


//From TK. Returns -1 if experiment has to be stopped.
int get_time_to_advance(){

  int per_round_advance = 100; 
  return per_round_advance;

}

typedef struct thread_aruments {
  char dst_IP[IP_ADDR_SIZE];
  int n_pkts;
  int fds_main_to_thread[2];
  int fds_thread_to_main[2];
  int is_blocked;
  int stop;
} thread_args;



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



int update_stub(int * curr_round_time_left, int *thread_to_main_fds, int * main_to_thread_fds, char * last_fn){
  if(!last_fn){
    *curr_round_time_left = 0;
    //pipe_send(thread_to_main_fds, THREAD_READY);
    do_debug("THREAD. Waiting for first cmd\n");
    *curr_round_time_left = pipe_read(main_to_thread_fds);
    do_debug("THREAD. Got first cmd: %d\n", *curr_round_time_left);
    if(*curr_round_time_left < 0)
      return THREAD_EXITING;

    return SUCCESS;
  }
  

  if(hmap_get_abs(&func_times_us, abs(str_hash(last_fn))) == NULL)
      return SUCCESS;

  int * time_elapsed = (int *)hmap_get_abs(&func_times_us, abs(str_hash(last_fn)));
  *curr_round_time_left = *curr_round_time_left - *time_elapsed;

  do_debug("THREAD. Updating curr_round_time_left: %d, last_fn = %s\n", *curr_round_time_left, last_fn);

  if(*curr_round_time_left <= 0){

      do_debug("THREAD. Finishing round\n");
      pipe_send(thread_to_main_fds, -1* (*curr_round_time_left));
      *curr_round_time_left = pipe_read(main_to_thread_fds);

      if(*curr_round_time_left <= 0)
        return THREAD_EXITING;

      return SUCCESS;
  }

}



void finish_thread(int *sock){
  close(*sock);
  pthread_exit(NULL);
}

void init_func_times(){

  hmap_init(&func_times_us,1000);

  hmap_put_abs(&func_times_us, abs(str_hash("sendto")), &SENDTO_US);
  hmap_put_abs(&func_times_us, abs(str_hash("flush_buffer")), &FLUSH_BUFFER_US);
  hmap_put_abs(&func_times_us, abs(str_hash("fill_buffer")), &FILL_BUFFER_US);  
  do_debug("Hmap initialized\n");


}


void fill_buffer(char * buffer, int size){

  int i = 0;
  for(i = 0 ; i < size; i++){
    buffer[i] = 'A';
  }

}

void process_thread(void * args){

  thread_args * arg = (thread_args *) args;
  char * dst_IP = arg->dst_IP;
  int n_pkts = arg->n_pkts;
  int pkt_no = 0;

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
  struct sockaddr_storage serverStorage;

  float nBytes_received = 0.0;
  int sock;
  int exp_curr_round_time_us = 0;
  int ret = 0;


  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
      perror("socket");
  }



  /*Configure settings in address struct*/
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(12000);
  serverAddr.sin_addr.s_addr = inet_addr(dst_IP);
  //memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero); 

  memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);  


  bind(sock, (struct sockaddr *) &serverAddr, sizeof(serverAddr));

  int optval = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));


  /*Initialize size variable to be used later on*/
  addr_size = sizeof(serverStorage);

  struct timeval StartTimeStamp;
  gettimeofday(&StartTimeStamp, NULL);
  long StartTS = StartTimeStamp.tv_sec * 1000000 + StartTimeStamp.tv_usec;

  len = sizeof(rcvaddr);

  if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, NULL) < 0){
      finish_thread(&sock);
  }


  while(1){

    if(arg->stop){
      finish_thread(&sock);
      
    }

    flush_buffer(buffer, BUF_SIZE);
    if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "flush_buffer" ) < 0 || arg->stop){
        finish_thread(&sock);
    }





    do_debug("Waiting for next request\n");
    arg->is_blocked = 1;
    exp_curr_round_time_us = 0;
    pipe_send(arg->fds_thread_to_main, THREAD_BLOCKED);


    nBytes = recvfrom(sock,buffer,BUF_SIZE,0,(struct sockaddr *)&serverStorage, &addr_size);

    arg->is_blocked = 0;
    exp_curr_round_time_us = pipe_read(arg->fds_main_to_thread);
    if(exp_curr_round_time_us <= 0 || arg->stop){
      finish_thread(&sock);
    }



    if(nBytes){

      do_debug("Received :%d bytes from client\n", nBytes);
      /*Convert message received to uppercase*/
      memset(buffer,0,BUF_SIZE);
      fill_buffer(buffer, BUF_SIZE - 1);

      if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "fill_buffer" ) < 0 || arg->stop){
        finish_thread(&sock);
      }


      /*Send uppercase message back to client, using serverStorage as the address*/
      sendto(sock,buffer,BUF_SIZE-1,0,(struct sockaddr *)&serverStorage,addr_size);

      if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "sendto" ) < 0 || arg->stop){
        finish_thread(&sock);
      }


    }
    

  }



}


int main(int argc, char * argv[]){
  
  char * dst_IP_ptr;
  char dst_IP[IP_ADDR_SIZE];

  int option;
  char sendbuf[BUF_SIZE];
  int n_pkts;
  int pkt_no = 0;
  thread_args args;
  pthread_t tid;
  int per_round_advance = 100;
  int time_elapsed = 0;
  int ret = 0;
  memset(dst_IP, 0, IP_ADDR_SIZE);

  //usleep(2000000);

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


  init_func_times();
  do_debug("Function times initialized !\n");

  
  strcpy(args.dst_IP, dst_IP);
  args.n_pkts = n_pkts;
  args.is_blocked = 0;
  args.stop = 0;
  do_debug("Starting Thread\n");
  do_debug("Starting Thread. Opening pipes\n");
  open_pipe(args.fds_main_to_thread);
  open_pipe(args.fds_thread_to_main);
  time_elapsed = 0;
  pthread_create(&tid, NULL, process_thread, &args);
  
  do_debug("Started all threads\n");


  while(1){

      per_round_advance = get_time_to_advance();
      if(per_round_advance == -1){
        // STOPPING EXPERIMENT
        args.stop = 1;
        do_debug("MAIN: Instructing Thread to Stop. Time elapsed = %d\n",time_elapsed);
        pipe_send(args.fds_main_to_thread, THREAD_EXITING);
        break;
      }
      else{

          if(!args.is_blocked){
            do_debug("MAIN: Instructing Thread to run for 10ms. Time elapsed = %d\n",time_elapsed);
            pipe_send(args.fds_main_to_thread, per_round_advance);
            ret = pipe_read(args.fds_thread_to_main);
            if(ret == THREAD_BLOCKED){
              do_debug("MAIN: Detected thread block\n");
              time_elapsed = time_elapsed + per_round_advance;
            }
            else if(ret >= 0){
              do_debug("MAIN: Return = %d\n", ret);
              time_elapsed = time_elapsed + ret + per_round_advance;
            }

          }
          else{
            // if still blocked
            //do_debug("MAIN. Thread: %d. Still Blocked\n", i);
            //time_elapsed += per_round_advance;
            usleep(per_round_advance);
          }
      }

  }
  
  do_debug("MAIN: Waiting for all threads to exit\n");
  pthread_join(tid,NULL);
  return 0;
}