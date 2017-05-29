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