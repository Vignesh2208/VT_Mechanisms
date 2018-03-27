current_dir=$(shell pwd)
EXTRA_CFLAGS += 
SUBDIR= $(shell pwd)
GCC:=gcc
RM:=rm

all:

	@gcc -I${current_dir}/../utils  ../utils/linkedlist.c udp_client_n.c -o udp_client_n
	@gcc -I${current_dir}/../utils  pkt_info.c udp_server_n.c -o udp_server_n
clean:
	@rm -f udp_client_n
	@rm -f udp_server_n
	@rm -f udp_client_vt
	@rm -f udp_server_vt