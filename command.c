#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <inttypes.h>
#include <ctype.h>
#include <sys/socket.h>
#include "queue.h"
#include "request.h"
#include "priority_queue.h"
#ifdef ENABLE_LIBFTL
#include "libmemio.h"
#endif
//for lsmtree
//#include "../lsmtree/utils.h"

/*
   if valid == 0 -> not yet
   else succ

   return -1 -> not yet
   else next postion to process
 */
#ifdef ENABLE_LIBFTL
int setup_ok() {
	return setup_ok_dma();
}

int setup_value() {
	return setup_value_dma();
}
#endif

int GetLength(char *str, char cmp, int len, int cur, req_t *req, char which) {
	int added_len=0;
	char flag = 0;
	//	int length;
	if ( cur == len ) return -1;
	else if ( str[cur] == '\n' ) {
		//		printf("GetLength1 :%c, %d\n",cmp,which);
		switch(which) {
			case 1:	// keyword
				req->keyword_info->valid++;
				break;
			case 2:	// type
				req->type_info->valid++;
				break;
			case 3:	// key
				req->key_info->valid++;
				break;
			case 4:	// value
				req->value_info->valid++;
				break;
		}
		return cur+1;
	}

	if ( cur < len && str[cur] == cmp ) cur++;
	if ( cur == len ) return -1;

	while ( cur < len && str[cur++] != '\r' ) {
		added_len = added_len * 10 + str[cur-1] - '0' ;
		//		printf("added_len : %d, %c\n", added_len, str[cur-1]);

	}

	if ( cur < len && str[cur] == '\n' ) flag = 1;

	switch(which) {
		case 1:	// keyword
			req->keyword_info->keywordNum = req->keyword_info->keywordNum * 10 + added_len;
			//			length = req->keyword_info->keywordNum;
			if ( flag ) req->keyword_info->valid++;
			break;
		case 2:	// type
			req->type_info->len = req->type_info->len * 10 + added_len;
			if ( flag ) req->type_info->valid++;
			break;
		case 3:	// key
			//printf("old key %d\n",req->key_info->len);
			req->key_info->len = req->key_info->len * 10 + added_len;
			//printf("new key %d\n",req->key_info->len);
			if ( flag ) req->key_info->valid++;
			break;
		case 4:	// value
			req->value_info->len = req->value_info->len * 10 + added_len;
			if ( flag ) req->value_info->valid++;
			break;
	}
	if ( flag ) {
		//		printf("GetLength2 :%c, %d, %d\n",cmp,which,length);
		return cur+1;
	}
	else return -1;
}

/*
   if type == 0 -> not yet
   else succ

   return -1 -> not yet
   else next postion to process
 */
int GetType(char *str, int len, int cur, req_t *req) {
	int left = req->type_info->len - req->type_info->offset;
	if ( left == 0 ) {
		if ( cur == len ) return -1;	// end of str
		else {
			if ( str[cur] == '\r') cur++;
			if ( cur < len && str[cur] == '\n' ) { 	// str[cur] == '\n'
				if ( strncmp(req->type_info->type_str, "SET", 4) == 0 ) req->type_info->type = 1;
				else if ( strncmp(req->type_info->type_str, "GET", 4) == 0 ) req->type_info->type = 2;
				else if ( strncmp(req->type_info->type_str, "DEL", 4) == 0 ) req->type_info->type = 3;
				req->type_info->valid++;
				//printf("Get Type1: %d\n",req->type_info->type);
				return cur+1;
			}
			else return -1;
		}
	}
	else {
		int available = (( len - cur > left ) ? left : len - cur) ;
		memcpy(&req->type_info->type_str[req->type_info->offset], &str[cur], available);
		req->type_info->offset += available;
		cur += available;

		/*
		   while ( cur < len && str[cur] != '\r' ) {	// copy str to type_str
		   type_str[req->type_info->offset++] = str[cur++];
		   }
		 */

		if ( cur == len ) {	// not completed
			return -1;
		}
		else {
			if ( str[cur] == '\r') cur++;
			if ( cur < len && str[cur] == '\n' ) {
				if ( strncmp(req->type_info->type_str, "SET", 4) == 0 ) req->type_info->type = 1;
				else if ( strncmp(req->type_info->type_str, "GET", 4) == 0 ) req->type_info->type = 2;
				else if ( strncmp(req->type_info->type_str, "DEL", 4) == 0 ) req->type_info->type = 3;	
				req->type_info->valid++;
				//printf("Get Type2: %d\n",req->type_info->type);
				return cur+1;
			}
			else return -1;
		}
	}
}

