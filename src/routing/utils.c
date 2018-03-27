#include "utils.h"
#include "pkt_info.h"


extern llist routing_list;
extern int str_hash(char * s);

#define MY_DEST_MAC0  0x00
#define MY_DEST_MAC1  0x00
#define MY_DEST_MAC2  0x00
#define MY_DEST_MAC3  0x00
#define MY_DEST_MAC4  0x00
#define MY_DEST_MAC5  0x00

#ifdef APP_VT

int RECVFROM_US = 1000;
int RAWSOCKET_US = 1000;
int SETPROMISC_US = 1000;
int INTF_NAME_TO_SOCK_IDX_US = 1000;
int GET_DST_SOCK_IDX_US = 1000;
int SEND_PACKET_US = 1000;
int SELECT_US = 1000;
int FLUSH_BUFFER_US = 1000;

extern hashmap func_times_us;
#endif



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

  return str_to_i(buf);
}


int str_to_i(char *s)
{
        int i,n;
        n = 0;
        for(i = 0; *(s+i) >= '0' && *(s+i) <= '9'; i++)
                n = 10*n + *(s+i) - '0';
        return n;
}



int rawSocket(int thread_no, char * bind_intf)
{
    int sock;
    struct ifreq ifr;
    int one = 1;
    const int *val = &one;
    struct sockaddr_ll sock_address;
 

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), bind_intf);

    sock=socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL));//from Ethernet
    //sock=socket(AF_INET,SOCK_RAW,IPPROTO_ICMP);
    if(sock<0)
    {
        do_debug("THREAD: %d. Create raw socket failed:%s\n",thread_no, strerror(errno));
        pthread_exit(NULL);
    }
    
    do_debug("THREAD %d. Raw socket created successfully!\n",thread_no);

    /*
    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) < 0) {

        do_debug("THREAD: %d. Failed to bind socket to interface: %s\n",thread_no,bind_intf);
        pthread_exit(NULL);
    }
    */

    memset(&sock_address, 0, sizeof(sock_address));
    sock_address.sll_family = PF_PACKET;
    sock_address.sll_protocol = htons(ETH_P_ALL);
    sock_address.sll_ifindex = if_nametoindex(bind_intf);
    if (bind(sock, (struct sockaddr*) &sock_address, sizeof(sock_address)) < 0) {
      perror("bind failed\n");
      close(sock);
      pthread_exit(NULL);
    }

    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    /*if(setsockopt(sock, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0)
    {
      perror("setsockopt() error");
      pthread_exit(NULL);
    }
    else
      printf("setsockopt() is OK.\n");
    */


    return sock;
}


int setPromisc(int thread_no, char *enterface, int *sock)
{
    struct ifreq ifr;
    strcpy(ifr.ifr_name, enterface);
    ifr.ifr_flags=IFF_UP|IFF_PROMISC|IFF_BROADCAST|IFF_RUNNING;
  
    if(ioctl(*sock,SIOCSIFFLAGS,&ifr)==-1)
    {
        do_debug("THREAD %d. Set %s to promisc model failed\n", thread_no, enterface); 
        pthread_exit(NULL);
    }
    printf("set '%s' to promisc successed!\n",enterface);
    return SUCCESS;
}

/**************************************************************************
 * tun_alloc: allocates or reconnects to a tun/tap device. The caller     *
 *            must reserve enough space in *dev.                          *
 **************************************************************************/
