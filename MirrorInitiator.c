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

#include "functions.h"
#include "Protocol.h"

#define BUFSIZE 1024

int main(int argc, char* argv[])
{
	char* mirrorAddress = NULL;
	int mirrorPort = -1;
	char* requests = NULL;
	
	int index = 1;

	//Read arguments
	while (index + 1 < argc)
	{
		if ( !strcmp(argv[index], "-n") )
		{
			mirrorAddress = argv[index + 1];
			index++;
		}
		else if ( !strcmp(argv[index], "-p") )
		{
			mirrorPort = atoi(argv[index + 1]);
			index++;
		}
		else if ( !strcmp(argv[index], "-s") )
		{
			requests = argv[index + 1];
			index++;
		}
		else
		{
			fprintf(stderr, "Skipping unknown argument '%s'.\n", argv[index]);
		}

		index++;
	}


	if (mirrorAddress == NULL)
	{
		fprintf(stderr, "Error! Argument MirrorServerAddress was not given!\n");
		fprintf(stderr, "Usage: './MirrorInitiator -n <MirrorServerAddress> -p <MirrorServerPort> \\\\\n-s <ContentServerAddress1:ContentServerPort1:dirorfile1:delay1, \\\\\\\nContentServerAddress2:ContentServerPort2:dirorfile2:delay2, ...>'\n");
		return -1;
	}
	else if (mirrorPort == -1)
	{
		fprintf(stderr, "Error! Argument MirrorServerPort was not given!\n");
		fprintf(stderr, "Usage: './MirrorInitiator -n <MirrorServerAddress> -p <MirrorServerPort> \\\\\n-s <ContentServerAddress1:ContentServerPort1:dirorfile1:delay1, \\\\\\\nContentServerAddress2:ContentServerPort2:dirorfile2:delay2, ...>'\n");
		return -1;
	}
	else if (requests == NULL)
	{
		fprintf(stderr, "Error! No requests given!\n");
		fprintf(stderr, "Usage: './MirrorInitiator -n <MirrorServerAddress> -p <MirrorServerPort> \\\\\n-s <ContentServerAddress1:ContentServerPort1:dirorfile1:delay1, \\\\\\\nContentServerAddress2:ContentServerPort2:dirorfile2:delay2, ...>'\n");
		return -1;
	}


	//Create socket and establish connection with the MirrorServer
	char buf[BUFSIZE];
    //socket vars
    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;
    int sock;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 )
		perror_exit("socket");

    rem = gethostbyname(mirrorAddress);
    if (rem == NULL) 
    {	
	   herror("gethostbyname");
	   exit(1);
    }
    
    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(mirrorPort);

    printf("Initiator Connecting to %s port %d with socket %d\n", mirrorAddress, mirrorPort, sock);
    
    if (connect(sock, serverptr, sizeof(server)) < 0)
       perror_exit("connect-Initiator");


   	//Get requests from arg given (tokenise) and send them to the mirrorServer
	char* token = strtok(requests, ",");//get first request
	int tokenSize;
	int counter;

	//send start of requests (SOR) message
	sendMessage(sock, "SOR");
	while (token != NULL)
	{
		//check if it is a valid request (has exactly 3 ':')
		counter = 0;
		tokenSize = strlen(token);
		for (index=0; index<tokenSize; index++)
			if (token[index] == ':')
				counter++;

		if (counter == 3)
		{
			fprintf(stderr, "Sending request '%s' to MirrorServer!\n", token);
			sendMessage(sock, token);
		}
		else
		{
			fprintf(stderr, "Skipping invalid request '%s'!\n", token);	
		}
		token = strtok(NULL, ",");//get next request (if there is one)
	}
	//send end of requests (EOR) message
	sendMessage(sock, "EOR");

	receiveMessage(sock, buf);
	if ( !strcmp(buf, "STATS"))
	{
		receiveMessage(sock, buf);
		fprintf(stderr, "Received stats from MirrorServer\n%s\n", buf);	
	}
	else
	{
		fprintf(stderr, "Received an unexpected reply:\n'%s'\n", buf);	
	}

	close(sock);

	return 0;
}