int GetKey(char *str, int len, int cur, req_t *req ) {
	int left = req->key_info->len - req->key_info->offset;
	if ( left == 0 ) {
		//printf("get 1\n");
		if ( cur == len ) return -1;
		else {
			//printf("get 2\n");
			if ( str[cur] == '\r' ) cur++;
			if ( cur < len && str[cur] == '\n' ) {
				//printf("get 3\n");
				//printf("Get Key1: %"PRIu64"\n",req->key_info->key);
				req->key_info->valid++;
				return cur+1;
			}
			else return -1;			
		}
	}
	else {
		//printf("get 4\n");
		int available = (( len - cur > left ) ? left : len - cur) ;
		int a = available;
		req->key_info->offset += available;
		while ( available-- ) { 
			if ( !isdigit(str[cur]) ) {
				cur++;
				continue;
			}
			req->key_info->key = req->key_info->key * 10 + str[cur++] - '0';
		}

		if ( cur == len ) {
			//printf("get 5\n");
			//printf("str : %s\nlen : %d\ncur : %d\nkeylen : %d\nkeyoffset : %d\navail : %d\n",str,len,cur,req->key_info->len, req->key_info->len - left, a);
			return -1;
		}
		else {
			//printf("get 6\n");
			if ( str[cur] == '\r' ) cur++;
			if ( cur < len && str[cur] == '\n' ) {
				//printf("get 7\n");
				req->key_info->valid++;
				//printf("Get Key2: %"PRIu64"\n",req->key_info->key);
				return cur+1;
			}
			else return -1;			
		}

	}
}

int GetValue(char* str, int len, int cur, req_t *req) {
	int left = req->value_info->len - req->value_info->offset;
	if ( left == 0 ) {
		if ( cur == len ) return -1;
		else {
			if ( str[cur] == '\r' ) cur++;
			if ( cur < len && str[cur] == '\n' ) {
				req->value_info->valid++;
				//printf("Get Value1: %s\n",req->value_info->value);
				return cur+1;
			}
			else return -1;			
		}
	}
	else {
		int available = (( len - cur > left ) ? left : len - cur) ;
		memcpy(&req->value_info->value[req->value_info->offset], &str[cur], available);
		req->value_info->offset += available;
		cur += available;

		if ( cur == len ) {
			return -1;
		}
		else {
			if ( str[cur] == '\r' ) cur++;
			if ( cur < len && str[cur] == '\n' ) {
				req->value_info->valid++;
				//printf("Get Value1: %s\n",req->value_info->value);
				return cur+1;
			}
			else return -1;			
		}

	}
}

req_t* udp_GetRequest(int fd, char *str, int len, req_t *req, struct sockaddr_in *addr) {
	int cur = 0, result = 0;
	uint64_t seq = 0;
	req_t* new_req = req;
	int key_len, i, value_len;
	//      printf("cur %d, req %p\n", cur, req);
	key_len = 0;
	value_len = 0;
	if ( new_req == NULL ) {
		//printf("nullnullnullnull\n");
		new_req = alloc_req(new_req);
		new_req->fd = fd;
		new_req->addr = addr;
	}
	else {
		//                      printf("new_one\n");
		//                      printf("keyword valid: %d\ntype valid: %d\nkey valid: %d\nvalue valid: %d\n",req->keyword_info->valid,req->type_info->valid,req->key_info->valid,req->value_info->valid);
	}

	for( i = 0; i < 3; i++ ) {
		new_req->target_fd = str[i] - '0';
		new_req->target_fd *= 10;
	}
	new_req->target_fd += str[3] - '0';

	new_req->type_info->type = (str[4] -'0') * 10 + str[5] - '0';

	key_len = (str[6] -'0') * 10 + str[7] - '0';
	for( i = 0; i < key_len; i++ ) {
		new_req->key_info->key += str[18-key_len+i] - '0';
		new_req->key_info->key *= 10;
	}
	new_req->key_info->key /= 10;
	new_req->key_info->key++;


#ifdef ENABLE_LIBFTL
	if ( new_req->type_info->type < 3 ) {
		alloc_dma(new_req);
	}
#endif


	if ( new_req->type_info->type > 1 ) {
		//EnqueReq(new_req, seq);
		make_req(new_req);
		new_req = NULL;

		return (req_t*)0;
	}


	for( i = 0; i < 3; i++ ) {
		value_len = str[18+i] - '0';
		value_len *= 10;
	}
	value_len += str[21] - '0';
	//memcpy(new_req->value_info->value, &str[22], value_len);
	make_req(new_req);
	new_req = NULL;
	return (req_t*)0;
}


