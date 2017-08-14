#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <poll.h>
#include <arpa/inet.h>
#include <unordered_map>
#include "command.h"
#include "request.h"
#include "LR_inter.h"
#include "lockfreeq.h"
using namespace std;


#define POLL_SIZE 5000
#define LISTEN_QUEUE 5
#define QSIZE 2048
#define BUF_SIZE 1024 * 1024 / 10

typedef struct read_data {
	int fd;
	char* buf;
	ssize_t nread;
}read_data;

unordered_map<int,req_t*> req_map;
struct pollfd poll_set[POLL_SIZE];
int numfds = 0;
spsc_bounded_queue_t<read_data*> *req_q;


int init_server(int port) {	// socket, bind, listen
	int serv_sock;
	struct sockaddr_in serv_adr;

	if ( (serv_sock = socket(PF_INET, SOCK_STREAM, 0))  == -1 ) {
		printf("socket() failed\n");
		return -1;
	}

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

	poll_set[0].fd = serv_sock;
	poll_set[0].events = POLLIN;
	numfds++;

	return serv_sock;
}

void execute_polling(int sock) {
	int serv_sock = sock;
	int clnt_sock;
	struct sockaddr_in clnt_addr;
	socklen_t clnt_adr_sz;
	int result;
	read_data *rdata;
	while(1) {
		char *buf;
		int fd_index;
		ssize_t nread = 0;
		//printf("Waiting for client (%d total)...\n", numfds);
		result = poll(poll_set, numfds, 10000);
		if ( result < 0 ) printf("poll failed()");
		for(fd_index = 0; fd_index < numfds; fd_index++)
		{
			if( poll_set[fd_index].revents & (POLLIN | POLLERR) ) {
				if(poll_set[fd_index].fd == serv_sock) {
					clnt_adr_sz = sizeof(clnt_addr);
					clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_adr_sz);

					poll_set[numfds].fd = clnt_sock;
					poll_set[numfds].events = POLLIN;
					numfds++;

					printf("Adding client on fd %d\n", clnt_sock);
				}
				else {
					//while(!nread && ioctl(poll_set[fd_index].fd, FIONREAD, &nread) >= 0);
					ioctl(poll_set[fd_index].fd, FIONREAD, &nread);
//					buf = (char*)malloc(BUF_SIZE);
//					if( (nread = read(poll_set[fd_index].fd, buf, BUF_SIZE)) <= 0 ) {
					if( nread  <= 0 ) {
						if ( nread < 0 ) perror("read");
						close(poll_set[fd_index].fd);
						numfds--;
						poll_set[fd_index].events = 0;
						printf("Removing client on fd %d\n", poll_set[fd_index].fd);
						poll_set[fd_index].fd = -1;
						exit(0);
						//free(buf);
					}

					else {
						rdata = (read_data*)malloc(sizeof(read_data));
						buf = (char*)malloc(BUF_SIZE);
						read(poll_set[fd_index].fd, buf, BUF_SIZE);
						rdata->fd = poll_set[fd_index].fd;
						rdata->buf = buf;
						rdata->nread = nread;
						while(!req_q->enqueue(rdata));
//		printf("recv1 msg\n%d\n%ld\n",rdata->fd,rdata->nread);
						nread = 0;
						/* enqueue */
					}
				}
			}
		}
	}
}

void* request_handler(void* arg) {
	req_t* req;
	read_data* rdata;
	unordered_map<int,req_t*>::iterator iter;
	while(1) {
		/* dequeue 
		rdata = dequeue;
		*/	
		while(!req_q->dequeue(&rdata));
		if ( (iter = req_map.find(rdata->fd)) == req_map.end() ) {
			req = NULL;
		}
		else {
//		printf("rdata fd %d\n%ld\n%ld",rdata->fd,rdata->nread,req_map.size());
			req = iter->second;
			req_map.erase(iter);
//			printf("find req\n");
		}
		if ( (req = GetRequest(rdata->fd, rdata->buf, rdata->nread, req)) != (req_t*)0 ) {
//			printf("split\n");
			req_map.insert( {rdata->fd, req} );
		}
		free(rdata->buf);
		free(rdata);
	}	

}

int main(int argc, char* argv[]) {
	int serv_sock;
	pthread_t t_id;
	serv_sock = init_server(atoi(argv[1]));
	req_q = new spsc_bounded_queue_t<read_data*>(QSIZE);
	pthread_create(&t_id, NULL, request_handler, NULL);
	pthread_detach(t_id);
	execute_polling(serv_sock);
}
