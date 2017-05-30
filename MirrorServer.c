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
#include <sys/stat.h>

#include "ThreadSafeBuffer.h"
#include "Protocol.h"
#include "functions.h"
#include "structs.h"

#define BUFSIZE 1024
#define BUFFER_SIZE 10

pthread_mutex_t mtx;
pthread_cond_t allDone;
int total_devices;
int numDevicesDone;
char* dirname;
//global structs
TSBuffer* ts_buffer; //buffer
id_counters_array* idc_array;//array countaining a counter for files to fetch for each ID

void* mirror_thread(void* infos_par)
{
	mirror_thread_params* infos = (mirror_thread_params *) infos_par;
	int sock;
    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;
    char buf[BUFSIZE];
    int matches_found;
    int files_for_fetch;
    id_counter* idc;

    /* Create socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    	perror_exit("socket");

	/* Find server address */
    if ((rem = gethostbyname(infos->host)) == NULL) 
    {	
	   herror("gethostbyname");
	   exit(1);
    }
    
    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(infos->port);
    
    /* Initiate connection */
    if (connect(sock, serverptr, sizeof(server)) < 0)
	   perror_exit("connect");
    
    printf("Mirror Thread Connecting to %s port %d\n", infos->host, infos->port);
        	
    sprintf(buf, "LIST %s %d", infos->id, infos->delay);

    fprintf(stderr, "MirrorThread: Sending '%s' to ContentServer!\n", buf);
    sendMessage(sock, buf);
    fprintf(stderr, "MirrorThread: Looking for: '%s'\n", infos->dirorfile);
        	
   	char c;
   	int buf_index;
   	FILE* socket_fp;

	//get FILE pointer for socket
   	socket_fp = fdopen(sock ,"r+");
	if (socket_fp == NULL)
		perror_exit("fdopen");

	matches_found = 0;
	char* localPath;
	//First of all,the content server will send us the name of the directories he contains
	//Create the ones we need (mkdir)
	buf_index = 0;
   	while ( (c = getc(socket_fp)) != EOF )
    {
    	//if we received one result (they end with '\n')
    	if (c == '\n')
    	{
    		buf[buf_index] = '\0';
    		// fprintf(stderr, "\nThread %ld got: '%s'\n",pthread_self(), buf);
    		if ( matchesDirOrFile(infos->dirorfile, buf, dirname, &localPath) == 1 )
    		{
    			matches_found++;
    			//add it to buffer/struct with: its ID,hostname,port
    			fprintf(stderr, "I need this dir!Mkdir: %s!\n", localPath);
    			mkdir(localPath, 0777);
    			free(localPath);
    		}
    		buf_index = 0;//reset for next result if there is one
    	}
    	else //keep saving
    	{
    		buf[buf_index++] = c;
    	}
    }

    //Afterwards, Content Server will send us the names of the files he contains
	//Add the ones we need in the buffer)

	//add record for this id in the idc_array
    pthread_mutex_lock(&mtx);
	if ( idc_array_add(idc_array, infos->id) != 0)
		exit(EXIT_FAILURE);
	pthread_mutex_unlock(&mtx);


    buf_index = 0;
    files_for_fetch = 0;
   	while ( (c = getc(socket_fp)) != EOF )
    {
    	//if we received one result (they end with '\n')
    	if (c == '\n')
    	{
    		buf[buf_index] = '\0';
    		// fprintf(stderr, "\nThread %ld got: '%s'\n",pthread_self(), buf);
    		if ( matchesDirOrFile(infos->dirorfile, buf, dirname, &localPath) == 1 )
    		{
    			matches_found++;
    			files_for_fetch++;
    			fprintf(stderr, "I need this!\n");
    			fprintf(stderr, "Local path is : %s\n", localPath);
    			//add it to buffer/struct with: its ID,hostname,port,localPath,remotePath(buf)
    			TSBuffer_push(ts_buffer, infos->host, infos->port, infos->id, localPath, buf);
    			pthread_cond_signal(&ts_buffer->not_empty);
			    
    			//increase counter for this id
			    pthread_mutex_lock(&mtx);
				idc_array_increase(idc_array, infos->id);
				pthread_mutex_unlock(&mtx);

    			free(localPath);
    			matches_found++;
    		}
    		buf_index = 0;//reset for next result if there is one
    	}
    	else //keep saving
    	{
    		buf[buf_index++] = c;
    	}
    }

    if (matches_found == 0)
    {
    	fprintf(stderr, "Content server '%s' does not contain dir/file '%s'!\n", infos->host, infos->dirorfile);
    	pthread_mutex_lock(&mtx);
    	numDevicesDone++;
    	if (numDevicesDone == total_devices)
    	{
    		pthread_cond_signal(&allDone);
    	}
    	pthread_mutex_unlock(&mtx);
    }
    //some matches were found, check if there are files to fetch (to count them)
    else if (files_for_fetch == 0)//no files to fetch (only dirs that were already created)
    {
    	pthread_mutex_lock(&mtx);
    	numDevicesDone++;
    	if (numDevicesDone == total_devices)
    	{
    		pthread_cond_signal(&allDone);
    	}
    	pthread_mutex_unlock(&mtx);
    }
    else
    {
    	pthread_mutex_lock(&mtx);
    	idc = idc_array_get(idc_array, infos->id);
    	if (idc == NULL)
    		exit(EXIT_FAILURE);

    	//the workers were so fast that they already fetched all the files!
    	//note that this device is done!
    	if (idc->counter == 0)
    	{
    		numDevicesDone++;
    		if (numDevicesDone == total_devices)
    		{
    			pthread_cond_signal(&allDone);
    		}
    	}
    	else//workers havent finished yet, let them know that this id's counter wont increase
    	{//so when they make it 0 they will note that the device is done
    		idc->wont_increase = 1;
    	}
    	pthread_mutex_unlock(&mtx);
    }

    free(infos->host);
    free(infos->dirorfile);
    free(infos->id);
    free(infos);
    /* Close socket */
    close(sock);
    fclose(socket_fp);

    pthread_exit(NULL);
}


