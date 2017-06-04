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
#define PRINTS 0
#define PRINTS2 1

int sock;
char* fileFullPath;
delays* delays_list;
int terminate;
pthread_mutex_t mtx;

void* thread_content_server(void* socket) 
{
    //to be detached
    pthread_detach(pthread_self());

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

    if (PRINTS2)
        fprintf(stderr, "\tContentServerThread received: '%s'\n", buf);
    
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
    	pthread_mutex_lock(&mtx);
        	delays_add(delays_list, id, delay);
    	pthread_mutex_unlock(&mtx);

        if (fileFullPath != NULL)//If this content server is in charge of a single file
        {
            //just send back the full path of that file..thats it.Thats all we got

            //mirrorServer is expecting some dirs and then EOF.just send EOF
            putc(EOF, socket_fp);

            //than he is expecting some files paths ending with \n(we got only one) and then EOF
            int i, max;
            max = strlen(fileFullPath);

            for (i=0; i<max; i++)
                putc(fileFullPath[i], socket_fp);

            putc('\n', socket_fp);
            putc(EOF, socket_fp);
            //thats it 
        }
        else //if we are in charge of a whole directory.We already chdir'ed to it
        {
        	//First find all the directories and send their names
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
        }

    	fclose(socket_fp);
    	free(id);
    }
    //if its a FETCH request
    else if ( !strcmp(token, "FETCH") )
    {
        if (PRINTS)
    	   fprintf(stderr, "\tContentServerThread: Dealing with a FETCH request!\n");

    	token = strtok_r(NULL, " ", &savePtr);//next token is the ID of the request
    	id = strdup(token);

    	pthread_mutex_lock(&mtx);
    	delay = delays_get_by_id(delays_list, id);
    	pthread_mutex_unlock(&mtx);

    	if (delay == -1)
    	{
    		fprintf(stderr, "Fetch error! No delay found for that id!\n");
    		exit(EXIT_FAILURE);
    	}

    	if (PRINTS)
            fprintf(stderr, "\tContentServerThread: Delay for this fetch is : %d\n", delay);

    	//last token is the path to the file (full path) for fetching
    	token = strtok_r(NULL, "", &savePtr);
    	path = strdup(token);


        if (PRINTS2)
            fprintf(stderr, "\tContentServerThread: Sending file %s\n", path);        
    	
        //first sleep for delay seconds,than send the file
        sleep(delay);
        sendFile(newsock, path, BUFSIZE);

    	free(id);
    	free(path);
    }
    else if ( !strcmp(token, "KYS") )//If he told me to kill my self
    {
        if (PRINTS2)
        {
            fprintf(stderr, "ContentServerThread: Received a KYS request!\n");
            fprintf(stderr, "ContentServerThread: About to terminate me and ContentServer!\n");
        }
        
        pthread_mutex_lock(&mtx);
            terminate = 1;
            shutdown(sock, SHUT_RDWR);//Brutally cancel possible stuck 'accept' calls so the content server can terminate
        pthread_mutex_unlock(&mtx);
    }
    else
    {
    	fprintf(stderr, "<!>ContentServerThread: Received an unknown request! : '%s'", token);
    }

    close(newsock);
    newsock = -1;
    free(socketPtr);

    pthread_exit(NULL);
}


int main(int argc, char *argv[]) 
{
	//malloc for global lists
	delays_list = delays_create();
	pthread_mutex_init(&mtx, 0);

    struct sockaddr_in server, client;
    socklen_t clientlen = sizeof(client);
    struct sockaddr *serverptr=(struct sockaddr *)&server;
    struct sockaddr *clientptr=(struct sockaddr *)&client;
    struct hostent *rem;
    
    //Read arguments....
    int port = -1;
    char* dirorfilename = NULL;
    
    int index = 1;

    //Read arguments
    while (index + 1 
        < argc)
    {
        if ( !strcmp(argv[index], "-p") )
        {
            port = atoi(argv[index + 1]);
            index++;
        }
        else if ( !strcmp(argv[index], "-d") )
        {
            dirorfilename = argv[index + 1];
            index++;
        }
        else
        {
            fprintf(stderr, "Skipping unknown argument '%s'.\n", argv[index]);
        }

        index++;
    }


    if (port == -1)
    {
        fprintf(stderr, "Error! Argument port was not given!\n");
        fprintf(stderr, "Usage: './ContentServer -p <port> -d <dirorfilename>'\n");
        return -1;
    }
    else if (dirorfilename == NULL)
    {
        fprintf(stderr, "Error! Argument dirorfilename was not given!\n");
        fprintf(stderr, "Usage: './ContentServer -p <port> -d <dirorfilename>'\n");
        return -1;
    }


    struct stat path_stat;
    stat(dirorfilename, &path_stat);
    if ( S_ISREG(path_stat.st_mode) ) //If content server is in charge of a single file
    {
        fileFullPath = malloc(100*sizeof(char));
        realpath(dirorfilename, fileFullPath);
    }
    else//change dir to the directory this content server is "in charge" of
    {
        fileFullPath = NULL;
        chdir(dirorfilename);
    }

    //Create socket for mirrorServer requests
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 )
        perror_exit("socket");

    server.sin_family = AF_INET;       /* Internet domain */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);      /* The given port */
    
    if (bind(sock, serverptr, sizeof(server)) < 0)
        perror_exit("bind");
    
    if (listen(sock, LISTEN_QUEUE_SIZE) < 0)
    	perror_exit("listen");

    if (PRINTS)    
        printf("ContentServer: Listening for connections to port %d\n", port);
    terminate = 0;

    while (terminate == 0) 
    { 
    	clientlen = sizeof(client);
    	int* newsock = malloc(sizeof(int));

    	if ((*newsock = accept(sock, clientptr, &clientlen)) < 0)
        {
            if (terminate == 1)
            {
                free(newsock);
                break;
            }
            else
                perror_exit("accept");
        }
    	
   		if ((rem = gethostbyaddr((char *) &client.sin_addr.s_addr, sizeof(client.sin_addr.s_addr), client.sin_family)) == NULL) 
   		{
  	    	herror("gethostbyaddr");
  	    	exit(1);
  	    }
   		
    	pthread_t thr;
    	int err;

    	if ( (err = pthread_create(&thr, NULL, thread_content_server, (void *) newsock)) )  
    	{
			perror2("pthread_create", err);
			exit(1);
		}
    	
    }

    if (PRINTS)
    {
        fprintf(stderr, "ContentServer: Terminating!\n");
    }

    if( fileFullPath != NULL)
        free(fileFullPath);

    delays_free(&delays_list);
    close(sock);
    pthread_exit(NULL);
}