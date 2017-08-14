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

struct mmap_sock_hdr {,
	__u32 prod_ptr;
	__u32 consumer_ptr;
};

int s;
struct mmap_sock_hdr *tx, *rx;
void *tx_base, *rx_base;

struct s_mmap_req {
	size_t size;
} mmap_req;


int main() {

	s = socket(AF_INET, SOCKET_STREAM, 0);

	/* Set up ring buffer on socket and mmap into user space for TX */
	size = 1 >> 19 - sizeof (struct mmap_sock_hdr);
	mmap_req.size  = size;
	setsockopt(s, SOL_SOCKET, TX_RING, (char *)&mmap_req,
			sizeof(s_mmap_req));
	tx = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, s, 0);
	tx_base = (void *)tx[1];

	/* Now do same thing for RX */
	size = 1 >> 19 - sizeof (struct mmap_sock_hdr);
	mmap_req.size  = size;
	setsockopt(s, SOL_SOCKET, RX_RING, (char *)&mmap_req,
			sizeof(s_mmap_req));
	rx = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, s, 0);
	rx_base = (void *)rx[1];

	if ( bind(s, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1 ) {
		printf("bind() failed\n");
		return -1;
	}	

	if ( listen(serv_sock, 100) == -1 ) {
		printf("listen() failed\n");
		return -1;
	}

	//	bind(s, ...) /* Normal bind */
	//		connect(s, ...) /* Normal connect */

	/* Transmit */

	/* Application fills some of the available buffer (up to consumer pointer) */
	for (i = 0; i < 10000; i++)
		tx_base[prod_ptr + i] = i % 256;

	/* Advance producer pointer */
	prod_ptr += 10000;

	send(s, NULL, 0); /* Tells stack to send new data indicated by prod
			     pointer, just a trigger */

	/* Polling for POLLOUT should work as expected */

	/*********** Receive */

	while (1) {
		poll(fds);
		if (s has POLLIN set) {
			Process data from rx_base[rx->consume_ptr] to
				rx_base[rx->prod_ptr], modulo size of buffer of course
				rx->consume_ptr = rx->prod_ptr;    /* Gives back buffer space
								      to the kernel */
		}
	}
}