int tun_alloc(char *dev, int flags) {

  struct ifreq ifr;
  int fd, err;
  char *clonedev = "/dev/net/tun";

  if( (fd = open(clonedev , O_RDWR)) < 0 ) {
    perror("Opening /dev/net/tun");
    return fd;
  }

  memset(&ifr, 0, sizeof(ifr));

  ifr.ifr_flags = flags;

  if (*dev) {
    strncpy(ifr.ifr_name, dev, strlen(dev));
  }

  printf("Opening Tun device: %s\n", ifr.ifr_name);

  if( (err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0 ) {
    printf("Failed to creat tun device (errno is %d)\n",
             errno);
    perror("ioctl(TUNSETIFF)");
    close(fd);
    return err;
  }

  strcpy(dev, ifr.ifr_name);

  return fd;
}

/**************************************************************************
 * cread: read routine that checks for errors and exits if an error is    *
 *        returned.                                                       *
 **************************************************************************/
int cread(int fd, char *buf, int n){
  
  int nread;

  if((nread=read(fd, buf, n)) < 0){
    perror("Reading data");
    exit(1);
  }
  return nread;
}

/**************************************************************************
 * cwrite: write routine that checks for errors and exits if an error is  *
 *         returned.                                                      *
 **************************************************************************/
int cwrite(int fd, char *buf, int n){
  
  int nwrite;

  if((nwrite=write(fd, buf, n)) < 0){
    perror("Writing data");
    exit(1);
  }
  return nwrite;
}

/**************************************************************************
 * read_n: ensures we read exactly n bytes, and puts them into "buf".     *
 *         (unless EOF, of course)                                        *
 **************************************************************************/
int read_n(int fd, char *buf, int n) {

  int nread, left = n;

  while(left > 0) {
    if ((nread = cread(fd, buf, left)) == 0){
      return 0 ;      
    }else {
      left -= nread;
      buf += nread;
    }
  }
  return n;  
}

/**************************************************************************
 * do_debug: prints debugging stuff (doh!)                                *
 **************************************************************************/
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


void flush_buffer (char * buf, int size){
  int i = 0;
  for(i = 0 ; i < size; i++)
    buf[i] = '\0';
}


void print_routing_list(llist * routing_list) {

  llist_elem * head = routing_list->head;
  routing_entry * entry;
  do_debug("Active routing table:\n");
  while(head != NULL) {
    entry = (routing_entry*) head->item;
    do_debug("-> IP:%s, IFACE:%s\n",entry->ip_address, entry->output_if);
    head = head->next;
  }
  do_debug("\n");
}

int populate_routing_list(char * f_name){

  FILE * fp;
  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  char tmp[BUFSIZE];
  int i = 0;
  int if_idx;

  routing_entry * new_entry = NULL;

  fp = fopen(f_name, "r");
  if (fp == NULL){
    printf("Failed to open file: %s\n", f_name);
    exit(FAIL);
  }

  while ((read = getline(&line, &len, fp)) != -1) {

      if(len > 1) {
        //do_debug("Retrieved line of length %zu :\n", read);
        //do_debug("%s\n", line);

        new_entry = (routing_entry *)malloc(sizeof(routing_entry));
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

        strcpy(new_entry->ip_address, tmp);
        strcpy(new_entry->output_if, tmp + if_idx);

        llist_append(&routing_list, new_entry);

      }
  }

  print_routing_list(&routing_list);

  return SUCCESS;



}

void cleanup(llist * routing_list){

  routing_entry * entry;

  entry = llist_pop(routing_list);
  if(!entry){
    free(entry);
  }
  else{
    llist_destroy(routing_list);
  }

}


int intf_name_to_sock_idx(char * intf, char * intf_names, int n_intfs){
  int i = 0;
  for(i = 0 ; i < n_intfs; i++){
      if(strcmp(intf, intf_names[i]) == 0)
        return i;
  }

  return FAIL;
}



int get_dst_sock_idx(char * pkt, int n_intfs, char * intf_names[N_MAX_TAPS]){

  unsigned short iphdrlen;
  struct sockaddr_in source,dest;

  char src_IP[IP_ADDR_SIZE];
  char dst_IP[IP_ADDR_SIZE];
  char * dst_intf = NULL;
  int i;

  llist * routes = &routing_list;

       
  struct iphdr *iph = (struct iphdr *)(pkt  + sizeof(struct ethhdr) );
  //struct iphdr *iph = (struct iphdr *)pkt;
  iphdrlen =iph->ihl*4;
   
  memset(&source, 0, sizeof(source));
  source.sin_addr.s_addr = iph->saddr;
   
  memset(&dest, 0, sizeof(dest));
  dest.sin_addr.s_addr = iph->daddr;

  flush_buffer(src_IP, IP_ADDR_SIZE);
  flush_buffer(dst_IP, IP_ADDR_SIZE);

  sprintf(src_IP, "%s", inet_ntoa(source.sin_addr));
  sprintf(dst_IP, "%s", inet_ntoa(dest.sin_addr));



  llist_elem * head = routes->head;
  routing_entry * entry;
  while(head != NULL) {
    entry = (routing_entry*) head->item;
    if(strcmp(entry->ip_address, dst_IP) == 0){
      dst_intf = entry->output_if;
      do_debug("THREAD. ROUTING FOR DST IP: %s, SRC_IP: %s, OUTPUT INTERFACE: %s\n", dst_IP, src_IP, dst_intf);
    }
    head = head->next;
  }
  

  if(dst_intf == NULL){
      do_debug("THREAD. ROUTING FOR DST IP: %s, SRC_IP: %s, NO ENTRY FOUND. DROPPING\n", dst_IP, src_IP);
      return -1;
  }

  for(i = 0 ; i < n_intfs; i++){
    if(strcmp(dst_intf, intf_names[i]) == 0)
      return i;
  }


  return 0;
}


void get_interface_MAC(char interface_name[IFNAMSIZ], int sockfd)
{
        struct ifreq if_mac;
        memset(&if_mac, 0, sizeof(struct ifreq));
        strncpy(if_mac.ifr_name, interface_name, IFNAMSIZ-1);
        if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0)
                perror("SIOCGIFHWADDR");
}

