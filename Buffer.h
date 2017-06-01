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
};
typedef struct Buffer_data Buffer_data;

struct Buffer
{
	int size;
	Buffer_data* data;
	int start;
	int end;
	int count;
};
typedef struct Buffer Buffer;

Buffer* Buffer_create(int size);
void Buffer_push(Buffer* buffer_ptr, char* host, int port, char* id, char* local_path, char* remote_path);
Buffer_data Buffer_pop(Buffer* buffer_ptr);
void Buffer_free(Buffer** buffer_ptr_ptr);
void Buffer_print(Buffer* buffer_ptr);

void Buffer_print_element(Buffer_data buffer_element);

#endif