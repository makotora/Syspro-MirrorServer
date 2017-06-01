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

#include "Buffer.h"
#include "Protocol.h"
#include "functions.h"
#include "structs.h"

#define BUFSIZE 1024
#define BUFFER_SIZE 30

pthread_mutex_t buff_mtx;
pthread_mutex_t idc_mtx;
pthread_mutex_t mtx;
pthread_cond_t allDone;
pthread_cond_t buff_not_empty;
pthread_cond_t buff_not_full;

int total_devices;
int numDevicesDone;
char* dirname;
//global structs
Buffer* buffer; //buffer
id_counters_array* idc_array;//array countaining a counter for files to fetch for each ID
int terminate;

void* mirror_thread(void* infos_par)
{
    mirror_thread_params* infos = (mirror_thread_params *) infos_par;
    char buf[BUFSIZE];
    //socket vars
    int sock;
    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;

    //Create socket and connect to the content server
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    	perror_exit("socket");

    rem = gethostbyname(infos->host);
    if (rem == NULL) 
    {	
	   herror("gethostbyname");
	   exit(1);
    }
    
    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(infos->port);
    
    if (connect(sock, serverptr, sizeof(server)) < 0)
	   perror_exit("connect");

    printf("\tMirror Thread Connected to %s port %d\n", infos->host, infos->port);
        	

    //Send LIST request
    sprintf(buf, "LIST %s %d", infos->id, infos->delay);
    fprintf(stderr, "\tMirrorThread: Sending '%s' to ContentServer!\n", buf);
    sendMessage(sock, buf);
    // fprintf(stderr, "MirrorThread: Looking for: '%s'\n", infos->dirorfile);
        	
	//get FILE pointer for socket
    FILE* socket_fp;
    socket_fp = fdopen(sock ,"r+");
    if (socket_fp == NULL)
        perror_exit("fdopen");

    char c;
    char* localPath;
    int buf_index, matches_found, files_for_fetch;
    id_counter* idc;
    matches_found = 0;

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
    		if ( matchesDirOrFile(infos->dirorfile, buf, infos->dirname, &localPath) == 1 )
    		{
    			matches_found++;
    			//add it to buffer/struct with: its ID,hostname,port
    			// fprintf(stderr, "I need this dir!Mkdir: %s!\n", localPath);
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
	//Add the ones we need in the buffer
	//Add record for this id in the idc_array.For each file pushed in the buffer, increment the counter for that id
    pthread_mutex_lock(&idc_mtx);
    	if ( idc_array_add(idc_array, infos->id) != 0)
    		exit(EXIT_FAILURE);
	pthread_mutex_unlock(&idc_mtx);


    buf_index = 0;
    files_for_fetch = 0;
   	while ( (c = getc(socket_fp)) != EOF )
    {
    	//if we received one result (they end with '\n')
    	if (c == '\n')
    	{
    		buf[buf_index] = '\0';
    		// fprintf(stderr, "\nThread %ld got: '%s'\n",pthread_self(), buf);
    		if ( matchesDirOrFile(infos->dirorfile, buf, infos->dirname, &localPath) == 1 )
    		{
                matches_found++;
                files_for_fetch++;

                //increase counter for this id
                pthread_mutex_lock(&idc_mtx);
                    idc_array_increase(idc_array, infos->id);
                pthread_mutex_unlock(&idc_mtx);
                
                // fprintf(stderr, "I need this!\n");
                // fprintf(stderr, "Local path is : %s\n", localPath);
                //add it to buffer/struct with: its ID,hostname,port,localPath,remotePath(buf)
                pthread_mutex_lock(&buff_mtx);

                    while(buffer->count >= buffer->size)
                    {
                        fprintf(stderr, "\tTHREAD %ld : FOUND BUFFER FULL!\n", pthread_self());
                        pthread_cond_wait(&buff_not_full, &buff_mtx);
                    }
                    Buffer_push(buffer, infos->host, infos->port, infos->id, localPath, buf);
                    pthread_cond_signal(&buff_not_empty);
                
                pthread_mutex_unlock(&buff_mtx);

    			free(localPath);
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
    //some matches were found, check if there are files to fetch
    else if (files_for_fetch == 0)//no files to fetch (only dirs that were already created)
    {
    	pthread_mutex_lock(&mtx);
        	numDevicesDone++;//done with this device (no file fetching!)
        	if (numDevicesDone == total_devices)
        	{
        		pthread_cond_signal(&allDone);
        	}
    	pthread_mutex_unlock(&mtx);
    }
    else
    {
        //this mutex is unlocked either in the if or the else below
    	pthread_mutex_lock(&idc_mtx);
        	idc = idc_array_get(idc_array, infos->id);

        	if (idc == NULL)
        		exit(EXIT_FAILURE);

            //the workers were so fast that they already fetched all the files!
        	//note that this device is done!
        	if (idc->counter == 0)
        	{
                pthread_mutex_unlock(&idc_mtx);//we are done with the idc.unlock to avoid deadlocks!
                //protect global var numDevicesDone
                pthread_mutex_lock(&mtx);
            		numDevicesDone++;
            		if (numDevicesDone == total_devices)
            		{
            			pthread_cond_signal(&allDone);
            		}
                pthread_mutex_unlock(&mtx);

        	}
        	else//workers havent finished yet, let them know that this id's counter wont increase
        	{//so when they make it 0 they will note that the device is done
                // printf("Setting wont increase\n");
        		idc->wont_increase = 1;
                pthread_mutex_unlock(&idc_mtx);
        	}
    }

    /* Close socket */
    fclose(socket_fp);
    socket_fp = NULL;
    close(sock);
    sock = -1;

    //free stuff
    free(infos->host);
    free(infos->dirname);
    free(infos->dirorfile);
    free(infos->id);
    free(infos);

    pthread_exit(NULL);
}


void* worker_thread(void* null)
{   
    char buf[BUFSIZE];
    //socket vars
    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;
    int sock;
    
    while (1)
    {
    	Buffer_data buffer_element;

    	pthread_mutex_lock(&buff_mtx);
            while (buffer->count <= 0 && terminate == 0)
            {
                printf("\t\tTHREAD %ld : FOUND BUFFER EMPTY!\n", pthread_self());
                pthread_cond_wait(&buff_not_empty, &buff_mtx);
            }


            if (terminate == 1)
            {//terminating!(received broadcast signal for unblock from buffer and found 'terminate' var 1!)
                pthread_mutex_unlock(&buff_mtx);
                printf("\t\tPopped nothing!\n");
                break;
            }

            buffer_element = Buffer_pop(buffer);

            fprintf(stderr, "\t\tPopped buffer element!\n");
            Buffer_print_element(buffer_element);
            pthread_cond_signal(&buff_not_full);
        
		pthread_mutex_unlock(&buff_mtx);

	
        //Create socket and connect to content server (according to the info popped from buffer)
    	sock = socket(AF_INET, SOCK_STREAM, 0);
        if ( sock < 0 )
    		perror_exit("socket");
    
		printf("\t\tWorker Thread %d Created socket %d\n", pthread_self(), sock);

        rem = gethostbyname(buffer_element.host);
	    if (rem == NULL) 
	    {	
		   herror("gethostbyname");
		   exit(1);
	    }
	    
	    server.sin_family = AF_INET;
	    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
	    server.sin_port = htons(buffer_element.port);

	    printf("\t\tWorker Thread %d Connecting to %s port %d with socket %d\n", pthread_self(), buffer_element.host, buffer_element.port, sock);
        
        if (connect(sock, serverptr, sizeof(server)) < 0)
           perror_exit("connect-workerThread");
	    
	    //Send FETCH request to content
	    sprintf(buf, "FETCH %s %s", buffer_element.id, buffer_element.remote_path);
	    sendMessage(sock, buf);

        //Receive the file
	    receiveFile(sock, buffer_element.local_path);
    	
	    //check if this fetch was the last for this device (the device is matched to a specific ID)
	    int id_counter;
	    int wont_increase;

        pthread_mutex_lock(&idc_mtx);
	       id_counter = idc_array_decrease(idc_array, buffer_element.id, &wont_increase);
        pthread_mutex_unlock(&idc_mtx);

        if (id_counter == 0 && wont_increase == 1)//if we made the counter 0,and the counter for this id is final
        {
            // printf("\t\tWONT INCREASE!\n");
            pthread_mutex_lock(&mtx);
                numDevicesDone++;
                if (numDevicesDone == total_devices)
                {
                    pthread_cond_signal(&allDone);
                }
            pthread_mutex_unlock(&mtx);
        }

        //free buffer element popped
        free(buffer_element.host);
        free(buffer_element.id);
        free(buffer_element.local_path);
        free(buffer_element.remote_path);
        
        close(sock);
	 	sock = -1;
	}

    printf("\t\tWorker %d exiting\n", pthread_self());
	pthread_detach(pthread_self());
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) 
{
    int port, threadnum, err, status, i;
    int id = 1;//ID for LIST requests
    pthread_t thr;
    pthread_t* mthr_array;

    //initialise mtxs cond_vars andm buffer
    pthread_mutex_init(&buff_mtx, 0);
    pthread_mutex_init(&idc_mtx, 0);
    pthread_mutex_init(&mtx, 0);
    pthread_cond_init(&allDone, 0);
    pthread_cond_init(&buff_not_empty, 0);
    pthread_cond_init(&buff_not_full, 0);
    terminate = 0;

    buffer = Buffer_create(BUFFER_SIZE);
   
    //read arguments
    threadnum = 3;
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
		if ( (err = pthread_create(&thr, NULL, worker_thread, (void *) NULL)) )  
	    {
			perror2("pthread_create", err);
			exit(1);
		}
    }
    
    port = atoi(argv[1]);
    strcpy(host_name, "localhost");

    //RECEIVE MESSAGE FROM INITIATOR
    int request_index = 0;
    int requests_num = 3;
    mthr_array = malloc(requests_num*sizeof(pthread_t));

    dirorfile = malloc(100*sizeof(char));
    strcpy(dirorfile, "/home/mt/Desktop/DI/Syspro/prj3/testdir/fileLefterisGamaeiBazolia");

    mirror_thread_params* infos = malloc(sizeof(mirror_thread_params));
    infos->dirname = strdup(dirname);
    infos->host = malloc( (strlen(host_name) + 1)*sizeof(char) );
    strcpy(infos->host, host_name);
    infos->dirorfile = malloc( (strlen(dirorfile) + 1)*sizeof(char) );
    strcpy(infos->dirorfile, dirorfile);
    infos->port = port;
    infos->id = malloc(	50*sizeof(char) );
    sprintf(infos->id, "%s.%d", host_name, id);
    id++;
    infos->delay = 2;

    if ( (err = pthread_create(&(mthr_array[request_index++]), NULL, mirror_thread, (void *) infos)) )  
    {
		perror2("pthread_create", err);
		exit(1);
	}

	infos = malloc(sizeof(mirror_thread_params));
    infos->dirname = strdup(dirname);
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

    if ( (err = pthread_create(&(mthr_array[request_index++]), NULL, mirror_thread, (void *) infos)) )  
    {
		perror2("pthread_create", err);
		exit(1);
	}

	infos = malloc(sizeof(mirror_thread_params));
    infos->dirname = strdup(dirname);
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

    if ( (err = pthread_create(&(mthr_array[request_index++]), NULL, mirror_thread, (void *) infos)) )  
    {
		perror2("pthread_create", err);
		exit(1);
	}

	for (request_index = 0; request_index < requests_num; request_index++)
	{
		if ( (err = pthread_join(mthr_array[request_index], (void **) &status)) ) 
		{
			perror2("pthread_join", err); /* termination */
			exit(1);
		}
	}
	
	pthread_mutex_lock(&mtx);
    	while (numDevicesDone != total_devices)
    	{
    		fprintf(stderr, "Waiting for allDone!\n");
    		pthread_cond_wait(&allDone, &mtx);
    	}
	pthread_mutex_unlock(&mtx);
    

    fprintf(stderr, "About to terminate workers!\n");

    pthread_mutex_lock(&buff_mtx);
        terminate = 1;//set terminate var to 1,then broadcast for blocked workers (on pop) to wake up!
        pthread_cond_broadcast(&buff_not_empty);
    pthread_mutex_unlock(&buff_mtx);

    fprintf(stderr, "--> ALLDONE!\n");


    idc_array_free(&idc_array);
	Buffer_free(&buffer);

	free(dirorfile);
	free(mthr_array);
    pthread_exit(NULL);
}		