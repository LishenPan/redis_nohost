#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <unordered_map>
#include <string.h>
#include <unistd.h>
#include "command.h"
#include "lockfreeq.h"
#include "request.h"
#include "LR_inter.h"
using namespace std;

#define SERVER_PORT     12345
#define MAX_CLNT         2000
#define BUFSIZE 1024*1024/10
#define QSIZE            2048
#define TRUE                1
#define FALSE               0

typedef struct read_data {
	int fd;
	char* buf;
	ssize_t nread;
	uint64_t addr_key;
	struct sockaddr_in *addr;
} read_data;

unordered_map<uint64_t,req_t*> req_map;
spsc_bounded_queue_t<read_data*> *req_q;

void* request_handler(void* arg) {
	req_t* 					req;
	read_data* 				rdata;
	unordered_map<uint64_t,req_t*>::iterator 	iter;
	while(1) {
		while(!req_q->dequeue(&rdata));
		if ( (iter = req_map.find(rdata->addr_key)) == req_map.end() ) {
			req = NULL;
		}
		else {
			//		printf("rdata fd %d\n%ld\n%ld",rdata->fd,rdata->nread,req_map.size());
			req = iter->second;
			req_map.erase(iter);
			//			printf("find req\n");
		}
		if ( (req = udp_GetRequest(rdata->fd, rdata->buf, rdata->nread, req, rdata->addr)) != (req_t*)0 ) {
			//			printf("split\n");
			req_map.insert( {rdata->addr_key, req} );
		}
		free(rdata->buf);
		//free(rdata->addr);
		free(rdata);
	}	
}

