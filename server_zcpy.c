#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include "server.h"
#include "command.h"
#include "queue.h"
#include "request.h"
#include "priority_queue.h"
#include "LR_inter.h"

struct mmap_sock_hdr {
	__u32 prod_ptr;
	__u32 consumer_ptr;
};

//struct mmap_sock_hdr *tx, *rx;
//void *tx_base, *rx_base;

struct s_mmap_req {
	size_t size;
} mmap_req;



int clnt_cnt = 0;
int clnt_socks[MAX_CLNT];
pthread_mutex_t clnt_mutx = PTHREAD_MUTEX_INITIALIZER;
queue *req_queue;

int init_server(int port) {	// socket, bind, listen
	int serv_sock;
	struct sockaddr_in serv_adr;
//	size_t size;

	pthread_mutex_init(&clnt_mutx,NULL); 	// init mutx
//	req_queue = create_queue(); 		// init queque

	if ( (serv_sock = socket(PF_INET, SOCK_STREAM, 0))  == -1 ) {
		printf("socket() failed\n");
		return -1;
	}
/*	
	size = 1 >> 19 - sizeof(struct mmap_sock_hdr);
	mmap_req.size = size;
	setsockopt(serv_sock, SOL_SOCKET, TX_RING, (char*)&mmap_req, sizeof(struct s_mmap_req));
	tx = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, serv_sock, 0);
	tx_base = (void*)tx[1];

	size = 1 >> 19 - sizeof(struct mmap_sock_hdr);
	mmap_req.size = size;
	setsockopt(serv_sock, SOL_SOCKET, RX_RING, (char*)&mmap_req, sizeof(struct s_mmap_req));
	rx = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, serv_sock, 0);
	rx_base = (void*)tx[1];
*/
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(port);

	if( bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1 ) {
		printf("bind() failed\n");
		return -1;
	}

	if( listen(serv_sock, 100) == -1 ) {
		printf("listen() failed\n");
		return -1;
	}

	return serv_sock;
}

int execute_server(int serv_sock) {
	struct sockaddr_in clnt_adr;
	socklen_t clnt_adr_sz;
	pthread_t t_id;
	//pthread_create(&t_id, NULL, proc_req, NULL);
	//pthread_detach(t_id);
	while(1) {
		int *clnt_sock = (int*)malloc(sizeof(int));
		clnt_adr_sz = sizeof(clnt_adr);
		if ( (*clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz) ) == -1 ) {
			printf("accept() failed\n");
			return -1;
		}
		
		pthread_mutex_lock(&clnt_mutx);
		clnt_socks[clnt_cnt++] = *clnt_sock;
		pthread_mutex_unlock(&clnt_mutx);

		pthread_create(&t_id, NULL, work_on_accept, (void*)clnt_sock);
		pthread_detach(t_id);
		#ifdef DEBUG
		printf("CLNT IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
		#endif
	}
	close(serv_sock);
	return 0;
}

void* work_on_accept(void *arg) {
	int clnt_sock = *((int*)arg);
	int str_len=0, i;
	unsigned int start = 0, end = 0;
	int tx_idx, 
	size_t size;
	mmap_req m_req;
	struct mmap_sock_hdr *tx = {0,0}, *rx = {0,0};
	void *tx_base, *rx_base;
	
	size = 1 << 19 - sizeof(struct mmap_sock_hdr);
	m_req.size = size;
	setsockopt(clnt_sock, SOL_SOCKET, TX_RING, (char*)&m_req, sizeof(struct s_mmap_req));
	tx = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, clnt_sock, 0);
	tx_base = (void*)tx[1];
	

	size = 1 << 19 - sizeof(struct mmap_sock_hdr);
	m_req.size = size;
	setsockopt(clnt_sock, SOL_SOCKET, RX_RING, (char*)&m_req, sizeof(struct s_mmap_req));
	rx = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, clnt_sock, 0);
	rx_base = (void*)rx[1];

	pthread_mutex_t read_async_mutx = PTHREAD_MUTEX_INITIALIZER;
//	char msg[BUF_SIZE];
	char *msg = (char*)malloc(100000);
	//static uint64_t seq;
	int exit_val;
	//seq++;
	req_t *req = NULL;
	heap_t *read_async_PQ = create_PQ();
	//printf("clnt seq : %"PRIu64" clnt_socket : %d\n",seq,clnt_sock);
	while( (str_len = read(clnt_sock, rx_base[rx->prod_ptr], 100000)) != 0 ) {
		rx->prod_ptr = (rx->prod_ptr + str_len) % m_req.size;
		
		if ( str_len == -1 ) {
			
		}
		//write(clnt_sock, "+OK\r\n", 5);
		/*
		if( (exit_val = ParseAndInsertCommand(clnt_sock, msg, str_len, seq)) != -2 ) {
			printf("parse failed %d\n", exit_val);
		}
		*/
//printf("MSG\n%s\n",msg);
		if ( GetRequest(clnt_sock, rx_base[rx->comsume_ptr], str_len, req, &start, &end, read_async_PQ, &read_async_mutx) == -1 ) {
//			memset(msg, 0, str_len);
			continue;
		}
		rx->consume_ptr = rx->prod_ptr;
//		memset(msg, 0, str_len);
		req = NULL;
		//make_req(req);
		//printf("str_len %d\n",str_len);
		//if( req != NULL ) printf("wonk keyword valid: %d\ntype valid: %d\nkey valid: %d\nvalue valid: %d\n",req->keyword_info->valid,req->type_info->valid,req->key_info->valid,req->value_info->valid);
	}
	//printf("ByeBye\n");
	pthread_mutex_lock(&clnt_mutx);
	for(i=0; i<clnt_cnt; i++) {
		if( clnt_sock == clnt_socks[i] ) {
			while( i++ < clnt_socks[i] )
				clnt_socks[i] = clnt_socks[i+1];
			break;
		}
	}
	clnt_cnt--;
	//close(clnt_sock);
	pthread_mutex_unlock(&clnt_mutx);
	destroy_PQ(read_async_PQ);
	free((int*)arg);
	return NULL;
}

int main(int argc, char* argv[]) {
	int serv_sock;
	serv_sock = init_server(atoi(argv[1]));
//	lr_inter_init();
	execute_server(serv_sock);
	return 0;
}
