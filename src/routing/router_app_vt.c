

#include "includes.h"
#include "utils.h"



pthread_t tid;
llist routing_list;
pthread_mutex_t lock;


#ifdef APP_VT
hashmap func_times_us;
extern void init_func_times(void);
extern void pipe_send(int * fd, char * msg);
extern int pipe_read(int *fd);
extern void init_func_times(void);

#endif



//From TK. Returns -1 if experiment has to be stopped.
int get_time_to_advance(){

	int per_round_advance = 100; 
	return per_round_advance;

}

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
	int tap_fd;
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
	fd_set rd_set;
	int tap2net = 0;
	char routing_file_path[100];
	thread_args args;
	int j  = 0;
	int per_round_advance = 100; //100_us
	int runnable = 0;
	int n_intfs_orig;

	FD_ZERO(&rd_set);

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
	}

	do_debug("Starting up. Switch name: %s\n", switch_name);
	do_debug("Active interfaces:\n");

	for(i = 0; i < n_intfs; i++){
		do_debug("-> %s\n", intf_names[i]);
	}


	i = 0;
	llist_init(&routing_list);
	populate_routing_list(routing_file_path);
	#ifdef APP_VT
	init_func_times();
	#endif

	do_debug("Function times initialized !\n");

  
	args.thread_no = 0;
	args.is_blocked = 0;
	args.stop = 0;

	strcpy(args.bind_if, intf_names[0]);
	do_debug("Starting Thread: %d\n", args.thread_no);

	args.intf_names = intf_names;
	args.n_intfs = n_intfs;

	do_debug("Starting Thread: %d. Opening pipes\n",0);
	#ifdef APP_VT
	open_pipe(args.fds_main_to_thread);
	open_pipe(args.fds_thread_to_main);
	#endif
	time_elapsed = 0;
	pthread_create(&tid, NULL, process_thread, &args);
  
  	do_debug("Started all threads\n");


	#ifdef APP_VT
	while(1){



		per_round_advance = get_time_to_advance();
		if(per_round_advance == -1){
			// STOPPING EXPERIMENT
			args.stop = 1;
			do_debug("MAIN: Instructing Thread: %d to Stop. Time elapsed = %d\n", i , time_elapsed);
			pipe_send(args.fds_main_to_thread, THREAD_EXITING);
			break;
		}
		else{

		  	if(!args.is_blocked){
			  	do_debug("MAIN: Instructing Thread: %d to run for 10ms. Time elapsed = %d\n", i, time_elapsed);
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
	#else

	do_debug("Waiting for 1 sec for threads to run\n");
	usleep(1000000);
	args.stop = 1;

	#endif


	do_debug("MAIN: Waiting for all threads to exit\n");
	pthread_join(tid,NULL);


	cleanup(&routing_list);
	for(i = 0 ; i < n_intfs; i++){
		free(intf_names[i]);
	}

	#ifdef APP_VT
	hmap_destroy(&func_times_us);
	#endif

	return 0;

}