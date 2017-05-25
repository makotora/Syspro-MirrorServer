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

int listRequest(int socket);
void acceptConnection(int socket);

int sendFile(int socket, char* path_to_file);
int receiveFile(int socket, char* path_to_file);


#endif