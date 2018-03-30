current_dir=$(shell pwd)
EXTRA_CFLAGS += 
SUBDIR= $(shell pwd)
GCC:=gcc
RM:=rm

all:

	@gcc -I${current_dir}/../utils  ../utils/linkedlist.c udp_client_n.c -o udp_client_n
	@gcc -I${current_dir}/../utils  pkt_info.c udp_server_n.c -o udp_server_n
	@gcc  client_n.c -o client_n -lm
	@gcc  server_n.c -o server_n

	@gcc -I${current_dir}/../utils  ../utils/linkedlist.c ../utils/hashmap.c client_n_vt.c -o client_vt -lpthread -lm
	@gcc -I${current_dir}/../utils  ../utils/linkedlist.c ../utils/hashmap.c server_n_vt.c -o server_vt -lpthread -lm
clean:
	@rm -f udp_client_n
	@rm -f udp_server_n
	@rm -f udp_client_vt
	@rm -f udp_server_vt
	@rm -f client_n
	@rm -f server_n
	@rm -f client_vt
	@rm -f server_vt