#include "Buffer.h"

Buffer* Buffer_create(int size)
{
    //create buff and init buffer's vars
    Buffer* new_tsbuffer = malloc(sizeof(Buffer));
    if (new_tsbuffer == NULL)
    {
    	perror_exit("Buffer_create: malloc error");
    }

    new_tsbuffer->data = malloc(size * sizeof(Buffer_data));
    if (new_tsbuffer->data == NULL)
    {
    	perror_exit("Buffer_create: malloc error");
    }

    new_tsbuffer->size = size;
    new_tsbuffer->start = 0;
    new_tsbuffer->end = -1;
    new_tsbuffer->count = 0;

    return new_tsbuffer;
}


void Buffer_push(Buffer* buffer_ptr, char* host, int port, char* id, char* local_path, char* remote_path)
{
	char* host_copy = strdup(host);
	char* localp_copy = strdup(local_path);
	char* remotep_copy = strdup(remote_path);
	char* id_copy = strdup(id);

	buffer_ptr->count++;//count one element more since I am about to add one

	//add data to buffer
	int end;

	buffer_ptr->end = (buffer_ptr->end + 1) % buffer_ptr->size;
	end = buffer_ptr->end;
	buffer_ptr->data[end].host = host_copy;
	buffer_ptr->data[end].port = port;
	buffer_ptr->data[end].id = id_copy;
	buffer_ptr->data[end].local_path = localp_copy;
	buffer_ptr->data[end].remote_path = remotep_copy;
}


Buffer_data Buffer_pop(Buffer* buffer_ptr)
{
	Buffer_data buff_data;

	buffer_ptr->count--;//reduce counter since I am about to remove an element 
	//ensure that not too many consumers will consume

	//pop data from buffer
	int start;

	start = buffer_ptr->start;
	buffer_ptr->start = (buffer_ptr->start + 1) % buffer_ptr->size;
	buff_data = buffer_ptr->data[start];

	return buff_data;
}

//THIS IS NOT THREAD SAFE!FREE IS ONLY CALLED WHEN NO CONSUMERS/PRODUCERS ARE DOING SOMETHING
void Buffer_free(Buffer** buffer_ptr_ptr)
{
	Buffer* buffer_ptr = *buffer_ptr_ptr;

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
}

//THIS FUNCTIONS IS NOT THREAD SAFE.ITS FOR DEBUGGING!
//assuming noone is messing with the buffer right now~
void Buffer_print(Buffer* buffer_ptr)
{
	int i = 0;
	int count = buffer_ptr->count;
	int start = buffer_ptr->start;

	while (count > 0)
	{
		fprintf(stderr, "Buffer element %d\n", ++i);
		fprintf(stderr, "\tHostname: %s\n", buffer_ptr->data[start].host);
		fprintf(stderr, "\tPort: %d\n", buffer_ptr->data[start].port);
		fprintf(stderr, "\tID: %s\n", buffer_ptr->data[start].id);
		fprintf(stderr, "\tLocal Path: %s\n",buffer_ptr->data[start].local_path);
		fprintf(stderr, "\tRemote Path: %s\n",buffer_ptr->data[start].remote_path);

		start = (start + 1) % buffer_ptr->size;
		count--;
	}
}


void Buffer_print_element(Buffer_data buffer_element)
{
	fprintf(stderr, "--Printing buffer element--\n");
		fprintf(stderr, "\tHostname: %s\n", buffer_element.host);
		fprintf(stderr, "\tPort: %d\n", buffer_element.port);
		fprintf(stderr, "\tID: %s\n", buffer_element.id);
		fprintf(stderr, "\tLocal Path: %s\n", buffer_element.local_path);
		fprintf(stderr, "\tRemote Path: %s\n", buffer_element.remote_path);
}