int send_packet(char * pkt, int size,  int sock, char * intf_name){
  //possibly change source mac here
  struct sockaddr_in sin, din;
  struct ifreq if_idx;
  struct ifreq if_mac;

  
  struct sockaddr_ll socket_address;

  //char sendBuf[BUFSIZE];
  //flush_buffer(sendBuf, BUFSIZE);

  //memcpy(sendBuf, pkt, size);

  char *sendBuf = pkt;

  struct ethhdr *eth = (struct ethhdr *)sendBuf;
  struct iphdr *iph = (struct iphdr *) (sendBuf + sizeof(struct ethhdr));
  

  /*if(eth->h_proto != ){
    printf("NO IP packet.");
    return SUCCESS;
  }*/
  
  //struct iphdr *iph = (struct iphdr *)pkt;

  /* Get the index of the interface to send on */
  memset(&if_idx, 0, sizeof(struct ifreq));
  strncpy(if_idx.ifr_name, intf_name, IFNAMSIZ-1);
  if (ioctl(sock, SIOCGIFINDEX, &if_idx) < 0)
      perror("SIOCGIFINDEX");


  /* Get the MAC address of the interface to send on */
  memset(&if_mac, 0, sizeof(struct ifreq));
  strncpy(if_mac.ifr_name, intf_name, IFNAMSIZ-1);
  if (ioctl(sock, SIOCGIFHWADDR, &if_mac) < 0)
      perror("SIOCGIFHWADDR");
  else{

  
  /* Ethernet header */

  /* 
  eth->h_source[0] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0];
  eth->h_source[1] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1];
  eth->h_source[2] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2];
  eth->h_source[3] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3];
  eth->h_source[4] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4];
  eth->h_source[5] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5];
  */
    socket_address.sll_addr[0] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0];
    socket_address.sll_addr[1] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1];
    socket_address.sll_addr[2] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2];
    socket_address.sll_addr[3] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3];
    socket_address.sll_addr[4] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4];
    socket_address.sll_addr[5] = ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5];


  }
  

  /*eth->h_dest[0] = MY_DEST_MAC0;
  eth->h_dest[1] = MY_DEST_MAC1;
  eth->h_dest[2] = MY_DEST_MAC2;
  eth->h_dest[3] = MY_DEST_MAC3;
  eth->h_dest[4] = MY_DEST_MAC4;
  eth->h_dest[5] = MY_DEST_MAC5;
  */

  socket_address.sll_ifindex = if_idx.ifr_ifindex;
  /* Address length*/
  socket_address.sll_halen = ETH_ALEN;
  /* Destination MAC */

  /*
  socket_address.sll_addr[0] = eth->h_dest[0] ;
  socket_address.sll_addr[1] = eth->h_dest[1] ;
  socket_address.sll_addr[2] = eth->h_dest[2] ;
  socket_address.sll_addr[3] = eth->h_dest[3] ;
  socket_address.sll_addr[4] = eth->h_dest[4] ;
  socket_address.sll_addr[5] = eth->h_dest[5] ;
  */





  do_debug("Sending to : %x:%x:%x:%x:%x:%x\n", eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], eth->h_dest[3], eth->h_dest[4], eth->h_dest[5] );

  /*
  struct iphdr *iph = (struct iphdr *)(pkt  + sizeof(struct ethhdr) );

  sin.sin_addr.s_addr = iph->saddr;
  din.sin_addr.s_addr = iph->daddr;

  
  if(sendto(sock, pkt, size, 0, (struct sockaddr *)&din, sizeof(din)) < 0){
    do_debug("sendto error !\n");
  }
  else{
    do_debug("THREAD. Send to success\n");
  }
  */

  if (sendto(sock, sendBuf, size, 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0)
      perror("THREAD. Send to failed!\n");
  else
      do_debug("THREAD. Send to success! Dst_Intf: %s. H proto: %x\n", intf_name, eth->h_proto);

  
  return SUCCESS;
}

