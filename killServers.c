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

#include "Protocol.h"
#include "functions.h"

int main(void)
{
	struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;
    int sock;

    char line[100];
    int max_len = 100;

    printf("Give 'host_name:port' of server you want to kill!\n");
    printf("Give 'exit' to exit killServers program!\n");
    printf(": ");

    while (fgets(line, max_len, stdin) != NULL)
    {
    	//remove \n from line if there is one 
    	int lineSize = strlen(line);
    	if ( line[lineSize - 1] == '\n')
    		line[lineSize - 1] = '\0';

    	if ( !strcmp(line, "exit") )
    		break;

    	char* host_name;
    	int port;

   		char* token;

   		token = strtok(line, ":");
   		if (token != NULL)
   		{
   			host_name = strdup(token);
   		}
   		else
   		{
   			printf("Invalid input.Give 'host_name:port' or 'exit'\n");
   			printf(": ");
   			continue;
   		}

   		token = strtok(NULL, " ");

   		if (token != NULL)
   		{
   			port = atoi(token);
   		}
   		else
   		{
   			free(host_name);
   			printf("Invalid input.Give 'host_name:port' or 'exit'\n");
   			printf(": ");
   			continue;
   		}

   		//input was okay.connect to server and send KillYourSelf request

	   	sock = socket(AF_INET, SOCK_STREAM, 0);
	    if ( sock < 0 )
			perror_exit("socket");

	    rem = gethostbyname(host_name);
	    if (rem == NULL) 
	    {	
		   herror("gethostbyname");
		   exit(1);
	    }
	    
	    server.sin_family = AF_INET;
	    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
	    server.sin_port = htons(port);
	    
	    if (connect(sock, serverptr, sizeof(server)) < 0)
	       perror_exit("connect-Initiator");

	   	sendMessage(sock, "KYS");
	   	printf("Sent KillYourSelf request to server '%s' at port '%d'\n", host_name, port);
	   	printf(": ");

   		close(sock);
   		free(host_name);
    }

    return 0;

}