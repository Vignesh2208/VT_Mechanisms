current_dir=$(shell pwd)
EXTRA_CFLAGS += 
SUBDIR= $(shell pwd)
GCC:=gcc
RM:=rm

all:

	@gcc -I${current_dir}/utils -D ROUTER_APP_VT utils/linkedlist.c utils/hashmap.c router_app_vt.c utils.c pkt_info.c -o router_app_vt -lpthread
	@gcc -I${current_dir}/utils utils/linkedlist.c utils/hashmap.c router_n.c utils.c pkt_info.c -o router_n -lpthread
clean:
	rm -f router_app_vt
	rm -f router_n
	rm -f router


