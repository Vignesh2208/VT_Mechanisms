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
#include "linkedlist.h"
#include "hashmap.h"
#include "TimeKeeper_functions.h"

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
int COMPUTATION_US = 50;



hashmap func_times_us;

void open_pipe(int * fd){
  int result = pipe(fd);
  if (result < 0){
        perror("pipe ");
        do_debug("Pipe open error\n");
        fflush(stdout);
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
    do_debug("Pipe send error\n");
    fflush(stdout);
    exit(0);
  }
}


int  pipe_read(int *fd) {
  char buf[BUFSIZE];
  flush_buffer(buf, BUFSIZE);
  int ret = read(fd[0], buf, BUFSIZE);

  if(ret < 0){
      do_debug("Pipe read error\n");
      fflush(stdout);
      exit(-1);
  }

  return atoi(buf);
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
  hmap_put_abs(&func_times_us, abs(str_hash("computation")), &COMPUTATION_US);  
  do_debug("Hmap initialized\n");


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

  /*Initialize size variable to be used later on*/
  addr_size = sizeof(serverAddr);  
  len = sizeof(rcvaddr);

  if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, NULL) < 0){
      finish_thread(&sock);
  }
  
  do_debug("Blocking on initial sleep ...\n");
  //blocking recvfrom. need to notify main thread about going to block through pipe.
  arg->is_blocked = 1;
  exp_curr_round_time_us = 0;
  pipe_send(arg->fds_thread_to_main, THREAD_BLOCKED);
  

  usleep(1000000);

  
  //blocking recvfrom. need to notify main thread about resume after blocking
  arg->is_blocked = 0;
  exp_curr_round_time_us = pipe_read(arg->fds_main_to_thread);
  if(exp_curr_round_time_us <= 0 || arg->stop){
    finish_thread(&sock);
  }
  do_debug("Resumed from sleep ...\n");


  struct timeval StartTimeStamp;
  gettimeofday(&StartTimeStamp, NULL);
  long StartTS = StartTimeStamp.tv_sec * 1000000 + StartTimeStamp.tv_usec;

  while(1){

    if(arg->stop){
      finish_thread(&sock);
      
    }

    flush_buffer(buffer, BUF_SIZE);
    sprintf(buffer,"REQUEST: %d", pkt_no + 1);

    if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "flush_buffer" ) < 0 || arg->stop){
        finish_thread(&sock);
    }


    nBytes = strlen(buffer) + 1;
    
    struct timeval SendTimeStamp;
    struct timeval RecvTimeStamp;

    
    gettimeofday(&SendTimeStamp, NULL);
    do_debug("Sending REQUEST no %d\n", pkt_no + 1);

    /*Send message to server*/
    sendto(sock,buffer,nBytes,0,(struct sockaddr *)&serverAddr,addr_size);

    if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "send_to" ) < 0 || arg->stop){
        finish_thread(&sock);
    }

   


    flush_buffer(buffer, BUF_SIZE);
    do_debug("Sent REQUEST no %d, sock = %d\n", pkt_no + 1, sock);

    if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "flush_buffer" ) < 0 || arg->stop){
        finish_thread(&sock);
    }

    arg->is_blocked = 1;
    exp_curr_round_time_us = 0;
    pipe_send(arg->fds_thread_to_main, THREAD_BLOCKED);



    /*Receive message from server*/
    nBytes = recvfrom(sock,buffer,BUF_SIZE,0,((struct sockaddr *)&serverAddr), &addr_size);
    arg->is_blocked = 0;
    exp_curr_round_time_us = pipe_read(arg->fds_main_to_thread);
    if(exp_curr_round_time_us <= 0 || arg->stop){
      finish_thread(&sock);
    }

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

        do_debug("Start Time (sec) : %lu, Recv Time: %lu\n", StartTS, RecvTS);
        do_debug("Avg delay (sec) : %f\n", avg_transmit_time/pkt_no);
        do_debug("Std delay (sec) : %f\n", sqrt(std_dev_transmit_time/pkt_no - ((avg_transmit_time/pkt_no)*(avg_transmit_time/pkt_no))));
        do_debug("Avg throughput : %f (Mbps), nBytes_received: %f\n", avg_recv_rate, nBytes_received);
        do_debug("#####################################################\n"); 


        if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "computation" ) < 0 || arg->stop){
          finish_thread(&sock);
        }

    }

  }



}




/***

  TK related

***/
#define MAX_PAYLOAD 1024 
#define TRACER_RESULTS 'J'
#define REL_CPU_SPEED 1.0
#define N_ROUND_INSNS 100000

int str_to_i(char *s)
{
        int i,n;
        n = 0;
        for(i = 0; *(s+i) >= '0' && *(s+i) <= '9'; i++)
                n = 10*n + *(s+i) - '0';
        return n;
}

int get_next_value (char *write_buffer)
{
        int i;
        for(i = 1; *(write_buffer+i) >= '0' && *(write_buffer+i) <= '9'; i++)
        {
                continue;
        }

        if(write_buffer[i] == '\0')
          return -1;

        if(write_buffer[i+1] == '\0')
          return -1;

        return (i + 1);
}

