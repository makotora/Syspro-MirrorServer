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

#include "Protocol.h"
#include "functions.h"
#include "structs.h"

#define LISTEN_QUEUE_SIZE 20
#define BUFSIZE 1024

delays* delays_list;
pthread_mutex_t delays_mtx;

void* thread_content_server(void* socket) 
{
    char buf[BUFSIZE];
    char command[18];
    char* token;
    char* savePtr;
    char* id;
    char* path;
    char c;
    int delay;
    FILE* popen_fp;
   	FILE* socket_fp;

    int* socketPtr = (int*) socket;
    int newsock = *socketPtr;

    //Receive request message
    receiveMessage(newsock, buf);
    fprintf(stderr, "\nContentServerThread: Received '%s' from MirrorServer!\n", buf);
    
    //get type of request
    token = strtok_r(buf, " ", &savePtr);

    
    //If it is a LIST request
    if ( !strcmp(token, "LIST") )
    {
    	token = strtok_r(NULL, " ", &savePtr);//get id
    	id = strdup(token);
    	token = strtok_r(NULL, " ", &savePtr);//get delay
    	delay = atoi(token);

    	socket_fp = fdopen(newsock ,"r+");
    		if (socket_fp == NULL)
    			perror_exit("fdopen");

    	//add delay to delays list
    	pthread_mutex_lock(&delays_mtx);
        	delays_add(delays_list, id, delay);
    	pthread_mutex_unlock(&delays_mtx);


    	//First find the directories and send their names
    	sprintf(command, "find $PWD -type d");

    	popen_fp = popen(command, "r");
    	if (popen_fp == NULL)
    		perror_exit("popen");

    	while ( (c = getc(popen_fp)) != EOF )
    	{
    		putc(c, socket_fp);
    	}
    	putc(EOF, socket_fp);

    	pclose(popen_fp);


    	//Then,find the files and send their names
    	sprintf(command, "find $PWD -type f");

    	popen_fp = popen(command, "r");
    	if (popen_fp == NULL)
    		perror_exit("popen");

    	while ( (c = getc(popen_fp)) != EOF )
    	{
    	    putc(c, socket_fp);
    	}
    	putc(EOF, socket_fp);

    	pclose(popen_fp);
    	fclose(socket_fp);
    	free(id);
    }
    //if its a FETCH request
    else if ( !strcmp(token, "FETCH") )
    {
    	fprintf(stderr, "ContentServer thread: Received a FETCH request!\n");
    	token = strtok_r(NULL, " ", &savePtr);//next token is the ID of the request
    	fprintf(stderr, "\ttoken1 %s\n", token);
    	id = strdup(token);

    	pthread_mutex_lock(&delays_mtx);
    	delay = delays_get_by_id(delays_list, id);
    	pthread_mutex_unlock(&delays_mtx);

    	if (delay == -1)
    	{
    		fprintf(stderr, "Fetch error! No delay found for that id!\n");
    		exit(EXIT_FAILURE);
    	}

    	fprintf(stderr, "Delay for this fetch is : %d\n", delay);
        // sleep(delay);
    	//last token is the path to the file (full path) for fetching
    	token = strtok_r(NULL, " ", &savePtr);
    	fprintf(stderr, "\ttoken2 %s\n", token);
    	path = strdup(token);

    	sendFile(newsock, path, BUFSIZE);

    	free(id);
    	free(path);
    }
    else
    {
    	fprintf(stderr, "ContentServer thread: Received an unknown request! : '%s'", token);
    }
    
    printf("Closing connection.\n\n");

    close(newsock);	  /* Close socket */
    newsock = -1;
    free(socketPtr);

    //detach!
    pthread_detach(pthread_self());
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) 
{
	//malloc for global lists
	char dirorfilename[50];
	delays_list = delays_create();
	pthread_mutex_init(&delays_mtx, 0);

    int port, sock;
    struct sockaddr_in server, client;
    socklen_t clientlen;
    struct sockaddr *serverptr=(struct sockaddr *)&server;
    struct sockaddr *clientptr=(struct sockaddr *)&client;
    struct hostent *rem;
    
    //Read arguments....
    strcpy(dirorfilename, "/home/mt/Desktop/DI/Syspro/prj3/testdir");
    port = atoi(argv[1]);
    chdir(dirorfilename);

    /* Create socket */
    if ( (sock = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
        perror_exit("socket");

    server.sin_family = AF_INET;       /* Internet domain */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);      /* The given port */
    
    /* Bind socket to address */
    if (bind(sock, serverptr, sizeof(server)) < 0)
        perror_exit("bind");
    
    /* Listen for connections */
    if (listen(sock, LISTEN_QUEUE_SIZE) < 0)
    	perror_exit("listen");
    
    printf("Listening for connections to port %d\n", port);

    while (1) 
    { 
    	clientlen = sizeof(client);
    	int* newsock = malloc(sizeof(int));
        /* accept connection */
    	if ((*newsock = accept(sock, clientptr, &clientlen)) < 0) perror_exit("accept");
    	
    	/* Find client's address */
   		if ((rem = gethostbyaddr((char *) &client.sin_addr.s_addr, sizeof(client.sin_addr.s_addr), client.sin_family)) == NULL) 
   		{
  	    	herror("gethostbyaddr");
  	    	exit(1);
  	    }
   	
   		printf("\nAccepted connection from '%s'\n", rem->h_name);

    	
    	pthread_t thr;
    	int err;

    	if ( (err = pthread_create(&thr, NULL, thread_content_server, (void *) newsock)) )  
    	{
			perror2("pthread_create", err);
			exit(1);
		}
    	
    }

    delays_free(&delays_list);
    close(sock);

    pthread_exit(NULL);
}