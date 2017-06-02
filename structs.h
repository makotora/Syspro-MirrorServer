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

//parameters passed to mirror threds
struct mirror_thread_params
{
	char* dirname;
	char* host;
	int port;
	char* dirorfile;
	char* id;
	int delay;
};
typedef struct mirror_thread_params mirror_thread_params;


//list that matches id to delay (for content server)
typedef struct id_delay id_delay;
struct id_delay
{
	char* id;
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
void delays_add(delays* delays_ptr, char* id, int delay);
int delays_get_by_id(delays* delays_ptr, char* id);

typedef struct id_counter id_counter;
struct id_counter
{
	char* id;
	int counter;
	int wont_increase;
	id_counter* next;
};

struct id_counters_list
{
	id_counter* first;
	id_counter* last;
};
typedef struct id_counters_list id_counters_list;

id_counters_list* idc_list_create();
int idc_list_add(id_counters_list* idc_list, char* id);
int idc_list_increase(id_counters_list* idc_list, char* id);
int idc_list_decrease(id_counters_list* idc_list, char* id, int* wont_increase);
id_counter* idc_list_get(id_counters_list* idc_list, char* id);
void idc_list_free(id_counters_list** idc_list);

#endif