req_t* master_GetRequest(int fd, char *str, int len, req_t *req) {
	int cur = 0, result = 0;
	uint64_t seq = 0;
	req_t* new_req = req;
	//	printf("cur %d, req %p\n", cur, req);
	while ( cur < len ) {
		if ( new_req == NULL ) {
			//			printf("nullnullnullnull\n");
			new_req = alloc_req(new_req);
			new_req->fd = fd;
		}
		else {
			//			printf("new_one\n");
			//			printf("keyword valid: %d\ntype valid: %d\nkey valid: %d\nvalue valid: %d\n",req->keyword_info->valid,req->type_info->valid,req->key_info->valid,req->value_info->valid);
		}

		if ( !new_req->keyword_info->valid )
			if ( (cur = GetLength(str, '*', len, cur, new_req, 1)) == -1 ) return new_req;
		//printf("1 %d\n",new_req->keyword_info->keywordNum); 
		if ( !new_req->type_info->valid ) 
			if ( (cur = GetLength(str, '$', len, cur, new_req, 2)) == -1 ) return new_req;

		//printf("2 %d\n",new_req->type_info->len); 
		if ( new_req->type_info->valid == 1 )
			if ( (cur = GetType(str, len, cur, new_req)) == -1 ) return new_req;

		//printf("3\n"); 
		if ( !new_req->key_info->valid ) 
			if ( (cur = GetLength(str, '$', len, cur, new_req, 3)) == -1 ) return new_req;

		//printf("4 %d\n",new_req->key_info->len); 
		if ( new_req->key_info->valid == 1 )
			if ( (cur = GetKey(str, len, cur, new_req)) == -1 ) return new_req;

		//printf("5\n"); 
		new_req->key_info->key++;
#ifdef ENABLE_LIBFTL
		if ( new_req->type_info->type < 3 ) {
			alloc_dma(new_req);
		}	
#endif


		if ( new_req->type_info->type > 1 ) {
			//EnqueReq(new_req, seq);
			master_make_req(new_req);
			new_req = NULL;
			continue;
		}

		//printf("6\n"); 
		if ( !new_req->value_info->valid )
			if ( (cur = GetLength(str, '$', len, cur, new_req, 4)) == -1 ) return new_req;

		//printf("7\n"); 
		if ( new_req->value_info->valid == 1 ) 
			if ( (cur = GetValue(str, len, cur, new_req)) == -1 ) return new_req;

		//printf("8\n"); 
		//EnqueReq(new_req, seq);
		master_make_req(new_req);
		new_req = NULL;
	}
	return (req_t*)0;
}

