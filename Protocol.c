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

void receiveFile(int socket, char* filePath)
{
	char* buffer;
	char message[BUFSIZE];
	int MaxBuffer;
	int fileSize, fileParts, lastPartBytes, i;

	//receive MaxBufSize used by server
	receiveMessage(socket, message);
	MaxBuffer = atoi(message);
	buffer = malloc(MaxBuffer*sizeof(char));

	//receive size of file to receive in bytes
	receiveMessage(socket, message);
	fileSize = atoi(message);

	//calculate how many FULL BUFFER messages it will take to receive the whole file
	//if the file requires an extra (not full) message, we ll do that afterwards
	int filefd = open(filePath, O_WRONLY | O_CREAT, 0777);
	//for empty files,we are done here
	if (fileSize == 0)
	{
		close(filefd);
		free(buffer);
		return;
	}

	fileParts = fileSize / MaxBuffer;
	lastPartBytes = fileSize % MaxBuffer;

	if (filefd == -1)
		perror_exit("receiveFile-open");

	for (i=0; i<fileParts; i++)
	{
		//read MaxBuffer bytes
	    if ( read(socket, buffer, MaxBuffer) < 0 )
			perror_exit("receiveFile-read");

    	//write them to the file
    	if ( write(filefd, buffer, MaxBuffer) < 0 )
    		perror_exit("receiveFile-write");

    	//send OK message to move on
    	sendMessage(socket, "OK");
	}

	//if there are remaining bytes to receive
	if (lastPartBytes != 0)
	{
		//read lastPartBytes bytes
	    if ( read(socket, buffer, lastPartBytes) < 0 )
			perror_exit("receiveFile-read");

    	//write them to the file
    	if ( write(filefd, buffer, lastPartBytes) < 0 )
    		perror_exit("receiveFile-write");

    	//send OK message to move on
    	sendMessage(socket, "OK");
	}

	close(filefd);
	free(buffer);
}


void sendFile(int socket, char* filePath, int bufferSize)
{
	char* buffer;
	char message[BUFSIZE];
	int fileSize, fileParts, lastPartBytes, i;
	buffer = malloc(bufferSize*sizeof(char));
	struct stat file_stat;

	//send bufferSize used for the communication
	sprintf(message, "%d", bufferSize);
	sendMessage(socket, message);

	//find size of file in bytes and send it to client
	int filefd = open(filePath, O_RDONLY);
	if (filefd == -1)
		perror_exit("sendFile-open");

	//Get file stats
    if (fstat(filefd, &file_stat) < 0)
    	perror_exit("sendFile-fstat");

    fileSize = file_stat.st_size;

    sprintf(message, "%d", fileSize);
	sendMessage(socket, message);

	//for empty files, we are done here
	if (fileSize == 0)
	{
		close(filefd);
		free(buffer);
		return;
	}

	//calculate how many FULL BUFFER messages it will take to send the whole file
	//if the file requires an extra (not full) message, we ll do that afterwards
	fileParts = fileSize / bufferSize;
	lastPartBytes = fileSize % bufferSize;

	for (i=0; i<fileParts; i++)
	{
		//read bufferSize bytes from file
		if ( read(filefd, buffer, bufferSize) < 0 )
    		perror_exit("sendFile-read");

    	//send them to client
    	if ( write(socket, buffer, bufferSize) < 0 )
			perror_exit("sendFile-write");

    	//receive OK message to move on
    	receiveMessage(socket, message);
    	if (strcmp(message, "OK"))
    	{
    		fprintf(stderr, "sendFile Error! Did not receive OK message for part %d!\n", i + 1);
    		fprintf(stderr, "Received: '%s'\n", message);
    	}
    	
	}

	//if there are remaining bytes to send
	if (lastPartBytes != 0)
	{
		//read lastPartBytes bytes from file
		if ( read(filefd, buffer, lastPartBytes) < 0 )
    		perror_exit("sendFile-read");

    	//send them to client
    	if ( write(socket, buffer, lastPartBytes) < 0 )
			perror_exit("sendFile-write");

    	//receive OK message to move on
    	receiveMessage(socket, message);
    	if (strcmp(message, "OK"))
    	{
    		fprintf(stderr, "sendFile Error! Did not receive OK message for part %d!\n", i + 1);
    		fprintf(stderr, "Received: '%s'\n", message);
    	}

	}

	close(filefd);
	free(buffer);
}