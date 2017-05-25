#include "structs.h"


//DELAYS
delays* delays_create()
{
	delays* new_delays = malloc(sizeof(delays));

	if (new_delays == NULL)
	{
		fprintf(stderr, "delays_create: malloc error!\n");
		exit(EXIT_FAILURE);
	}

	new_delays->first = NULL;
	new_delays->last = NULL;

	return new_delays;

}


void delays_free(delays** delays_ptr_ptr)
{
	delays* delays_ptr = *delays_ptr_ptr;
	id_delay* current = delays_ptr->first;
	id_delay* target;

	while (current != NULL)
	{
		target = current;
		current = current->next;
		free(target);
	}

	free(*delays_ptr_ptr);
}

void delays_add(delays* delays_ptr, int id, int delay)
{
	id_delay* new_id_delay = malloc(sizeof(id_delay));
	new_id_delay->id = id;
	new_id_delay->delay = delay;
	new_id_delay->next = NULL;

	if (new_id_delay == NULL)
	{
		fprintf(stderr, "delays_add: malloc error\n");
		exit(EXIT_FAILURE);
	}

	if (delays_ptr->first == NULL)
	{
		delays_ptr->first = new_id_delay;
		delays_ptr->last = new_id_delay;
	}
	else
	{
		delays_ptr->last->next = new_id_delay;
		delays_ptr->last = new_id_delay;
	}
}


//THREAD INFOS
thread_infos* thread_infos_create()
{
	thread_infos* new_infos = malloc(sizeof(thread_infos));

	if (new_infos == NULL)
	{
		fprintf(stderr, "thread_infos_create: malloc error!\n");
		exit(EXIT_FAILURE);
	}

	new_infos->first = NULL;
	new_infos->last = NULL;

	return new_infos;
}


void thread_infos_free(thread_infos** infos_ptr_ptr)
{
	thread_infos* infos_ptr = *infos_ptr_ptr;
	thread_info* current = infos_ptr->first;
	thread_info* target;

	while (current != NULL)
	{
		target = current;
		current = current->next;
		free(target);
	}

	free(*infos_ptr_ptr);
}

void thread_infos_add(thread_infos* infos_ptr, pthread_t thr)
{
	thread_info* new_info_ptr = malloc(sizeof(thread_info));
	
	if (new_info_ptr == NULL)
	{
		fprintf(stderr, "thread_infos_add: malloc error\n");
		exit(EXIT_FAILURE);
	}

	new_info_ptr->thr = thr;
	new_info_ptr->next = NULL;

	if (infos_ptr->first == NULL)
	{
		infos_ptr->first = new_info_ptr;
		infos_ptr->last = new_info_ptr;
	}
	else
	{
		infos_ptr->last->next = new_info_ptr;
		infos_ptr->last = new_info_ptr;
	}
}