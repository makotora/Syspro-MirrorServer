#include "Protocol.h"
#include "functions.h"

void receiveMessage(int socket, char* message)
{
	if ( read(socket, message, BUFSIZE) < 0 )
    	perror_exit("receiveMessage-read");
}


void sendMessage(int socket, char* message)
{
    if ( write(socket, message, BUFSIZE) < 0 )
    	perror_exit("sendMessage-write");
}


int requestConnection(int socket)
{
	char message[BUFSIZE];

	sendMessage(socket, "CONNECTION");
	receiveMessage(socket, message);

	if ( !strcmp(message, "ACCEPT") )
	{
		return 0;
	}
	else
	{
		fprintf(stderr, "requestConnection: Error, didnt receive 'ACCEPT'\n");
		fprintf(stderr, "Message received: '%s'\n", message);

		return -1;
	}
}


void acceptConnection(int socket)
{
	sendMessage(socket, "ACCEPT");
}


void terminateConnection(int socket)
{
	sendMessage(socket, "TERMINATE");
}


int sendFile(int socket, char* path_to_file)
{
;
}


int receiveFile(int socket, char* path_to_file)
{
	//initiate connection
	int error = requestConnection(socket);

	if (error)
	{
		fprintf(stderr, "sendFile: Error in requestConnection!\n");
		exit(EXIT_FAILURE);
	}

	char message[BUFSIZE];
	int part_number = 0;


	//receive parts of file until "TERMINATE" message is sent!
	do
	{
		;
	}
	while ( strcmp(message, "TERMINATE") );
}