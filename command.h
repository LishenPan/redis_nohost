#ifndef __COMMAND_H__
#define __COMMAND_H_
#include <stdint.h>
#include "priority_queue.h"
#include <pthread.h>
#include <sys/socket.h>
#include "request.h"

int GetLength(char*, char, int, int, req_t, char);
int GetType(char*, int, int, req_t);
int GetKey(char*, int, int, req_t);
int GetValue(char*, int, int, req_t);
req_t* AllocRequest(req_t*);
req_t* uv_GetRequest(char*, int, req_t*);
//req_t* GetRequest(int, char*, int, req_t*, unsigned int*, unsigned int*, heap_t*, pthread_mutex_t*);
req_t* master_GetRequest(int, char*, int, req_t*);
req_t* udp_GetRequest(int, char*, int, req_t*,struct sockaddr_in*);
req_t* GetRequest(int, char*, int, req_t*);
//int ParseAndInsertCommand(int, char*, int, uint64_t);
int master_SendOkCommand(int);
int udp_SendOkCommand(req_t*);
int SendErrCommand(int, char*);
int master_SendBulkValue(int, char*, int);
int udp_SendBulkValue(req_t*, char*, int);
int SendDelCommand(int);
int setup_ok();
int setup_value();
#endif
