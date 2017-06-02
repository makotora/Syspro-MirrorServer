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