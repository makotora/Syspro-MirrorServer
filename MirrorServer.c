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

#define BUFSIZE 1024

void perror_exit(char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) 
{
    int port, sock, i, threadnum;
    char buf[BUFSIZE];
    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;
   
    //read arguments
    char host_name[BUFSIZE];
    char dirname[BUFSIZE]; 
    
    port = 9000;
    threadnum = 5;
    strcpy(host_name, "localhost");
    strcpy(dirname, "/home/mt/Desktop/DI");

	/* Create socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    	perror_exit("socket");

    printf("Here\n");
    fflush(stdout);
	/* Find server address */
    if ((rem = gethostbyname(host_name)) == NULL) 
    {	
	   herror("gethostbyname");
	   exit(1);
    }
    
    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(port);
    
    /* Initiate connection */
    if (connect(sock, serverptr, sizeof(server)) < 0)
	   perror_exit("connect");
    
    printf("Connecting to %s port %d\n", argv[1], port);
        	

    fprintf(stderr, "MirrorServer: Sending dirname '%s' to ContentServer!\n", dirname);
    
    if (write(sock, dirname, BUFSIZE) < 0)
    	perror_exit("write");
        	
    if (read(sock, buf, BUFSIZE) < 0)
    	perror_exit("read");

    fprintf(stderr, "MirrorServer: Received '%s' from ContentServer!\n", buf);
    
    close(sock);/* Close socket */

    return 0;
}		