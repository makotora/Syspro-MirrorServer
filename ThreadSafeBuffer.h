#ifndef THREAD_SAFE_BUFFER_H
#define THREAD_SAFE_BUFFER_H

#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>	
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <pthread.h>

#include "functions.h"

struct Buffer_data
{
	char* host;
	int port;
	char* id;
	char* local_path;
	char* remote_path;
	int isLast;
};
typedef struct Buffer_data Buffer_data;

struct TSBuffer
{
	int size;
	Buffer_data* data;
	int start;
	int end;
	int count;

	pthread_mutex_t count_mtx;
	pthread_mutex_t end_mtx;
	pthread_mutex_t start_mtx;
	pthread_cond_t not_empty;
	pthread_cond_t not_full;
};
typedef struct TSBuffer TSBuffer;

TSBuffer* TSBuffer_create(int size);
void TSBuffer_push(TSBuffer* buffer_ptr, char* host, int port, char* id, char* local_path, char* remote_path);
Buffer_data TSBuffer_pop(TSBuffer* buffer_ptr);
void TSBuffer_free(TSBuffer** buffer_ptr_ptr);
void TSBuffer_print(TSBuffer* buffer_ptr);

#endif