void* worker_thread(void* wid)
{
    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;
    int* worker_id = (int*) wid;
    
    while (1)
    {
    	char* message;
    	Buffer_data buffer_element;
    	int sock;

	    /* Create socket */
    	if ( (sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    		perror_exit("socket");
    
		printf("\tWorker Thread %d Created socket %d\n", *worker_id, sock);

    	buffer_element = TSBuffer_pop(ts_buffer);
    	fprintf(stderr, "Popped buffer element!\n");
    	pthread_cond_signal(&ts_buffer->not_empty);

		
		/* Find server address */
	    if ((rem = gethostbyname(buffer_element.host)) == NULL) 
	    {	
		   herror("gethostbyname");
		   exit(1);
	    }
	    
	    server.sin_family = AF_INET;
	    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
	    server.sin_port = htons(buffer_element.port);
	    
	    printf("\tWorker Thread %d Connecting to %s port %d with socket %d\n", *worker_id, buffer_element.host, buffer_element.port, sock);
	    
	    /* Initiate connection */
	    if (connect(sock, serverptr, sizeof(server)) < 0)
		   perror_exit("connect-workerThread");
	    
	    message = malloc( (8 + strlen(buffer_element.id) + strlen(buffer_element.remote_path))*sizeof(char) );
	    sprintf(message, "FETCH %s %s", buffer_element.id, buffer_element.remote_path);
	    sendMessage(sock, message);

	    receiveFile	(sock, buffer_element.local_path);
    	
	    //check if this fetch was the last for this device (the device is matched to a specific ID)
	    pthread_mutex_lock(&mtx);
	    
	    int retVal;
	    int wont_increase;
	    //decrease counter for this id
	    retVal = idc_array_decrease(idc_array, buffer_element.id, &wont_increase);

	    	if (retVal == 0 && wont_increase == 1)//if we made the counter 0,and the counter for this id is final
	    {
	    	numDevicesDone++;
	    	if (numDevicesDone == total_devices)
	    	{
	    		pthread_cond_signal(&allDone);
	    	}
	    }

	    pthread_mutex_unlock(&mtx);

	    free(message);
	    free(buffer_element.host);
	    free(buffer_element.id);
	    free(buffer_element.local_path);
	    free(buffer_element.remote_path);
	 	close(sock);  	
	}
    	
	free(worker_id);
}

int main(int argc, char *argv[]) 
{
    int port, threadnum, err, status, i;
    pthread_t thr;
    int id = 1;//ID for LIST requests
    int* worker_count_ptr;
    pthread_t* thr_array;

    //initialise mtx cond_var and buffer
    pthread_mutex_init(&mtx, 0);
    pthread_cond_init(&allDone, 0);
    ts_buffer = TSBuffer_create(BUFFER_SIZE);
   
    //read arguments
    threadnum = 5;
    char host_name[BUFSIZE];
    dirname = "temp";
    mkdir(dirname, 0777); 
    total_devices = 3;
    char* dirorfile;

    //initialise array of file for fetch counters (for this session)
    idc_array = idc_array_create(total_devices);
    numDevicesDone = 0;

    //Create worker threads!
    for (i=0; i<threadnum; i++)
    {
    	worker_count_ptr = malloc(sizeof(int));
    	*worker_count_ptr = i + 1;
		if ( (err = pthread_create(&thr, NULL, worker_thread, (void *) worker_count_ptr)) )  
	    {
			perror2("pthread_create", err);
			exit(1);
		}
    }
    
    port = atoi(argv[1]);
    strcpy(host_name, "localhost");

    //RECEIVE MESSAGE FROM INITIATOR
    int request_index = 0;
    int requests_num = 2;
    thr_array = malloc(requests_num*sizeof(pthread_t));

    dirorfile = malloc(100*sizeof(char));
    strcpy(dirorfile, "/home/mt/Desktop/DI/Syspro/prj3/testdir/fileLefterisGamaeiBazolia");

    mirror_thread_params* infos = malloc(sizeof(mirror_thread_params));
    infos->host = malloc( (strlen(host_name) + 1)*sizeof(char) );
    strcpy(infos->host, host_name);
    infos->dirorfile = malloc( (strlen(dirorfile) + 1)*sizeof(char) );
    strcpy(infos->dirorfile, dirorfile);
    infos->port = port;
    infos->id = malloc(	50*sizeof(char) );
    sprintf(infos->id, "%s.%d", host_name, id);
    id++;
    infos->delay = 2;

    if ( (err = pthread_create(&(thr_array[request_index++]), NULL, mirror_thread, (void *) infos)) )  
    {
		perror2("pthread_create", err);
		exit(1);
	}

	infos = malloc(sizeof(mirror_thread_params));
    infos->host = malloc( (strlen(host_name) + 1)*sizeof(char) );
    strcpy(infos->host, host_name);
    strcpy(dirorfile, "prj3/testdir/dirLefterisOBabasSas");
    infos->dirorfile = malloc( (strlen(dirorfile) + 1)*sizeof(char) );
    strcpy(infos->dirorfile, dirorfile);
    infos->port = port;
    infos->id = malloc(	50*sizeof(char) );
    sprintf(infos->id, "%s.%d", host_name, id);
    id++;
    infos->delay = 3;

    if ( (err = pthread_create(&(thr_array[request_index++]), NULL, mirror_thread, (void *) infos)) )  
    {
		perror2("pthread_create", err);
		exit(1);
	}

	infos = malloc(sizeof(mirror_thread_params));
    infos->host = malloc( (strlen(host_name) + 1)*sizeof(char) );
    strcpy(infos->host, host_name);
    strcpy(dirorfile, "prj3/testdir/");
    infos->dirorfile = malloc( (strlen(dirorfile) + 1)*sizeof(char) );
    strcpy(infos->dirorfile, dirorfile);
    infos->port = port;
    infos->id = malloc(	50*sizeof(char) );
    sprintf(infos->id, "%s.%d", host_name, id);
    id++;
    infos->delay = 1;

    if ( (err = pthread_create(&(thr_array[request_index++]), NULL, mirror_thread, (void *) infos)) )  
    {
		perror2("pthread_create", err);
		exit(1);
	}

	for (request_index = 0; request_index < requests_num; request_index++)
	{
	
		if ( (err = pthread_join(thr_array[request_index], (void **) &status)) ) 
		{
			perror2("pthread_join", err); /* termination */
			exit(1);
		}
	}
	
	//printf("Thread %ld exited with code %d\n", thr, status);
	// TSBuffer_print(ts_buffer);
	while (numDevicesDone != total_devices)
	{
		fprintf(stderr, "Waiting for allDone!\n");
		pthread_cond_wait(&allDone, &mtx);
	}
	fprintf(stderr, "ALLDONE!!!!\n");
	idc_array_free(&idc_array);

	TSBuffer_free(&ts_buffer);

	free(dirorfile);
	free(thr_array);
    pthread_exit(NULL);
}		