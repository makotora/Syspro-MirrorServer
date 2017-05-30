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
id_counters_array* idc_array_create(int size)
{
	id_counters_array* new_idc_array = malloc(sizeof(id_counters_array));

	if (new_idc_array == NULL)
	{
		fprintf(stderr, "idc_array_create: malloc error\n");
		exit(EXIT_FAILURE);
	}

	new_idc_array->array = malloc(size*sizeof(id_counter));

	if (new_idc_array->array == NULL)
	{
		fprintf(stderr, "idc_array_create: malloc error\n");
		exit(EXIT_FAILURE);
	}

	new_idc_array->size = size;
	new_idc_array->index = 0;

	return new_idc_array;
}


int idc_array_add(id_counters_array* idc_array, char* id)
{
	fprintf(stderr, "Adding %s\n", id);
	if (idc_array->index == idc_array->size)
	{
		fprintf(stderr, "IDC_array_add error! Array is full, cannot add!");
		return -1;
	}

	//(else)
	id_counter* array = idc_array->array; 
	array[idc_array->index].id = strdup(id);
	array[idc_array->index].counter = 0;
	array[idc_array->index].wont_increase = 0;
	idc_array->index++;

	return 0;
}

int idc_array_increase(id_counters_array* idc_array, char* id)
{
	//find that id
	int i;
	int max = idc_array->index;
	id_counter* array = idc_array->array;

	for (i=0; i<max; i++)
	{
		if (!strcmp(array[i].id, id))
		{
			fprintf(stderr, "Increasing %s.New counter %d!\n", id, array[i].counter + 1);
			array[i].counter++;
			return array[i].counter;
		}
	}

	//if we didnt return inside,no such id!
	fprintf(stderr, "idc_array_increase error!No such id!");
	fprintf(stderr, "ID: %s\n", id);
	return -999;	
}


int idc_array_decrease(id_counters_array* idc_array, char* id, int* wont_increase)
{
	//find that id
	int i;
	int max = idc_array->index;
	id_counter* array = idc_array->array;

	for (i=0; i<max; i++)
	{
		if (!strcmp(array[i].id, id))
		{
			fprintf(stderr, "Decreasing %s.New counter %d!\n", id, array[i].counter - 1);
			array[i].counter--;
			*wont_increase = array[i].wont_increase;
			return array[i].counter;
		}
	}

	//if we didnt return inside,no such id!
	fprintf(stderr, "idc_array_decrease error!No such id!");
	fprintf(stderr, "ID: %s\n", id);
	return -999;	
}


id_counter* idc_array_get(id_counters_array* idc_array, char* id)
{
	//find that id
	int i;
	int max = idc_array->index;
	id_counter* array = idc_array->array;

	for (i=0; i<max; i++)
	{
		if (!strcmp(array[i].id, id))
		{
			fprintf(stderr, "Getting %s!\n", id);
			return &(array[i]);
		}
	}

	//if we didnt return inside,no such id!
	fprintf(stderr, "idc_array_set error!No such id!");
	fprintf(stderr, "ID: %s\n", id);
	return NULL;	
}


void idc_array_free(id_counters_array** idc_array)
{
	int i;
	int max = (*idc_array)->index;
	id_counter* array = (*idc_array)->array;

	for (i=0; i<max; i++)
	{
		free(array[i].id);
	}

	free(array);
	free(*idc_array);
}