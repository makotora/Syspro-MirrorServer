#include "ThreadSafeBuffer.h"

TSBuffer* TSBuffer_create(int size)
{
    //create buff and init buffer's vars
    TSBuffer* new_tsbuffer = malloc(sizeof(TSBuffer));
    if (new_tsbuffer == NULL)
    {
    	perror_exit("TSBuffer_create: malloc error");
    }

    new_tsbuffer->data = malloc(size * sizeof(Buffer_data));
    if (new_tsbuffer->data == NULL)
    {
    	perror_exit("TSBuffer_create: malloc error");
    }

    new_tsbuffer->size = size;
    new_tsbuffer->start = 0;
    new_tsbuffer->end = -1;
    new_tsbuffer->count = 0;

    	//initialise mutex and cond vars
	pthread_mutex_init(&new_tsbuffer->count_mtx, 0);
	pthread_mutex_init(&new_tsbuffer->end_mtx, 0);
	pthread_mutex_init(&new_tsbuffer->start_mtx, 0);
    pthread_cond_init(&new_tsbuffer->not_empty, 0);
    pthread_cond_init(&new_tsbuffer->not_full, 0);

    return new_tsbuffer;
}


void TSBuffer_push(TSBuffer* buffer_ptr, char* host, int port, char* id, char* local_path, char* remote_path)
{
	char* host_copy = strdup(host);
	char* localp_copy = strdup(local_path);
	char* remotep_copy = strdup(remote_path);
	char* id_copy = strdup(id);

	//make sure buffer is not full
	pthread_mutex_lock(&buffer_ptr->count_mtx);

	while(buffer_ptr->count >= buffer_ptr->size)
	{
		printf("THREAD %ld : FOUND BUFFER FULL!\n", pthread_self());
		pthread_cond_wait(&buffer_ptr->not_full, &buffer_ptr->count_mtx);
	}

	buffer_ptr->count++;//count one element more since I am about to add one
	//ensure that not too many threads will push (even though I havent pushed yet)

	pthread_mutex_unlock(&buffer_ptr->count_mtx);

	//add data to buffer
	int end;

	pthread_mutex_lock(&buffer_ptr->end_mtx);//make sure that no other thread will mess with the end var at the same time
	buffer_ptr->end = (buffer_ptr->end + 1) % buffer_ptr->size;
	end = buffer_ptr->end;
	pthread_mutex_unlock(&buffer_ptr->end_mtx);
	
	buffer_ptr->data[end].host = host_copy;
	buffer_ptr->data[end].port = port;
	buffer_ptr->data[end].id = id_copy;
	buffer_ptr->data[end].local_path = localp_copy;
	buffer_ptr->data[end].remote_path = remotep_copy;
}


Buffer_data TSBuffer_pop(TSBuffer* buffer_ptr)
{
	Buffer_data buff_data;

	pthread_mutex_lock(&buffer_ptr->count_mtx);

	//make sure buffer is not empty
	while (buffer_ptr->count <= 0)
	{
		printf("THREAD %ld : FOUND BUFFER EMPTY!\n", pthread_self());
		pthread_cond_wait(&buffer_ptr->not_empty, &buffer_ptr->count_mtx);
	}
	buffer_ptr->count--;//reduce counter since I am about to remove an element 
	//ensure that not too many threads will pop (even though I havent popped yet)

	pthread_mutex_unlock(&buffer_ptr->count_mtx);

	//pop data to buffer
	int start;

	pthread_mutex_lock(&buffer_ptr->start_mtx);//make sure that no other thread will mess with the start var at the same time
	start = buffer_ptr->start;
	buffer_ptr->start = (buffer_ptr->start + 1) % buffer_ptr->size;
	pthread_mutex_unlock(&buffer_ptr->start_mtx);	
	
	buff_data = buffer_ptr->data[start];

	return buff_data;
}

//THIS IS NOT THREAD SAFE!FREE IS ONLY CALLED WHEN NO CONSUMERS/PRODUCERS ARE DOING SOMETHING
void TSBuffer_free(TSBuffer** buffer_ptr_ptr)
{
	TSBuffer* buffer_ptr = *buffer_ptr_ptr;

	while (buffer_ptr->count > 0)
	{
		int start = buffer_ptr->start;

		free(buffer_ptr->data[start].host);
		free(buffer_ptr->data[start].local_path);
		free(buffer_ptr->data[start].remote_path);
		free(buffer_ptr->data[start].id);

		buffer_ptr->start = (buffer_ptr->start + 1) % buffer_ptr->size;
		buffer_ptr->count--;
	}

	free(buffer_ptr->data);
	free(*buffer_ptr_ptr);

	//mtx and cond vars destroy
	pthread_cond_destroy(&buffer_ptr->not_full);
	pthread_cond_destroy(&buffer_ptr->not_empty);    
    pthread_mutex_destroy(&buffer_ptr->count_mtx);
    pthread_mutex_destroy(&buffer_ptr->start_mtx);
    pthread_mutex_destroy(&buffer_ptr->end_mtx);
}

//THIS FUNCTIONS IS NOT THREAD SAFE.ITS FOR DEBUGGING!
//assuming noone is messing with the buffer right now~
void TSBuffer_print(TSBuffer* buffer_ptr)
{
	int i = 0;
	int count = buffer_ptr->count;
	int start = buffer_ptr->start;

	while (count > 0)
	{
		fprintf(stderr, "TSBuffer element %d\n", ++i);
		fprintf(stderr, "\tHostname: %s\n", buffer_ptr->data[start].host);
		fprintf(stderr, "\tPort: %d\n", buffer_ptr->data[start].port);
		fprintf(stderr, "\tID: %s\n", buffer_ptr->data[start].id);
		fprintf(stderr, "\tLocal Path: %s\n",buffer_ptr->data[start].local_path);
		fprintf(stderr, "\tRemote Path: %s\n",buffer_ptr->data[start].remote_path);

		start = (start + 1) % buffer_ptr->size;
		count--;
	}
}