int get_next_command_tuple(char * buffer, int resume_ptr, pid_t * pid, u32 * n_insns){


  int i = resume_ptr;
  int nxt_idx = 0;
  if(strcmp(buffer + resume_ptr, "STOP") == 0 || resume_ptr < 0){
    *pid = -1;
    *n_insns = 0;
    return -1;
  }

  *pid = 0;
  *n_insns = 0;

  if(buffer[i] != '|' || buffer[i] == '\0')
    return -1;

  *pid = str_to_i(buffer + i + 1);

  nxt_idx = get_next_value(buffer + i);

  if(nxt_idx == -1){
    *pid = 0;
    *n_insns = 0;
    return -1;
  }
  else{
    nxt_idx = nxt_idx + i;
    *n_insns = str_to_i(buffer + nxt_idx);

    resume_ptr = nxt_idx + get_next_value(buffer + nxt_idx);
    resume_ptr = resume_ptr - 1;
    return resume_ptr;
  
  }

}

//From TK. Returns -1 if experiment has to be stopped.
int get_time_to_advance(int fp){

  int per_round_advance = 100;

  char nxt_cmd[MAX_PAYLOAD];
  flush_buffer(nxt_cmd, MAX_PAYLOAD);
  int tail_ptr = 0;
  pid_t new_cmd_pid = 0;
  u32 n_insns;
  int read_ret = -1;

  do_debug("Waiting for nxt cmd ...\n");
  while(read_ret == -1){
    read_ret = read(fp, nxt_cmd,fp);
    usleep(10000);
  }

  do_debug("Received Command: %s\n", nxt_cmd);
  while(tail_ptr != -1){
    tail_ptr = get_next_command_tuple(nxt_cmd, tail_ptr, &new_cmd_pid, &n_insns);

    if(tail_ptr == -1 && new_cmd_pid == -1){
      do_debug("STOP command received. Stopping...\n");
      return -1;
    }

    do_debug("Received Command n_insns: %d, tail_ptr: %d, new_cmd_pid=%d\n", n_insns, tail_ptr, new_cmd_pid);

    if(new_cmd_pid == 0)
          break;

    per_round_advance = (int) n_insns/1000;

    
  }

  if(per_round_advance < 0)
    return 100;

  do_debug("Advancing by: %d\n", per_round_advance);
  return per_round_advance;

}
/*
void write_results(int fp, int advance_err){

  char command[MAX_BUF_SIZ];
  flush_buffer(command,MAX_BUF_SIZ);
  sprintf(command, "%c,%d,", TRACER_RESULTS,advance_err);
  write(fp,command, strlen(command));
}*/

void write_results(int fp, int advance_err){


  do_debug("Writing results back \n");
  char command[MAX_BUF_SIZ];
  char * ptr;
  int ret = 0;
  flush_buffer(command,MAX_BUF_SIZ);
  sprintf(command, "%c,%d,", TRACER_RESULTS,advance_err);
  ptr = command;
  while(ret != strlen(command)){
    ret = write(fp,ptr, strlen(ptr));

    if(ret < 0){
      do_debug("Write Error!\n");
      perror("Write Error!\n");
      break;
    }
    ptr  = ptr + ret;

  }
}

/***
  END
***/

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

  addToExp(REL_CPU_SPEED, N_ROUND_INSNS);
  int fp = open("/proc/status", O_RDWR);

  if(fp == -1){
    do_debug("PROC file open error\n");
    exit(-1);;
  }


  while(1){

      //do_debug("Getting next command:\n");
      per_round_advance = get_time_to_advance(fp);
      if(per_round_advance == -1){
        // STOPPING EXPERIMENT
        args.stop = 1;
        do_debug("MAIN: Instructing Thread to Stop. Time elapsed = %d\n",time_elapsed);
        pipe_send(args.fds_main_to_thread, THREAD_EXITING);
        break;
      }
      else{

          if(!args.is_blocked){
            do_debug("MAIN: Instructing Thread to run for %d us. Time elapsed = %d\n",per_round_advance, time_elapsed);
            pipe_send(args.fds_main_to_thread, per_round_advance);
            ret = pipe_read(args.fds_thread_to_main);
            if(ret == THREAD_BLOCKED){
              do_debug("MAIN: Detected thread block\n");
              time_elapsed = time_elapsed + per_round_advance;
              write_results(fp,0);
            }
            else if(ret >= 0){
              do_debug("MAIN: Return = %d\n", ret);
              time_elapsed = time_elapsed + ret + per_round_advance;
              write_results(fp,ret*1000);
              //write_results(fp,0);
            }
            else
              write_results(fp,0);

          }
          else{
            // if still blocked
            do_debug("MAIN. Thread Still Blocked\n");
            time_elapsed += per_round_advance;
            usleep(per_round_advance);
            write_results(fp,0);
          }
      }

  }
  
  do_debug("MAIN: Waiting for all threads to exit\n");
  pthread_join(tid,NULL);
  return 0;
}