#include "includes.h"
#include "utils.h"

llist routing_list;
pthread_mutex_t lock;


struct timeval start_sendto;
struct timeval stop_sendto;
struct timeval start_flush;
struct timeval stop_flush;
struct timeval start_poll;
struct timeval stop_poll;
struct timeval start_get;
struct timeval stop_get;
struct timeval start_recv;
struct timeval stop_recv;


int n_sendto = 0;
int n_flush = 0;
int n_poll = 0;
int n_get = 0;
int n_recv = 0;

float avg_sendto = 0.0;
float avg_flush = 0.0;
float avg_poll = 0.0;
float avg_get = 0.0;
float avg_recv = 0.0;




/**************************************************************************
 * usage: prints usage and exits.                                         *
 **************************************************************************/
void usage(char * progname) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "%s -i <name> -n <number of interfaces> -f <routing table file>\n", progname);
  fprintf(stderr, "%s -h\n", progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "-i <name>: Name assigned to router (mandatory)\n");
  fprintf(stderr, "-n <number of interfaces in router> (mandatory)\n");
  fprintf(stderr, "-f <file path containing static routing table entries>\n");
  fprintf(stderr, "-h: prints this help text\n");
  exit(1);
}

int main(int argc, char *argv[]) {
  
	int option;
	int n_intfs = 0;
	char switch_name[IFNAMSIZ];
	char *intf_names[N_MAX_TAPS];
	int time_elapsed;
	int maxfd = -1;
	uint16_t nread, nwrite;
	char buffer[BUFSIZE];
	struct sockaddr_in local, remote;
	int i = 0;
	int start = 0;
	int ret;
	char routing_file_path[100];
	int runnable = 0;


	struct pollfd * poll_set;
  	int data_size[5];
 	struct sockaddr_in rcvaddr;
	int len;
	int socks[N_MAX_TAPS];
	char buf[5][BUFSIZE];
	int dst_sock_idx;
	int dst_sock ;
	int sock;

	flush_buffer(switch_name, IFNAMSIZ);
	flush_buffer(buffer, BUFSIZE);


	/* Check command line options */
	while((option = getopt(argc, argv, "i:n:f:")) > 0) {
		switch(option) {
		  case 'h':
		    usage(argv[0]);
		    break;
		  case 'i':
		    strncpy(switch_name,optarg, IFNAMSIZ-1);
		    break;
		  case 'n':
		    n_intfs = atoi(optarg);
		    break;
		  case 'f' :
		  	flush_buffer(routing_file_path, 100);
		  	strncpy(routing_file_path, optarg, 100);
		  	break;
		  default:
		    my_err("Unknown option %c\n", option);
		    usage(argv[0]);
		}
	}

	argv += optind;
	argc -= optind;

	if(argc > 0) {
		my_err("Too many options!\n");
		usage(argv[0]);
	}

	if(*switch_name == '\0') {
		my_err("Must specify switch name!\n");
		usage(argv[0]);
	} else if(n_intfs <= 0) {
		my_err("Number of interfaces must be greater than 0\n");
		usage(argv[0]);
	} 


	if(switch_name[0] == 'h') {
		start = 0;
	}
	else{
		start = 1;
	}

  
	for(i = 0 ; i < n_intfs; i++){
		intf_names[i] = (char *) malloc(IF_NAME_SIZE*sizeof(char));
		if(!intf_names[i]){
			my_err("NO MEM error\n");
			exit(-1);
		}
		flush_buffer(intf_names[i], IF_NAME_SIZE);
		sprintf(intf_names[i],"%s-eth%d",switch_name,start);
		start ++;
		socks[i] = 0;
	}

	do_debug("Starting up. Switch name: %s\n", switch_name);
	do_debug("Active interface:\n");



	for(i = 0; i < n_intfs; i++){
		do_debug("-> %s\n", intf_names[i]);
	}


	llist_init(&routing_list);
	populate_routing_list(routing_file_path);


	poll_set = malloc(n_intfs*sizeof(struct pollfd));



	if(!poll_set){
		do_debug("Poll set Alloc NOMEM\n");
		pthread_exit(NULL);
	}
  

	for(i = 0; i < n_intfs; i++){
	  sock = rawSocket(0, intf_names[i]);
	  socks[i] = sock;

	  do_debug("Sock[%d] = %d\n", i, socks[i]);
	  //setPromisc(0, intf_names[i], &socks[i]);
	  poll_set[i].fd = sock;
	  poll_set[i].events = POLLIN;
	}


	do_debug("Opened all sockets. Active interfaces:\n");
	do_debug("My Process PID: %d\n", getpid());

	for(i = 0; i < n_intfs; i++){
		do_debug("-> %s\n", intf_names[i]);
	}

 
  	while(1) {


  		repeat:


  		gettimeofday(&start_poll,NULL);


  		for(i = 0; i < n_intfs; i++){
	  
	  	  //do_debug("Sock[%d] = %d\n", i, socks[i]);
		  poll_set[i].fd = socks[i];
		  poll_set[i].events = POLLIN;
		}

   
		ret = poll(poll_set, n_intfs, 0);
		gettimeofday(&stop_poll,NULL);
		n_poll ++;


		avg_poll += (stop_poll.tv_sec * 1000000 + stop_poll.tv_usec) - (start_poll.tv_sec * 1000000 + start_poll.tv_usec);

		if (ret < 0 && errno == EINTR){
		  continue;
		}



		if (ret < 0) {
		  perror("poll()");
		  pthread_exit(NULL);
		}



		runnable = 0;
		len = sizeof(rcvaddr);
		for(i = 0; i < n_intfs; i++){
		  data_size[i] = 0;
		  sock = socks[i];
		  if((poll_set[i].revents & POLLIN) && (poll_set[i].fd == sock)) {
		    //runnable = 1;

		    gettimeofday(&start_flush,NULL);
		    flush_buffer(buf[i],BUFSIZE);
		    gettimeofday(&stop_flush,NULL);
		    n_flush ++;

		    avg_flush += (stop_flush.tv_sec * 1000000 + stop_flush.tv_usec) - (start_flush.tv_sec * 1000000 + start_flush.tv_usec);


		    gettimeofday(&start_recv,NULL);
		    data_size[i] = recvfrom(sock,buf[i],BUFSIZE,0,(struct sockaddr*)&rcvaddr,&len);
		    gettimeofday(&stop_recv,NULL);
		    n_recv ++;
		    avg_recv += (stop_recv.tv_sec * 1000000 + stop_recv.tv_usec) - (start_recv.tv_sec * 1000000 + start_recv.tv_usec);

		   }
		}

		for(i = 0; i < n_intfs; i++){

		  	sock = socks[i];
		    if(data_size[i] > 0){
		    	gettimeofday(&start_get,NULL);
		        dst_sock_idx = get_dst_sock_idx(buf[i], n_intfs, intf_names);
		        gettimeofday(&stop_get,NULL);
		        n_get ++;
		        avg_get += (stop_get.tv_sec * 1000000 + stop_get.tv_usec) - (start_get.tv_sec * 1000000 + start_get.tv_usec);


		        if(dst_sock_idx != -1){
		          dst_sock = socks[dst_sock_idx];
		          //do_debug("Recv sock: %d, Dst sock idx = %d, dst_sock = %d\n", sock, dst_sock_idx, dst_sock);
		          gettimeofday(&start_sendto,NULL);
		          send_packet(buf[i], data_size[i], dst_sock, intf_names[dst_sock_idx]);
		          gettimeofday(&stop_sendto,NULL);
		          n_sendto ++;
		          avg_sendto += (stop_sendto.tv_sec * 1000000 + stop_sendto.tv_usec) - (start_sendto.tv_sec * 1000000 + start_sendto.tv_usec);
		        }
		        else{
		        	//continue;
		        }
		    }


		} //end of for

		

		if(runnable == 0){

		    // going to block indefinitely
		   

		    while(1){
		      ret = poll(poll_set, n_intfs, -1);
		      
		      if(ret < 0 && errno == EINTR)
		      	continue;
		      else if(ret < 0){
		      	  perror("poll()");
		          pthread_exit(NULL);
		      }
		      else{
		      	break;
		      	runnable = 1;
		      }
		    }
		}

	  /*
	  do_debug("Avg poll : %f (us)\n", avg_poll/n_poll);
      do_debug("Avg send packet : %f (us)\n", avg_sendto/n_sendto);
      do_debug("Avg flush : %f (us)\n", avg_flush/n_flush);
      do_debug("Avg get sock : %f (us)\n", avg_get/n_get);
      do_debug("Avg recv : %f (us)\n", avg_recv/n_recv);
      do_debug("#####################################################\n"); 
      */
    
	} //end of while
	

	cleanup(&routing_list);
	for(i = 0 ; i < n_intfs; i++){
		free(intf_names[i]);
	}

	free(poll_set);

	return 0;

}