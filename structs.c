#include "structs.h"

#define DEBUG_PRINTS 0

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
		free(target->id);
		free(target);
	}

	free(*delays_ptr_ptr);
}

void delays_add(delays* delays_ptr, char* id, int delay)
{
	id_delay* new_id_delay = malloc(sizeof(id_delay));
	new_id_delay->id = strdup(id);
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


int delays_get_by_id(delays* delays_ptr, char* id)
{
	id_delay* current = delays_ptr->first;

	while (current != NULL)
	{
		if (!strcmp(id, current->id))
			return current->delay;

		current = current->next;
	}

	return -1;
}


//ID COUNTER ARRAY
id_counters_list* idc_list_create()
{
	id_counters_list* new_idc_list = malloc(sizeof(id_counters_list));

	if (new_idc_list == NULL)
	{
		fprintf(stderr, "idc_list_create: malloc error\n");
		exit(EXIT_FAILURE);
	}

	new_idc_list->first = NULL;
	new_idc_list->last = NULL;

	return new_idc_list;
}


int idc_list_add(id_counters_list* idc_list, char* id)
{
	if (DEBUG_PRINTS)
		fprintf(stderr, "Adding %s\n", id);

	id_counter* new_idc = malloc(sizeof(id_counter));
	if (new_idc == NULL)
	{
		fprintf(stderr, "idc_list_create: malloc error\n");
		exit(EXIT_FAILURE);
	}

	new_idc->id = strdup(id);
	new_idc->counter = 0;
	new_idc->wont_increase = 0;
	new_idc->next = NULL;

	if (idc_list->first == NULL)
	{
		idc_list->first = new_idc;
		idc_list->last = new_idc;
	}
	else
	{
		idc_list->last->next = new_idc;
		idc_list->last = new_idc;
	}

	return 0;
}

int idc_list_increase(id_counters_list* idc_list, char* id)
{
	//find that id_counter
	id_counter* current = idc_list->first;

	while (current != NULL)
	{
		if (!strcmp(current->id, id))
		{
			if (DEBUG_PRINTS)
				fprintf(stderr, "Increasing %s.New counter %d!\n", id, current->counter + 1);
			current->counter++;
			return current->counter;
		}

		current = current->next;
	}

	//if we didnt return inside,no such id!
	fprintf(stderr, "idc_list_increase error!No such id!");
	fprintf(stderr, "ID: %s\n", id);
	return -999;	
}


int idc_list_decrease(id_counters_list* idc_list, char* id, int* wont_increase)
{
	//find that id_counter
	id_counter* current = idc_list->first;

	while (current != NULL)
	{
		if (!strcmp(current->id, id))
		{
			if (DEBUG_PRINTS)
				fprintf(stderr, "Decreasing %s.New counter %d!\n", id, current->counter - 1);
			current->counter--;
			*wont_increase = current->wont_increase;
			return current->counter;
		}

		current = current->next;
	}

	//if we didnt return inside,no such id!
	fprintf(stderr, "idc_list_decrease error!No such id!");
	fprintf(stderr, "ID: %s\n", id);
	return -999;	
}


id_counter* idc_list_get(id_counters_list* idc_list, char* id)
{
	//find that id_counter
	id_counter* current = idc_list->first;

	while (current != NULL)
	{
		if (!strcmp(current->id, id))
		{
			if (DEBUG_PRINTS)
				fprintf(stderr, "Getting %s!\n", id);
			return current;
		}

		current = current->next;
	}

	//if we didnt return inside,no such id!
	fprintf(stderr, "idc_list_get error!No such id!");
	fprintf(stderr, "ID: %s\n", id);
	return NULL;	
}


void idc_list_free(id_counters_list** idc_list)
{
	id_counters_list* idc_list_ptr = *idc_list;
	id_counter* current = idc_list_ptr->first;
	id_counter* target;

	while (current != NULL)
	{
		target = current;
		current = current->next;

		free(target->id);
		free(target);
	}

	free(*idc_list);
}


//Server list
server_list* server_list_create()
{
	server_list* new_sl = malloc(sizeof(server_list));

	if (new_sl == NULL)
	{
		fprintf(stderr, "server_list_create: malloc error!\n");
		exit(EXIT_FAILURE);
	}

	new_sl->first = NULL;
	new_sl->last = NULL;

	return new_sl;
}


void server_list_add(server_list* sl, char* address, int port)
{
	//first make sure that this server_info is not already in the list
	//if it is.just return
	server_info* current = sl->first;

	while (current != NULL)
	{
		//it is already in the list
		if ( !strcmp(current->address, address) && current->port == port)
			return;

		current = current->next;
	}

	//if we got here its not in the list!Add it
	server_info* new_server_info = malloc(sizeof(server_info));

	if (new_server_info == NULL)
	{
		fprintf(stderr, "server_list_add: malloc error!\n");
		exit(EXIT_FAILURE);
	}

	new_server_info->address = strdup(address);
	new_server_info->port = port;
	new_server_info->next = NULL;

	if (sl->first == NULL)
	{
		sl->first = new_server_info;
		sl->last = new_server_info;
	}
	else
	{
		sl->last->next = new_server_info;
		sl->last = new_server_info;
	}

}


void server_list_free(server_list** sl)
{
	server_list* sl_ptr = *sl;
	server_info* current = sl_ptr->first;
	server_info* target;

	while (current != NULL)
	{
		target = current;
		current = current->next;

		free(target->address);
		free(target);
	}

	free(*sl);
}