req_t* GetRequest(int fd, char *str, int len, req_t *req) {
	int cur = 0, result = 0;
	uint64_t seq = 0;
	req_t* new_req = req;
	//	printf("cur %d, req %p\n", cur, req);
	while ( cur < len ) {
		if ( new_req == NULL ) {
			//			printf("nullnullnullnull\n");
			new_req = alloc_req(new_req);
			new_req->fd = fd;
		}
		else {
			//			printf("new_one\n");
			//			printf("keyword valid: %d\ntype valid: %d\nkey valid: %d\nvalue valid: %d\n",req->keyword_info->valid,req->type_info->valid,req->key_info->valid,req->value_info->valid);
		}

		if ( !new_req->keyword_info->valid )
			if ( (cur = GetLength(str, '*', len, cur, new_req, 1)) == -1 ) return new_req;
		//printf("1 %d\n",new_req->keyword_info->keywordNum); 
		if ( !new_req->type_info->valid ) 
			if ( (cur = GetLength(str, '$', len, cur, new_req, 2)) == -1 ) return new_req;

		//printf("2 %d\n",new_req->type_info->len); 
		if ( new_req->type_info->valid == 1 )
			if ( (cur = GetType(str, len, cur, new_req)) == -1 ) return new_req;

		//printf("3\n"); 
		if ( !new_req->key_info->valid ) 
			if ( (cur = GetLength(str, '$', len, cur, new_req, 3)) == -1 ) return new_req;

		//printf("4 %d\n",new_req->key_info->len); 
		if ( new_req->key_info->valid == 1 )
			if ( (cur = GetKey(str, len, cur, new_req)) == -1 ) return new_req;

		//printf("5\n"); 
		new_req->key_info->key++;
#ifdef ENABLE_LIBFTL
		if ( new_req->type_info->type < 3 ) {
			alloc_dma(new_req);
		}	
#endif


		if ( new_req->type_info->type > 1 ) {
			//EnqueReq(new_req, seq);
			make_req(new_req);
			new_req = NULL;
			continue;
		}

		//printf("6\n"); 
		if ( !new_req->value_info->valid )
			if ( (cur = GetLength(str, '$', len, cur, new_req, 4)) == -1 ) return new_req;

		//printf("7\n"); 
		if ( new_req->value_info->valid == 1 ) 
			if ( (cur = GetValue(str, len, cur, new_req)) == -1 ) return new_req;

		//printf("8\n"); 
		//EnqueReq(new_req, seq);
		make_req(new_req);
		new_req = NULL;
	}
	return (req_t*)0;
}

int master_SendOkCommand(int fd) {

	if ( write(fd, "+OK\r\n", 5) == -1 ) {
		printf("send OK failed\n");
		return -1;
	}

#ifdef ENABLE_LIBFTL
	send_ok(fd);
#endif
	return 0;
}
int udp_SendOkCommand(req_t *req) {
	char buf[6];
	sprintf(buf, "%04d%02d", req->target_fd, req->type_info->type);
	socklen_t adr_sz = sizeof( struct sockaddr );
	if ( sendto(req->fd, buf, 6, 0, (struct sockaddr*)(req->addr), adr_sz) < 0 ) {
		printf("sendto failed()\n");
		return -1;
	}
	return 0;
}

int SendErrCommand(int fd, char* ErrMsg) {
	if ( write(fd, "-ERR ", 5) == -1 ) {
		printf("send Err failed\n");
		return -1;
	}
	if ( write(fd, ErrMsg, strlen(ErrMsg)) == -1 ) {
		printf("send Err failed\n");
		return -1;
	}
	if ( write(fd, "\r\n", 2) == -1 ) {
		printf("send Err failed\n");
		return -1;
	}
	return 0;
}

int master_SendBulkValue(int fd, char* value, int len) {


	char header[7+8192+2];
	sprintf(header, "$%d\r\n",8192);
	memcpy(&header[7], value, 8192);
	sprintf(&header[8199], "\r\n");

	if ( write(fd, header, 8201) == -1 ) {
		printf("send bulf value failed\n");
		return -1;
	}

	return 0;
}

int udp_SendBulkValue(req_t *req, char* value, int len) {
	//udp   
	char header[10+8192];
	socklen_t adr_sz = sizeof( struct sockaddr );
	sprintf(header, "%04d%02d%04d",req->target_fd, req->type_info->type,8192);
	sprintf(header, "%04d%02d%04d",req->target_fd, req->type_info->type,8192);
	memcpy(&header[10], value, 8192);
	if ( sendto(req->fd, header, 8202, 0, (struct sockaddr*)(req->addr), adr_sz) < 0 ) {
		printf("sendto failed()\n");
		return -1;
	}       

	return 0;
}


int SendDelCommand(int fd) {
	if ( write(fd, ":1\r\n", 4) == -1 ) {
		printf("send Del failed\n");
		return -1;
	}
	return 0;
}