int main (int argc, char *argv[])
{
	int    		len, rc, on = 1;
	int    		listen_sd = -1, new_sd = -1;
	int    		desc_ready, end_server = FALSE, compress_array = FALSE;
	int    		close_conn;
	char   		*buffer;
	struct 		sockaddr_in   addr;
	struct 		sockaddr_in   *clnt_addr;
	socklen_t 	clnt_adr_sz;
	int    		timeout;
	//	struct 		pollfd fds[MAX_CLNT];
	struct 		pollfd fds[1];
	int    		nfds = 1, current_size = 0, i, j;
	pthread_t 	t_id;
	read_data	*rdata;

	/*************************************************************/
	/* Create an AF_INET stream socket to receive incoming       */
	/* connections on                                            */
	/*************************************************************/
	listen_sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (listen_sd < 0)
	{
		perror("socket() failed");
		exit(-1);
	}

	/*************************************************************/
	/* Allow socket descriptor to be reuseable                   */
	/*************************************************************/
	rc = setsockopt(listen_sd, SOL_SOCKET,  SO_REUSEADDR,
			(char *)&on, sizeof(on));
	if (rc < 0)
	{
		perror("setsockopt() failed");
		close(listen_sd);
		exit(-1);
	}

	/*************************************************************/
	/* Set socket to be nonblocking. All of the sockets for    */
	/* the incoming connections will also be nonblocking since  */
	/* they will inherit that state from the listening socket.   */
	/*************************************************************/
	rc = ioctl(listen_sd, FIONBIO, (char *)&on);
	if (rc < 0)
	{
		perror("ioctl() failed");
		close(listen_sd);
		exit(-1);
	}

	/*************************************************************/
	/* Bind the socket                                           */
	/*************************************************************/
	memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port        = htons(atoi(argv[1]));
	//for(int i = 0; i < 14; i++ )
	//	printf("%d\n", ((struct sockaddr*)&addr)->sa_data[i]);
	rc = bind(listen_sd,
			(struct sockaddr *)&addr, sizeof(addr));
	if (rc < 0)
	{
		perror("bind() failed");
		close(listen_sd);
		exit(-1);
	}

	/*************************************************************/
	/* Set the listen back log                                   */
	/*************************************************************/
	/*
	   rc = listen(listen_sd, 32);
	   if (rc < 0)
	   {
	   perror("listen() failed");
	   close(listen_sd);
	   exit(-1);
	   }
	 */
	/*************************************************************/
	/* Initialize the pollfd structure                           */
	/*************************************************************/
	memset(fds, 0 , sizeof(fds));

	/*************************************************************/
	/* Set up the initial listening socket                        */
	/*************************************************************/
	fds[0].fd = listen_sd;
	fds[0].events = POLLIN;
	/*************************************************************/
	/* Initialize the timeout to 3 minutes. If no               */
	/* activity after 3 minutes this program will end.           */
	/* timeout value is based on milliseconds.                   */
	/*************************************************************/
	timeout = (3 * 60 * 1000);

	lr_inter_init();	

	req_q = new spsc_bounded_queue_t<read_data*>(QSIZE);
	pthread_create(&t_id, NULL, request_handler, NULL);
	pthread_detach(t_id);

	/*************************************************************/
	/* Loop waiting for incoming connects or for incoming data   */
	/* on any of the connected sockets.                          */
	/*************************************************************/
	do
	{
		/***********************************************************/
		/* Call poll() and wait 3 minutes for it to complete.      */
		/***********************************************************/
		//printf("Waiting on poll()...\n");
		//rc = poll(fds, nfds, timeout);
		rc = poll(fds, 1, timeout);

		/***********************************************************/
		/* Check to see if the poll call failed.                   */
		/***********************************************************/
		if (rc < 0)
		{
			perror("  poll() failed");
			break;
		}

		/***********************************************************/
		/* Check to see if the 3 minute time out expired.          */
		/***********************************************************/
		if (rc == 0)
		{
			printf("  poll() timed out.  End program.\n");
			break;
		}


		/***********************************************************/
		if(fds[0].revents == 0) {
			continue;
}

		if(fds[0].revents != POLLIN)
		{
			printf("  Error! revents = %d\n", fds[0].revents);
			end_server = TRUE;
			break;

		}

		/* Receive data on this connection until the         */
		/* recv fails with EWOULDBLOCK. If any other        */
		/* failure occurs, we will close the                 */
		/* connection.                                       */
		/*****************************************************/

		buffer = (char*)malloc(BUFSIZE);
		clnt_adr_sz = sizeof(struct sockaddr);
		clnt_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));

		rc = recvfrom(fds[0].fd, buffer, BUFSIZE, 0, (struct sockaddr *)clnt_addr, &clnt_adr_sz);
		if (rc < 0)
		{
			if (errno != EWOULDBLOCK)
			{
				perror("  recv() failed");
				close_conn = TRUE;
				free(clnt_addr);
				free(buffer);
			}
			break;
		}
		/*****************************************************/
		/* Check to see if the connection has been           */
		/* closed by the client                              */
		/*****************************************************/

		/*****************************************************/
		/* Data was received                                 */
		/*****************************************************/
		rdata = (read_data*)malloc(sizeof(read_data));
		rdata->fd = fds[0].fd;
		rdata->buf = buffer;
		rdata->nread = rc;
		rdata->addr = clnt_addr;

		for ( int i = 0; i < 5; i++ ) {
			rdata->addr_key += ((sockaddr*)clnt_addr)->sa_data[i];
			rdata->addr_key <<= 8;
		}
		rdata->addr_key += ((sockaddr*)clnt_addr)->sa_data[5];

		while(!req_q->enqueue(rdata));
		len = rc;
		//	printf("  %d bytes received\n", len);

		/*****************************************************/
		/* Echo the data back to the client                  */
		/*****************************************************/
		/*
		   rc = send(fds[i].fd, buffer, len, 0);
		   if (rc < 0)
		   {
		   perror("  send() failed");
		   close_conn = TRUE;
		   break;
		   }
		 */

	} while(TRUE);

	/*******************************************************/
	/* If the close_conn flag was turned on, we need       */
	/* to clean up this active connection. This           */
	/* clean up process includes removing the              */
	/* descriptor.                                         */
	/*******************************************************/
	return 0;
}

