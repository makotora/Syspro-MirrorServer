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

#define LISTEN_QUEUE_SIZE 10
#define BUFSIZE 1024

void sigchld_handler (int sig) 
{
	while (waitpid(-1, NULL, WNOHANG) > 0);
}

void perror_exit(char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

void child_server(int newsock) {
    char buf[BUFSIZE];

    if (read(newsock, buf, BUFSIZE) < 0)
    	perror_exit("read");

    fprintf(stderr, "ContentServer: Received '%s' from MirrorServer!\n", buf);
    
	strcpy(buf, "OK");
    fprintf(stderr, "ContentServer: Sending '%s' to MirrorServer!\n", buf);
    
    if (write(newsock, buf, BUFSIZE) < 0)
    	perror_exit("write");
        	

    printf("Closing connection.\n");
    close(newsock);	  /* Close socket */
}

int main(int argc, char *argv[]) 
{
    int port, sock, newsock;
    struct sockaddr_in server, client;
    socklen_t clientlen;
    struct sockaddr *serverptr=(struct sockaddr *)&server;
    struct sockaddr *clientptr=(struct sockaddr *)&client;
    struct hostent *rem;
    
    //Read arguments....
    char dirorfilename[100];
    strcpy(dirorfilename, "/");
    port = 9000;	

    signal(SIGCHLD, sigchld_handler);
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
        /* accept connection */
    	if ((newsock = accept(sock, clientptr, &clientlen)) < 0) perror_exit("accept");
    	
    	/* Find client's address */
   		if ((rem = gethostbyaddr((char *) &client.sin_addr.s_addr, sizeof(client.sin_addr.s_addr), client.sin_family)) == NULL) 
   		{
  	    	herror("gethostbyaddr");
  	    	exit(1);
  	    }
   	
   		printf("Accepted connection from '%s'\n", rem->h_name);
    	printf("Accepted connection\n");
    	
    	switch (fork()) 
    	{    /* Create child for serving client */
    		case -1:     /* Error */
    	    	perror("fork");
    	    	break;
    		case 0:	     /* Child process */
    	    	close(sock);
    	    	child_server(newsock);
    	    	exit(0);
    	}
    	
    	close(newsock); /* parent closes socket to client */
    }

    return 0;
}