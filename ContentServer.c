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

#define LISTEN_QUEUE_SIZE 10
#define BUFSIZE 1024
#define perror2(s,e) fprintf(stderr, "%s: %s\n", s, strerror(e))

char dirorfilename[50];
delays* delays_list;
thread_infos* thread_infos_list;

void* thread_server(void* socket) 
{
	fprintf(stderr, "IN THREAD SERVER FUNCTION!\n");

    char buf[BUFSIZE];
    char command[50];
    char* token;
    int id,delay;
    FILE* popen_fp;
   	FILE* socket_fp;

    int* socketPtr = (int*) socket;
    int newsock = *socketPtr;

    receiveMessage(newsock, buf);

    fprintf(stderr, "ContentServer: Received '%s' from MirrorServer!\n", buf);
    
    //get type of request
    token = strtok(buf, " ");

    if ( !strcmp(token, "LIST") )
    {
    	printf("here\n");
    	token = strtok(NULL, " ");//get id
    	id = atoi(token);
    	token = strtok(NULL, " ");//get delay
    	delay = atoi(token);

    	//add delay to delays list
//ADD MUTEX AND CONDITION VARIABLE
    	delays_add(delays_list, id, delay);

    	sprintf(command, "find %s", dirorfilename);

    	socket_fp = fdopen(newsock ,"r+");
    	if (socket_fp == NULL)
    		perror_exit("fdopen");

    	popen_fp = popen(command, "r");
    	if (popen_fp == NULL)
    		perror_exit("popen");

    	char c;
    	while ( (c = getc(popen_fp)) != EOF )
    	{
    		putc(c, socket_fp);
    	}
    	putc(EOF, socket_fp);
    }
    else if ( !strcmp(token, "FETCH") )
    {
    	;
    }
    else
    {
    	strcpy(buf, "ERROR");
    }
    
    printf("Closing connection.\n");

    pclose(popen_fp);
    fclose(socket_fp);
    close(newsock);	  /* Close socket */
    free(socketPtr);

    //detach!
    pthread_detach(pthread_self());
}

int main(int argc, char *argv[]) 
{
	//malloc for global lists
	delays_list = delays_create();
	thread_infos* thread_infos_list = thread_infos_create();


    int port, sock;
    struct sockaddr_in server, client;
    socklen_t clientlen;
    struct sockaddr *serverptr=(struct sockaddr *)&server;
    struct sockaddr *clientptr=(struct sockaddr *)&client;
    struct hostent *rem;
    
    printf("Here\n");
    //Read arguments....
    strcpy(dirorfilename, "/home/mt/Desktop/DI/Syspro/prj3/testdir");
    port = atoi(argv[1]);

    /* Create socket */
    if ( (sock = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
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
   	
   		printf("Accepted connection from '%s'\n", rem->h_name);
    	printf("Accepted connection\n");

    	
    	pthread_t thr;
    	int err;

    	if ( (err = pthread_create(&thr, NULL, thread_server, (void *) newsock)) )  
    	{
			perror2("pthread_create", err);
			exit(1);
		}
    	
    }

    delays_free(&delays_list);
    thread_infos_free(&thread_infos_list);
    close(sock);

    return 0;
}