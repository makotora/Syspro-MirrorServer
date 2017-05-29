#ifndef PROTOCOL_H
#define PROTOCOL_H

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

void receiveMessage(int socket, char* message);
void sendMessage(int socket, char* message);

#endif