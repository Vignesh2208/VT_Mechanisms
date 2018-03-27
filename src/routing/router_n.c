#include "includes.h"
#include "utils.h"

llist routing_list;
pthread_mutex_t lock;



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
  	int data_size;
 	struct sockaddr_in rcvaddr;
	int len;
	int socks[N_MAX_TAPS];
	char buf[BUFSIZE];
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
	do_debug("Active interfaces:\n");

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

	for(i = 0; i < n_intfs; i++){
		do_debug("-> %s\n", intf_names[i]);
	}

 
  	while(1) {


  		repeat:
  		for(i = 0; i < n_intfs; i++){
	  
	  	  //do_debug("Sock[%d] = %d\n", i, socks[i]);
		  poll_set[i].fd = socks[i];
		  poll_set[i].events = POLLIN;
		}

   
		ret = poll(poll_set, n_intfs, 0);

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

		  sock = socks[i];
		  if((poll_set[i].revents & POLLIN) && (poll_set[i].fd == sock)) {
		    runnable = 1;

		    flush_buffer(buf,BUFSIZE);

		    data_size = recvfrom(sock,buf,BUFSIZE,0,(struct sockaddr*)&rcvaddr,&len);

		    if(data_size > 0){
		        dst_sock_idx = get_dst_sock_idx(buf, n_intfs, intf_names);

		        if(dst_sock_idx != -1){
		          dst_sock = socks[dst_sock_idx];
		          do_debug("Recv sock: %d, Dst sock idx = %d, dst_sock = %d\n", sock, dst_sock_idx, dst_sock);

		          send_packet(buf, data_size, dst_sock, intf_names[dst_sock_idx]);
		        }
		        else{
		        	goto repeat;
		        }
		    }

		    //poll_set[i].revents = 0;

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
		      else
		      	break;
		    }
		}
    
	} //end of while
	

	cleanup(&routing_list);
	for(i = 0 ; i < n_intfs; i++){
		free(intf_names[i]);
	}

	free(poll_set);

	return 0;

}