#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "measure.h"
#include "queue.h"
#include "priority_queue.h"
#include "request.h"
#include "command.h"
#include "LR_inter.h"

#ifdef ENABLE_LIBFTL
#include "libmemio.h"
#endif
#define INT_MAX 1000000
extern MeasureTime mt;
pthread_mutex_t queue_mutx = PTHREAD_MUTEX_INITIALIZER;
extern queue *req_queue;
extern int serv_sock;

req_t *reqs[1000] = {0,};
int start = 0, end = 0;

int end_req(req_t*);
struct timeval ms,me,re;

void EnqueReq(req_t *req, uint64_t seq) {
	//	req->key |= seq << 32; // make unique key
	pthread_mutex_lock(&queue_mutx);
	printf("type: %d, Key: %lu\n Value len:%d\nValue: %s\n",req->type_info->type,req->key_info->key,req->value_info->len,req->value_info->value);
	printf("enqueue!!! sock %d\n", req->fd);
	//	add_rear(req_queue, (void*)req);
	pthread_mutex_unlock(&queue_mutx);
}

req_t* DequeReq() {
	req_t *req;
	pthread_mutex_lock(&queue_mutx);
	//	req = (req_t*)remove_front(req_queue);
	pthread_mutex_unlock(&queue_mutx);
	return req;
}

#ifdef ENABLE_LIBFTL
void alloc_dma(req_t *req) {
#ifdef DEBUG_MODE
	printf("start\n");
#endif
	req->dmaTag = memio_alloc_dma(req->type_info->type, &(req->value_info->value));
#ifdef DEBUG_MODE
	printf("end\n");
#endif
	return;
}
#endif

#ifdef ENABLE_LIBFTL
void free_dma(req_t *req) {
	//memio_free_dma(req->type_info->type, req->value_info->value);
	memio_free_dma(req->type_info->type, req->dmaTag);
	return;
}
#endif

req_t* alloc_req(req_t *req) {
	req = (req_t*)calloc(1,sizeof(req_t));
	req->keyword_info = (Keyword_info*)malloc(sizeof(Keyword_info));
	memset(req->keyword_info, 0, sizeof(Keyword_info));
	req->type_info = (Type_info*)malloc(sizeof(Type_info));
	memset(req->type_info, 0, sizeof(Type_info));
	req->key_info = (Key_info*)malloc(sizeof(Key_info));
	memset(req->key_info, 0, sizeof(Key_info));
	req->value_info = (Value_info*)malloc(sizeof(Value_info));
	memset(req->value_info, 0, sizeof(Value_info));
#ifndef ENABLE_LIBFTL
	req->value_info->value = (char*)malloc(sizeof(char)*8192);
	//memset(req->value_info->value, 0, 8192);
#endif
	req->fd = 0;
	req->keyword_info->valid = 0;
	req->type_info->valid = 0;
	req->key_info->valid = 0;
	req->value_info->valid = 0;
	req->key_info->key = 0;
	req->type_info->offset = 0;
	req->key_info->offset = 0;
	req->value_info->offset = 0;

	req->keyword_info->keywordNum = 0;
	req->type_info->len = 0;
	req->key_info->len = 0;
	req->value_info->len = 0;

	req->end_req = end_req;

	memset(req->type_info->type_str, 0, 10);
	return req;	
}

void free_req(req_t *req) {
#ifdef ENABLE_LIBFTL
	if ( req->type_info->type == 2) free_dma(req);
#endif
#ifndef ENABLE_LIBFTL
	free(req->value_info->value);
#endif
	free(req->keyword_info);
	free(req->type_info);
	free(req->key_info);
	free(req->value_info);
	if(req->buf) free(req->buf);
	if(req->addr) free(req->addr);
	free(req);
	return;
}

int make_req(req_t *req) {
	static int r=0;
	static bool flag=false;
	int i;
	uint64_t key = req->key_info->key;
	int key_len = 0;
	int len;
	char * buf;
	int empty = 10028;
	int cx = 0;

	if ( lr_make_req(req) == -1 ) {
		udp_SendBulkValue(req, NULL, -1);
		printf("make req failed!\n");
		return -1;
	}

	if ( req->type_info->type == 1 ) {
		udp_SendOkCommand(req);
	}
	end_req(req);
}

//#ifdef ENABLE_LIBFTL
int master_make_req(req_t *req) {
	static int r=0;
	static bool flag=false;
	int i;
	uint64_t key = req->key_info->key;
	int key_len = 0;
	int len;
	char * buf;
	int empty = 10028;
	int cx = 0;
	switch(req->type_info->type) {
		case 1 :
			req->buf = (char*)calloc(10028,sizeof(char));
			buf = req->buf;
			cx = snprintf(&buf[0], empty - cx, "%04d", req->fd);

			cx += snprintf(&buf[4], empty - cx,  "%02d", 1);
			while(key != 0) {
				key /= 10;
				key_len++;
			}	
			cx += snprintf(&buf[6], empty - cx, "%02d%010d",key_len,(int)(req->key_info->key));
			cx += snprintf(&buf[18], empty - cx, "%04d%s\r\n", 8192, req->value_info->value);
			break;
		case 2 : 
			req->buf = (char*)calloc(20,sizeof(char));
			buf = req->buf;
			cx += snprintf(&buf[0], empty - cx, "%04d", req->fd);
			cx += snprintf(&buf[4], empty - cx, "%02d", 2);
			while(key != 0) { 
				key /= 10;
				key_len++;
			}
			cx += snprintf(&buf[6], empty - cx, "%02d%010d\r\n",key_len,(int)(req->key_info->key));
			break;
		case 3 :
			break;
		default :
			break;
	}
	len = strlen(req->buf);
	if ( write(serv_sock, req->buf, len) < 0 ) {
		printf("write() failed\n");
		return -1;
	}
	free_req(req);
	return 0;
}



//#ifdef ENABLE_LIBFTL
int end_req(req_t *req) {
	//	printf("end_req\n");
#ifdef DEBUG_MODE
	printf("end_req start\n");
#endif
	if ( req->type_info->type == 2 ) {
		/*
		   unsigned int *_cur = req->cur;
		   if ( req->seq == *_cur ) {
		   node_t *n;
		   req_t *r;
		   SendBulkValue(req->fd, req->value_info->value, req->value_info->len);
		 *_cur = ( *_cur + 1 ) % 0xffffffff;
		 while ( (n = PQ_front(req->pq)) != NULL ) {
		 r = (req_t*)(n->data);
		 if ( r->seq == *_cur ) {
		 PQ_pop(req->pq);
		 SendBulkValue(r->fd, r->value_info->value, r->value_info->len);
		 *_cur = ( *_cur + 1 ) % 0xffffffff;
		 free_req(r);
		 }
		 else break;
		 }
		 free_req(req);
		 }
		 else {
		 PQ_push(req->pq, req->seq, (void*)req);
		 }
		 }
		 else {
		 free_req(req);
		 */
		udp_SendBulkValue(req, req->value_info->value, req->value_info->len);

		//		SendBulkValue(req->fd, NULL, -1);
	}
	free_req(req);

#ifdef DEBUG_MODE
	printf("end_req end\n");
#endif
	return 0;
}
//#endif