#ifdef APP_VT
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

int update_stub_blocking(int * curr_round_time_left, int *thread_to_main_fds, int * main_to_thread_fds){
  pipe_send(thread_to_main_fds, THREAD_BLOCKED);
  *curr_round_time_left = pipe_read(main_to_thread_fds);

  if(*curr_round_time_left < 0)
      return THREAD_EXITING;

    return SUCCESS;

}


#endif


void finish_thread(int *socks, int n_intfs){
  int i = 0;
  do_debug("Finishing thread\n");
  for(i = 0; i < n_intfs; i++){
    close(socks[i]);
  }

  pthread_exit(NULL);
}

void interface_thread(void * args){

  thread_args * arg = (thread_args *) args;
  if(!arg){
    do_debug("THREAD did not get any argument\n");
    pthread_exit(NULL);
  }

  int thread_no = arg->thread_no;
  char * bind_intf = arg->bind_if;
  int n_intfs = arg->n_intfs;

  do_debug("THREAD: %d. Starting ..\n", thread_no);

  int sock = rawSocket(thread_no, bind_intf);
  int socks[N_MAX_TAPS];
  int maxfd = sock + 1;
  int ret = 0;
  struct sockaddr rcvaddr;
  int len;
  int i  = 0;
  char buf[BUFSIZE];
  int dst_sock_idx;
  int dst_sock ;
  int exp_run_time_us = 100000;
  int exp_curr_round_time_us = 0;
  int total_exec_time = 0;
  struct pollfd poll_set[1];
  int data_size;

  poll_set[0].fd = sock;
  poll_set[0].events = POLLIN;

  setPromisc(thread_no, bind_intf, &sock);

  for(i = 0; i < n_intfs; i++){
    if(strcmp(bind_intf, arg->intf_names[i]) == 0){
      socks[i] = sock;
    }
    else{
      socks[i] = rawSocket(thread_no, arg->intf_names[i]);
    }

  }


  do_debug("Thread: %d. Opened all sockets. Active interfaces:\n", thread_no);

  for(i = 0; i < n_intfs; i++){
    do_debug("-> %s\n", arg->intf_names[i]);
  }

  fd_set rd_set;

  #ifdef APP_VT
    if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, NULL) < 0){
        finish_thread(socks, n_intfs);
    }
  #endif

  while(1) {

    if(arg->stop){
      finish_thread(socks, n_intfs);
    }


    FD_ZERO(&rd_set);
    FD_SET(sock, &rd_set); 

    flush_buffer(buf,BUFSIZE);

    //ret = select(maxfd + 1, &rd_set, NULL, NULL, NULL);
    ret = poll(poll_set, 1, 0);

    #ifdef APP_VT
    if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "poll" ) < 0 || arg->stop){
        finish_thread(socks, n_intfs);
    }
    #endif

    if (ret < 0 && errno == EINTR){
      continue;
    }

    if (ret < 0) {
      perror("poll()");
      pthread_exit(NULL);
    }

    if(poll_set[0].revents & POLLIN && poll_set[0].fd == sock) {

      //non blocking. no need to notify main thread through pipe.      
      data_size = recvfrom(sock,buf,sizeof(buf),0,(struct sockaddr*)&rcvaddr,&len);
      // no need to block on pipe afer this return.

      #ifdef APP_VT
      if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "recvfrom") < 0 || arg->stop){
          finish_thread(socks, n_intfs);
      }
      #endif

    }
    else{
      do_debug("THREAD: %d. Need to Block\n");

      /*#ifdef APP_VT
      if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "recvfrom") < 0 || arg->stop){
          finish_thread(socks, n_intfs);
      }
      #endif
      */

      

        #ifdef APP_VT
        //blocking recvfrom. need to notify main thread about going to block through pipe.
        arg->is_blocked = 1;
        exp_curr_round_time_us = 0;
        pipe_send(arg->fds_thread_to_main, THREAD_BLOCKED);
        #endif

        data_size = recvfrom(sock,buf,sizeof(buf),0,(struct sockaddr*)&rcvaddr,&len);
        //need to block on pipe afer this return.

        #ifdef APP_VT
        //blocking recvfrom. need to notify main thread about going to block through pipe.
        arg->is_blocked = 0;
        exp_curr_round_time_us = pipe_read(arg->fds_main_to_thread);
        if(exp_curr_round_time_us <= 0 || arg->stop)
            finish_thread(socks, n_intfs);
        #endif
      
        
    }



    if(data_size > 0){
      dst_sock_idx = get_dst_sock_idx(buf, arg->n_intfs, arg->intf_names);

      //ProcessPacket(buf, data_size);


      #ifdef APP_VT
      if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "get_dst_sock_idx") < 0 || arg->stop){
            finish_thread(socks, n_intfs);
        }
      #endif

      if(dst_sock_idx != -1){
        dst_sock = socks[dst_sock_idx];
        send_packet(buf, data_size, dst_sock, arg->intf_names[dst_sock_idx]);

        #ifdef APP_VT
        if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "send_packet") < 0 || arg->stop){
              finish_thread(socks, n_intfs);
        }
        #endif
      }

    }


  }
}



