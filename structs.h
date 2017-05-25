#ifndef STRUCTS_H
#define STRUCTS_H

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

typedef struct id_delay id_delay;
struct id_delay
{
	int id;
	int delay;
	id_delay* next;
};

struct delays
{
	id_delay* first;
	id_delay* last;
};
typedef struct delays delays;

delays* delays_create();
void delays_free(delays** delays_ptr_ptr);
void delays_add(delays* delays_ptr, int id, int delay);

typedef struct thread_info thread_info;
struct thread_info
{
	pthread_t thr;
	thread_info* next;
};


struct thread_infos
{
	thread_info* first;
	thread_info* last;
};
typedef struct thread_infos thread_infos;

thread_infos* thread_infos_create();
void thread_infos_free(thread_infos** infos_ptr_ptr);
void thread_infos_add(thread_infos* infos_ptr, pthread_t thr);

#endif