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

TSBuffer* ts_buffer;
pthread_mutex_t mtx;
int total_devices;
int numDevicesDone = 0;
char* dirname;

void* mirror_thread(void* infos_par)
{
	mirror_thread_params* infos = (mirror_thread_params *) infos_par;
	int sock;
    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;
    char buf[BUFSIZE];
    int matches_found;

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
    buf_index = 0;
    matches_found = 0;
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
    			fprintf(stderr, "I need this!\n");
    			fprintf(stderr, "Local path is : %s\n", localPath);
    			//add it to buffer/struct with: its ID,hostname,port
    			TSBuffer_push(ts_buffer, infos->host, infos->port, infos->id, localPath, buf);
    			pthread_cond_signal(&ts_buffer->not_empty);
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
    }

    free(infos->host);
    free(infos->dirorfile);
    free(infos->id);
    free(infos);
    fclose(socket_fp);
    close(sock);/* Close socket */

    pthread_exit(NULL);
}


void* worker_thread(void* wid)
{
	int sock;
    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;
    int* worker_id = (int*) wid;

    while (1)
    {
    	char* message;
    	char c;
    	FILE* socket_fp;
    	FILE* newFile;
    	Buffer_data buffer_element;

    	buffer_element = TSBuffer_pop(ts_buffer);
    	fprintf(stderr, "Popped buffer element!\n");
    	pthread_cond_signal(&ts_buffer->not_empty);

	    /* Create socket */
	    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	    	perror_exit("socket");

		/* Find server address */
	    if ((rem = gethostbyname(buffer_element.host)) == NULL) 
	    {	
		   herror("gethostbyname");
		   exit(1);
	    }

	    socket_fp = fdopen(sock ,"r+");
	    if (socket_fp == NULL)
			perror_exit("fdopen");
	    
	    server.sin_family = AF_INET;
	    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
	    server.sin_port = htons(buffer_element.port);
	    
	    /* Initiate connection */
	    if (connect(sock, serverptr, sizeof(server)) < 0)
		   perror_exit("connect");
	    
	    printf("Worker Thread %d Connecting to %s port %d\n", *worker_id, buffer_element.host, buffer_element.port);
	    
	    message = malloc( (8 + strlen(buffer_element.id) + strlen(buffer_element.remote_path))*sizeof(char) );
	    sprintf(message, "FETCH %s %s", buffer_element.id, buffer_element.remote_path);
	    sendMessage(sock, message);

	    newFile = fopen(buffer_element.local_path, "w");
	    while ( (c = getc(socket_fp)) != EOF )
	    {
	    	putc(c, newFile);
	    }
    	
    	fclose(newFile);
	    fclose(socket_fp);
	    free(message);
	}

	free(worker_id);
}

int main(int argc, char *argv[]) 
{
    int port, threadnum, err, status, i;
    pthread_t thr;
    int id = 1;//ID for LIST requests
    int* worker_count_ptr;

    //initialise mtx and structs
    pthread_mutex_init(&mtx, 0);
    thread_infos* thread_infos_list = thread_infos_create();
    ts_buffer = TSBuffer_create(BUFFER_SIZE);
   
    //read arguments
    threadnum = 3;
    char host_name[BUFSIZE];
    dirname = "temp";
    mkdir(dirname, 0777); 
    total_devices = 2;
    char* dirorfile;

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
    threadnum = 5;
    strcpy(host_name, "localhost");
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

    if ( (err = pthread_create(&thr, NULL, mirror_thread, (void *) infos)) )  
    {
		perror2("pthread_create", err);
		exit(1);
	}
	thread_infos_add(thread_infos_list, thr);

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

    if ( (err = pthread_create(&thr, NULL, mirror_thread, (void *) infos)) )  
    {
		perror2("pthread_create", err);
		exit(1);
	}
	thread_infos_add(thread_infos_list, thr);


	thread_info* current = thread_infos_list->first;

	while (current != NULL)
	{

		if ( (err = pthread_join(current->thr, (void **) &status)) ) 
		{
			perror2("pthread_join", err); /* termination */
			exit(1);
		}

		current = current->next;
	}
	
	//printf("Thread %ld exited with code %d\n", thr, status);
	TSBuffer_print(ts_buffer);
	sleep(2);

	thread_infos_free(&thread_infos_list);
	TSBuffer_free(&ts_buffer);
	free(dirorfile);
    pthread_exit(NULL);
}		