void process_thread(void * args){

  thread_args * arg = (thread_args *) args;
  if(!arg){
    do_debug("THREAD did not get any argument\n");
    pthread_exit(NULL);
  }

  int thread_no = arg->thread_no;
  char * bind_intf = arg->bind_if;
  int n_intfs = arg->n_intfs;

  do_debug("THREAD: %d. Starting ..\n", thread_no);


  int socks[N_MAX_TAPS];
  int maxfd = -1;
  int ret = 0;
  struct sockaddr rcvaddr;
  int len;
  int i  = 0;
  char buf[BUFSIZE];
  int dst_sock_idx;
  int dst_sock ;
  int exp_run_time_us = 100000;
  int exp_curr_round_time_us = 0;
  int total_exec_time = 0;
  struct pollfd * poll_set;
  int data_size;
  int runnable;


  poll_set = malloc(n_intfs*sizeof(struct pollfd));

  if(!poll_set){
    do_debug("Poll set Alloc NOMEM\n");
    pthread_exit(NULL);
  }
  

  

  for(i = 0; i < n_intfs; i++){
      socks[i] = rawSocket(thread_no, arg->intf_names[i]);
      setPromisc(thread_no, arg->intf_names[i], &socks[i]);
      poll_set[i].fd = socks[i];
      poll_set[i].events = POLLIN;
  }


  do_debug("Thread: %d. Opened all sockets. Active interfaces:\n", thread_no);

  for(i = 0; i < n_intfs; i++){
    do_debug("-> %s\n", arg->intf_names[i]);
  }

  fd_set rd_set;

  #ifdef APP_VT
    if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, NULL) < 0){
      free(poll_set);
      finish_thread(socks, n_intfs);

    }
  #endif

  while(1) {

    if(arg->stop){
      free(poll_set);
      finish_thread(socks, n_intfs);
      
    }

   
    ret = poll(poll_set, n_intfs, 0);

    #ifdef APP_VT
    if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "poll" ) < 0 || arg->stop){
        free(poll_set);
        finish_thread(socks, n_intfs);
    }
    #endif

    if (ret < 0 && errno == EINTR){
      continue;
    }

    if (ret < 0) {
      perror("poll()");
      pthread_exit(NULL);
    }

    runnable = 0;
    for(i = 0; i < n_intfs; i++){
      if(poll_set[i].revents & POLLIN && poll_set[i].fd == socks[i]) {
        runnable = 1;

        flush_buffer(buf,BUFSIZE);

        #ifdef APP_VT
        if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "flush_buffer" ) < 0 || arg->stop){
            free(poll_set);
            finish_thread(socks, n_intfs);
        }
        #endif

        //non blocking. no need to notify main thread through pipe.      
        data_size = recvfrom(socks[i],buf,sizeof(buf),0,(struct sockaddr*)&rcvaddr,&len);
        // no need to block on pipe afer this return.

        #ifdef APP_VT
        if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "recvfrom") < 0 || arg->stop){
            free(poll_set);
            finish_thread(socks, n_intfs);
        }
        #endif


        if(data_size > 0){
            dst_sock_idx = get_dst_sock_idx(buf, arg->n_intfs, arg->intf_names);

            //ProcessPacket(buf, data_size);
            #ifdef APP_VT
            if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "get_dst_sock_idx") < 0 || arg->stop){
                  free(poll_set);
                  finish_thread(socks, n_intfs);
              }
            #endif

            if(dst_sock_idx != -1){
              dst_sock = socks[dst_sock_idx];
              send_packet(buf, data_size, dst_sock, arg->intf_names[dst_sock_idx]);

              #ifdef APP_VT
              if(update_stub(&exp_curr_round_time_us, arg->fds_thread_to_main, arg->fds_main_to_thread, "send_packet") < 0 || arg->stop){
                    free(poll_set);
                    finish_thread(socks, n_intfs);
              }
              #endif
            }
        }

      }

    } //end of for


    if(runnable == 0){
        // going to block indefinitely
        ret = 0;
        do_debug("THREAD: %d. Need to Block\n");

        #ifdef APP_VT
        //blocking recvfrom. need to notify main thread about going to block through pipe.
        arg->is_blocked = 1;
        exp_curr_round_time_us = 0;
        pipe_send(arg->fds_thread_to_main, THREAD_BLOCKED);
        #endif

        while(ret == 0){
          ret = poll(poll_set, n_intfs, -1);
          
          if(ret < 0){
              perror("poll()");
              pthread_exit(NULL);
          }
        
        }

        #ifdef APP_VT
        //blocking recvfrom. need to notify main thread about resume after blocking
        arg->is_blocked = 0;
        exp_curr_round_time_us = pipe_read(arg->fds_main_to_thread);
        if(exp_curr_round_time_us <= 0 || arg->stop){
          free(poll_set);
          finish_thread(socks, n_intfs);
        }
        #endif
      

    }


    
  } //end of while
}



#ifdef APP_VT
void init_func_times(){

  hmap_init(&func_times_us,1000);

  hmap_put_abs(&func_times_us, abs(str_hash("recvfrom")), &RECVFROM_US);
  hmap_put_abs(&func_times_us, abs(str_hash("rawSocket")), &RAWSOCKET_US);
  hmap_put_abs(&func_times_us, abs(str_hash("setPromisc")), &SETPROMISC_US);
  hmap_put_abs(&func_times_us, abs(str_hash("intf_name_to_sock_idx")), &INTF_NAME_TO_SOCK_IDX_US);
  hmap_put_abs(&func_times_us, abs(str_hash("get_dst_sock_idx")), &GET_DST_SOCK_IDX_US);
  hmap_put_abs(&func_times_us, abs(str_hash("send_packet")), &SEND_PACKET_US);
  hmap_put_abs(&func_times_us, abs(str_hash("poll")), &SELECT_US);
  hmap_put_abs(&func_times_us, abs(str_hash("flush_buffer")), &FLUSH_BUFFER_US);
  
  do_debug("Hmap initialized\n");


}
#endif


