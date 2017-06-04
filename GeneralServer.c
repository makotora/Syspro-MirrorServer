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

#include "Buffer.h"
#include "Protocol.h"
#include "functions.h"
#include "structs.h"

#define LISTEN_QUEUE_SIZE 20
#define BUFSIZE 1024
#define BUFFER_SIZE 30

#define MAX_RECONNECTS 100

//prints with many details
#define PRINTS 1

//basic prints
#define PRINTS2 1

//tell content servers included in requests to terminate (after we are done with them)
#define KILL_CONTENT_SERVERS 1

//general server keeps_running forever (and accept many initiator requests)
//of course he kills himself if he receives a KYS message
//if it is 0, it terminates after handling one initiator
#define KEEP_RUNNING 0

int sock;
char* fileFullPath;
char* dirname;
char* dirFullPath;
int threadnum;

//global structs
delays* delays_list;
Buffer* buffer;
id_counters_list* idc_list;//list countaining a counter for files to fetch for each ID

int terminateMirror;
int terminate;


pthread_mutex_t buff_mtx;
pthread_mutex_t idc_mtx;
pthread_mutex_t mtx;
pthread_cond_t allDone;
pthread_cond_t buff_not_empty;
pthread_cond_t buff_not_full;

int total_devices;
int numDevicesDone;
int filesTransferred;
int bytesTransferred;
int* fileSizes_array;
int fileSizes_index;
int fileSizes_max;


void* mirror_thread(void* infos_par)
{
    mirror_thread_params* infos = (mirror_thread_params *) infos_par;
    char buf[BUFSIZE];
    //socket vars
    int sock;
    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr*)&server;
    struct hostent *rem;

    //Create socket and connect to the content server
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        perror_exit("socket");

    rem = gethostbyname(infos->host);
    if (rem == NULL) 
    {   
       herror("mirror-gethostbyname");
       exit(1);
    }
    
    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(infos->port);
    
    //Once in a while, connect fails for OS reasons..
    //When connect fails i try to connect again MAX_RECONNECTS times,with a fresh socket each time
    int i;
    for (i=0; i<MAX_RECONNECTS; i++)
    {
        if (connect(sock, serverptr, sizeof(server)) >= 0)
           break;
        else if (i+1 == MAX_RECONNECTS)//this was our last try,throw error and exit
           perror_exit("connect");
        else
        {//Retry with a fresh socket
            close(sock);
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0)
                perror_exit("socket");

            rem = gethostbyname(infos->host);
            if (rem == NULL) 
            {   
               herror("mirror-gethostbyname");
               exit(1);
            }
            
            server.sin_family = AF_INET;
            memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
            server.sin_port = htons(infos->port);
        }
    }

    if (PRINTS)
        printf("\tMirrorThread: Connected to %s port %d\n", infos->host, infos->port);

    //Send LIST request
    sprintf(buf, "LIST %s %d", infos->id, infos->delay);

    if (PRINTS)
        fprintf(stderr, "\tMirrorThread: Sending '%s' to GeneralServer!\n", buf);
    
    sendMessage(sock, buf);
            
    //get FILE pointer for socket
    FILE* socket_fp;
    socket_fp = fdopen(sock ,"r+");
    if (socket_fp == NULL)
        perror_exit("fdopen");

    char c;
    char* localPath;
    int buf_index, matches_found, files_for_fetch;
    id_counter* idc;
    matches_found = 0;

    //First of all,the content server will send us the name of the directories he contains
    //Create the ones we need (mkdir)
    buf_index = 0;
    while ( (c = getc(socket_fp)) != EOF )
    {
        //if we received one result (they end with '\n')
        if (c == '\n')
        {
            buf[buf_index] = '\0';
            if ( matchesDirOrFile(infos->dirorfile, buf, infos->dirFullPath, &localPath) == 1 )
            {
                matches_found++;        
                if (PRINTS2)
                    fprintf(stderr, "Created dir : %s\n", localPath);
                
                if ( mkdir(localPath, 0777) == -1 )
                {
                    if (PRINTS)
                        perror("mkdir");
                }

                free(localPath);
            }
            buf_index = 0;//reset for next result if there is one
        }
        else //keep saving
        {
            buf[buf_index++] = c;
        }
    }

    //Afterwards, Content Server will send us the names of the files he contains
    //Add the ones we need in the buffer
    //Add record for this id in the idc_list.For each file pushed in the buffer, increment the counter for that id
    pthread_mutex_lock(&idc_mtx);
        if ( idc_list_add(idc_list, infos->id) != 0)
            exit(EXIT_FAILURE);
    pthread_mutex_unlock(&idc_mtx);


    buf_index = 0;
    files_for_fetch = 0;
    while ( (c = getc(socket_fp)) != EOF )
    {
        //if we received one result (they end with '\n')
        if (c == '\n')
        {
            buf[buf_index] = '\0';
            if ( matchesDirOrFile(infos->dirorfile, buf, infos->dirFullPath, &localPath) == 1 )
            {
                matches_found++;
                files_for_fetch++;

                //increase counter for this id
                pthread_mutex_lock(&idc_mtx);
                    idc_list_increase(idc_list, infos->id);
                pthread_mutex_unlock(&idc_mtx);
                
                //add it to buffer/struct with: its ID,hostname,port,localPath,remotePath(buf)
                pthread_mutex_lock(&buff_mtx);

                    while(buffer->count >= buffer->size)
                    {
                        if (PRINTS)
                            fprintf(stderr, "\tMirrorThread: Found buffer full!\n");
                        
                        pthread_cond_wait(&buff_not_full, &buff_mtx);
                    }
                    Buffer_push(buffer, infos->host, infos->port, infos->id, localPath, buf);

                    if (PRINTS)
                        fprintf(stderr, "\tMirrorThread: Added '%s' to buffer!\n", localPath);

                    pthread_cond_signal(&buff_not_empty);
                
                pthread_mutex_unlock(&buff_mtx);

                free(localPath);
            }
            buf_index = 0;//reset for next result if there is one
        }
        else //keep saving
        {
            buf[buf_index++] = c;
        }
    }

    if (matches_found == 0)
    {
        fprintf(stderr, "<!> Content server '%s' does not contain dir/file '%s'!Nothing to fetch!\n", infos->host, infos->dirorfile);
        pthread_mutex_lock(&mtx);
            numDevicesDone++;
            if (numDevicesDone == total_devices)
            {
                pthread_cond_signal(&allDone);
            }
        pthread_mutex_unlock(&mtx);
    }
    //some matches were found, check if there are files to fetch
    else if (files_for_fetch == 0)//no files to fetch (only dirs that were already created)
    {
        pthread_mutex_lock(&mtx);
            numDevicesDone++;//done with this device (no file fetching!)
            if (numDevicesDone == total_devices)
            {
                pthread_cond_signal(&allDone);
            }
        pthread_mutex_unlock(&mtx);
    }
    else
    {
        //this mutex is unlocked either in the if or the else below
        pthread_mutex_lock(&idc_mtx);
            idc = idc_list_get(idc_list, infos->id);

            if (idc == NULL)
                exit(EXIT_FAILURE);

            //the workers were so fast that they already fetched all the files!
            //note that this device is done!
            if (idc->counter == 0)
            {
                pthread_mutex_unlock(&idc_mtx);//we are done with the idc.unlock to avoid deadlocks!
                //protect global var numDevicesDone
                pthread_mutex_lock(&mtx);
                    numDevicesDone++;
                    if (numDevicesDone == total_devices)
                    {
                        pthread_cond_signal(&allDone);
                    }
                pthread_mutex_unlock(&mtx);

            }
            else//workers havent finished yet, let them know that this id's counter wont increase
            {//so when they make it 0 they will note that the device is done
                idc->wont_increase = 1;
                pthread_mutex_unlock(&idc_mtx);
            }
    }

    /* Close socket */
    fclose(socket_fp);
    socket_fp = NULL;
    close(sock);
    sock = -1;

    //free stuff
    free(infos->host);
    free(infos->dirFullPath);
    free(infos->dirorfile);
    free(infos->id);
    free(infos);

    pthread_exit(NULL);
}


void* worker_thread(void* null)
{   
    //to be detached
    pthread_detach(pthread_self());

    while (1)
    {
        Buffer_data buffer_element;
        char buf[BUFSIZE];
        //socket vars
        struct sockaddr_in server;
        struct sockaddr *serverptr = (struct sockaddr*)&server;
        struct hostent *rem;
        int sock;
        int file_size;

        pthread_mutex_lock(&buff_mtx);
            while (terminateMirror == 0 && buffer->count <= 0)//FIRST check if we are terminating!
            {//ITS IMPORTANT TO CHECK THAT FIRST BECAUSE THERE MIGHT BE NOT BUFFER (free)
                if (PRINTS)
                    printf("\t\tWorkerThread: Found buffer empty!\n");
                
                pthread_cond_wait(&buff_not_empty, &buff_mtx);
            }


            if (terminateMirror == 1)
            {//terminating!(received broadcast signal for unblock from buffer and found 'terminate' var 1!)
                pthread_mutex_unlock(&buff_mtx);
                break;
            }

            buffer_element = Buffer_pop(buffer);

            if (PRINTS)
            {
                fprintf(stderr, "\t\tWorkerThread: Popped buffer element:\n");
                Buffer_print_element(buffer_element);
            }
            
            pthread_cond_signal(&buff_not_full);
        
        pthread_mutex_unlock(&buff_mtx);

    
        //Create socket and connect to content server (according to the info popped from buffer)
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if ( sock < 0 )
            perror_exit("socket");

        rem = gethostbyname(buffer_element.host);
        if (rem == NULL) 
        {   
           herror("worker-gethostbyname");
           exit(1);
        }
        
        server.sin_family = AF_INET;
        memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
        server.sin_port = htons(buffer_element.port);
        
        //Once in a while, connect fails for OS reasons..
        //When connect fails i try to connect again MAX_RECONNECTS times,with a fresh socket each time
        int i;
        for (i=0; i<MAX_RECONNECTS; i++)
        {
            if (connect(sock, serverptr, sizeof(server)) >= 0)
               break;
            else if (i+1 == MAX_RECONNECTS)//this was our last try,throw error and exit
               perror_exit("connect");
            else
            {//Retry with a fresh socket
                close(sock);
                sock = socket(AF_INET, SOCK_STREAM, 0);
                if (sock < 0)
                    perror_exit("socket");

                rem = gethostbyname(buffer_element.host);
                if (rem == NULL) 
                {   
                   herror("mirror-gethostbyname");
                   exit(1);
                }
                
                server.sin_family = AF_INET;
                memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
                server.sin_port = htons(buffer_element.port);
            }
        }
        
        //Send FETCH request to content
        sprintf(buf, "FETCH %s %s", buffer_element.id, buffer_element.remote_path);

        if (PRINTS)
            fprintf(stderr, "\t\tWorkerThread: Sending '%s' to ContentServer!\n", buf);

        sendMessage(sock, buf);

        //Receive the file
        file_size = receiveFile(sock, buffer_element.local_path);

        if (PRINTS2)
            fprintf(stderr, "Fetched file: %s\n", buffer_element.local_path);


        pthread_mutex_lock(&mtx);
            filesTransferred++;
            bytesTransferred += file_size;

            if (fileSizes_index == fileSizes_max)
            {
                fileSizes_max = fileSizes_max * 2;
                fileSizes_array = realloc(fileSizes_array, fileSizes_max*sizeof(int));
            }

            fileSizes_array[fileSizes_index++] = file_size;
        pthread_mutex_unlock(&mtx);
        
        //check if this fetch was the last for this device (the device is matched to a specific ID)
        int id_counter;
        int wont_increase;

        pthread_mutex_lock(&idc_mtx);
           id_counter = idc_list_decrease(idc_list, buffer_element.id, &wont_increase);
        pthread_mutex_unlock(&idc_mtx);

        if (id_counter == 0 && wont_increase == 1)//if we made the counter 0,and the counter for this id is final
        {
            pthread_mutex_lock(&mtx);
                numDevicesDone++;
                if (numDevicesDone == total_devices)
                {
                    pthread_cond_signal(&allDone);
                }
            pthread_mutex_unlock(&mtx);
        }

        //free buffer element popped
        free(buffer_element.host);
        free(buffer_element.id);
        free(buffer_element.local_path);
        free(buffer_element.remote_path);
        
        close(sock);
        sock = -1;
    }

    pthread_exit(NULL);
}


void* thread_content_server(void* sock_par) 
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

    int* socketPtr = (int*) sock_par;
    int newsock = *socketPtr;

    //Receive request message
    receiveMessage(newsock, buf);

    if (PRINTS2)
        fprintf(stderr, "\tGeneralServerThread received: '%s'\n", buf);
    
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
           fprintf(stderr, "\tGeneralServerThread: Dealing with a FETCH request!\n");

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
            fprintf(stderr, "\tGeneralServerThread: Delay for this fetch is : %d\n", delay);

        //last token is the path to the file (full path) for fetching
        token = strtok_r(NULL, "", &savePtr);
        path = strdup(token);


        if (PRINTS2)
            fprintf(stderr, "\tGeneralServerThread: Sending file %s\n", path);        
        
        //first sleep for delay seconds,than send the file
        sleep(delay);
        sendFile(newsock, path, BUFSIZE);

        free(id);
        free(path);
    }
    else if ( !strcmp(token, "SOR") )
    {
        pthread_mutex_lock(&buff_mtx);
            if (buffer != NULL)
            {
                pthread_mutex_unlock(&buff_mtx);
                sendMessage(newsock, "GeneralServer: Handling another initiator right now.Please try later.\n");
                
                close(newsock);
                newsock = -1;
                free(socketPtr);

                pthread_exit(NULL);            
            }
        pthread_mutex_unlock(&buff_mtx);
        //WE RECEIVED A 'StartOfRequests' MESSAGE.Do what a mirrorServer would do
        //Fork ourself to deal with that Initiator, and standby to (maybe) do the same for another initiator
        char buf[BUFSIZE];
        int err, status, i;
        pthread_t thr;
        pthread_t* mthr_array;

        //Structs and vars initialisation
                //for buffer and terminations
        terminateMirror = 0;
        buffer = Buffer_create(BUFFER_SIZE); 
        idc_list = idc_list_create();
        numDevicesDone = 0;
        total_devices = 0;

                //for stats
        filesTransferred = 0;
        bytesTransferred = 0;
        fileSizes_index = 0;
        fileSizes_max = 20;
        fileSizes_array = malloc(fileSizes_max*sizeof(int));//to keep fileSizes for spread
            
                //for mirror threads join and (possibly) content server 'killing'
        int request_index = 0;
        int requests_max = 5;
        char* token;
        char* savePtr;
        int id = 1;//ID counter for requests
        mirror_thread_params* infos;
        mthr_array = malloc(requests_max*sizeof(pthread_t));
        server_list* s_list = server_list_create();

        //Create worker threads!
        //(no need to keep their ids,they will detach)
        for (i=0; i<threadnum; i++)
        {
            if ( (err = pthread_create(&thr, NULL, worker_thread, (void *) NULL)) )  
            {
                perror2("pthread_create", err);
                exit(1);
            }
        }


        //receive requests and create one mirror thread for each request
        //also save distinct servers' infos in a list so we can 'kill' them after alldone

        if (PRINTS2)
            fprintf(stderr, "Receiving requests from MirrorInitiator!\n");

        receiveMessage(newsock, buf);
        while ( strcmp(buf, "EOR") )//until we receive EndOfRequests message
        {
            if (PRINTS2)
                fprintf(stderr, "Received request: '%s' from MirrorInitiator!\n", buf);

            total_devices++;

            infos = malloc(sizeof(mirror_thread_params));
            infos->dirFullPath = strdup(dirFullPath);

            token = strtok_r(buf, ":", &savePtr);
            infos->host = strdup(token);
            token = strtok_r(NULL, ":", &savePtr);
            infos->port = atoi(token);
            token = strtok_r(NULL, ":", &savePtr);
            infos->dirorfile = strdup(token);
            token = strtok_r(NULL, ":", &savePtr);
            infos->delay = atoi(token);

            infos->id = malloc( (strlen(infos->host) + count_digits(id) + 2)*sizeof(char) );
            sprintf(infos->id, "%s.%d", infos->host, id);
            id++;

            server_list_add(s_list, infos->host, infos->port);

            if (request_index == requests_max)
            {
                requests_max *= 2;
                mthr_array = realloc(mthr_array, requests_max*sizeof(pthread_t));
            }

            if ( (err = pthread_create(&(mthr_array[request_index++]), NULL, mirror_thread, (void *) infos)) )  
            {
                perror2("pthread_create", err);
                exit(1);
            }

            receiveMessage(newsock, buf);
        }

        for (i = 0; i < total_devices; i++)
        {
            if ( (err = pthread_join(mthr_array[i], (void **) &status)) ) 
            {
                perror2("pthread_join", err); /* termination */
                exit(1);
            }
        }
        
        pthread_mutex_lock(&mtx);
            while (numDevicesDone != total_devices)
            {
                if (PRINTS)
                  fprintf(stderr, "Waiting for allDone!\n");
                pthread_cond_wait(&allDone, &mtx);
            }
        pthread_mutex_unlock(&mtx);
        

        if (PRINTS)
            fprintf(stderr, "Terminating workers!\n");

        pthread_mutex_lock(&buff_mtx);
            terminateMirror = 1;//set terminate var to 1,then broadcast for blocked workers (on pop) to wake up!
            pthread_cond_broadcast(&buff_not_empty);
        pthread_mutex_unlock(&buff_mtx);

        if (PRINTS)
            fprintf(stderr, "Done!\n");

        //calculate stats and send them to Initiator
        float mean = (float) bytesTransferred / (float) filesTransferred;
        float spr = 0;
        for (i=0; i<filesTransferred; i++)
        {
            spr += ((float)fileSizes_array[i] - mean)*((float)fileSizes_array[i] - mean);
        }
        if (filesTransferred > 1)
            spr = spr / (filesTransferred - 1);

        if (PRINTS)
        {
            printf("Files Transferred: %d\nBytes Transferred: %d\nMean: %.2f\nSpread: %.2f\n", filesTransferred, bytesTransferred, mean, spr);
            fprintf(stderr, "Sending stats to mirror initiator!\n");
        }

        sendMessage(newsock, "STATS");//message so he knows he will receive stats
        sprintf(buf, "Files Transferred: %d\nBytes Transferred: %d\nMean: %.2f\nSpread: %.2f\n", filesTransferred, bytesTransferred, mean, spr);
        sendMessage(newsock, buf);

        close(newsock);

        //Connect to each distinct Content server and send him a KillYourselfMessage
        if (KILL_CONTENT_SERVERS == 1)
        {

            if (PRINTS)
                fprintf(stderr, "Terminating content servers!\n");

            server_info* current = s_list->first;
            while (current != NULL)
            {
                int cs_socket;
                struct sockaddr_in server;
                struct sockaddr *serverptr = (struct sockaddr*)&server;
                struct hostent *rem;

                //Create socket and connect to the content server
                cs_socket = socket(AF_INET, SOCK_STREAM, 0);
                if (cs_socket < 0)
                    perror_exit("socket");

                rem = gethostbyname(current->address);
                if (rem == NULL) 
                {   
                   herror("mirror-gethostbyname");
                   exit(1);
                }
                
                server.sin_family = AF_INET;
                memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
                server.sin_port = htons(current->port);
                
                //Once in a while, connect fails for OS reasons..
                //When connect fails i try to connect again MAX_RECONNECTS times,with a fresh socket each time
                int i;
                for (i=0; i<MAX_RECONNECTS; i++)
                {
                    if (connect(cs_socket, serverptr, sizeof(server)) >= 0)
                       break;
                    else if (i+1 == MAX_RECONNECTS)//this was our last try,throw error and exit
                       perror_exit("connect");
                    else
                    {//Retry with a fresh socket
                        close(cs_socket);
                        cs_socket = socket(AF_INET, SOCK_STREAM, 0);
                        if (cs_socket < 0)
                            perror_exit("socket");

                        rem = gethostbyname(current->address);
                        if (rem == NULL) 
                        {   
                           herror("mirror-gethostbyname");
                           exit(1);
                        }
                        
                        server.sin_family = AF_INET;
                        memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
                        server.sin_port = htons(current->port);
                    }
                }

                //send KillYourSelf message
                sendMessage(cs_socket, "KYS");
                close(cs_socket);

                current = current->next;
            }
        }

        idc_list_free(&idc_list);
        server_list_free(&s_list);
        Buffer_free(&buffer);

        free(mthr_array);
        free(fileSizes_array);
        buffer = NULL;

        if (KEEP_RUNNING == 0)//if we are not supposed to run forever..
        {//shutdown
            pthread_mutex_lock(&mtx);
                terminate = 1;
                shutdown(sock, SHUT_RDWR);//Brutally cancel possible stuck 'accept' calls so the general server can terminate
            pthread_mutex_unlock(&mtx);
        }
    }
    else if ( !strcmp(token, "KYS") )//If he told me to kill my self
    {
        if (PRINTS2)
        {
            fprintf(stderr, "GeneralServerThread: Received a KYS request!\n");
            fprintf(stderr, "GeneralServerThread: About to terminate me and GeneralServer!\n");
        }
        
        pthread_mutex_lock(&mtx);
            terminate = 1;
            shutdown(sock, SHUT_RDWR);//Brutally cancel possible stuck 'accept' calls so the content server can terminate
        pthread_mutex_unlock(&mtx);
    }
    else
    {
        fprintf(stderr, "<!>GeneralServerThread: Received an unknown request! : '%s'", token);
    }

    close(newsock);
    newsock = -1;
    free(socketPtr);

    pthread_exit(NULL);
}


int main(int argc, char *argv[]) 
{
    struct sockaddr_in server, client;
    socklen_t clientlen = sizeof(client);
    struct sockaddr *serverptr=(struct sockaddr *)&server;
    struct sockaddr *clientptr=(struct sockaddr *)&client;
    struct hostent *rem;

    //initialise mtxs cond_vars and global structs
    pthread_mutex_init(&buff_mtx, 0);
    pthread_mutex_init(&idc_mtx, 0);
    pthread_mutex_init(&mtx, 0);
    pthread_cond_init(&allDone, 0);
    pthread_cond_init(&buff_not_empty, 0);
    pthread_cond_init(&buff_not_full, 0);

    delays_list = delays_create();
    buffer = NULL;

    //Read arguments....
    int port = -1;
    char* dirorfilename = NULL;
    dirname = NULL;
    threadnum = -1;
    
    int index = 1;

    //Read arguments
    while (index + 1 < argc)
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
        else if ( !strcmp(argv[index], "-m") )
        {
            dirname = argv[index + 1];
            index++;
        }
        else if ( !strcmp(argv[index], "-w") )
        {
            threadnum = atoi(argv[index + 1]);
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
        fprintf(stderr, "Usage: './GeneralServer -p <port> -d <dirorfilename> -m <dirname> -w <threadnum>'\n");
        return -1;
    }
    else if (dirorfilename == NULL)
    {
        fprintf(stderr, "Error! Argument dirorfilename was not given!\n");
        fprintf(stderr, "Usage: './GeneralServer -p <port> -d <dirorfilename> -m <dirname> -w <threadnum>'\n");
        return -1;
    }
    else if (dirname == NULL)
    {
        fprintf(stderr, "Error! Argument dirname was not given!\n");
        fprintf(stderr, "Usage: './GeneralServer -p <port> -d <dirorfilename> -m <dirname> -w <threadnum>'\n");
        return -1;   
    }
    else if (threadnum == -1)
    {
        fprintf(stderr, "Error! Argument threadnum was not given!\n");
        fprintf(stderr, "Usage: './GeneralServer -p <port> -d <dirorfilename> -m <dirname> -w <threadnum>'\n");
        return -1;
    }

    //Get current directory full path
    long size;
    char *cwd;

    size = pathconf(".", _PC_PATH_MAX);

    if ( (cwd = (char *)malloc((size_t)size)) != NULL)
        cwd = getcwd(cwd, (size_t)size);

    //Create directory to save fetched files/dir (if it doesnt exist)
    mkdir(dirname, 0777);

    //save full path for dirname (we might chdir in a bit so we need to keep that to save fetches)
    dirFullPath = malloc( (strlen(cwd) + strlen(dirname) + 2)*sizeof(char) );
    sprintf(dirFullPath, "%s/%s", cwd, dirname);
    
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

    //Create socket for requests
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if ( sock < 0 )
        perror_exit("socket");

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    
    if (bind(sock, serverptr, sizeof(server)) < 0)
        perror_exit("bind");
    
    if (listen(sock, LISTEN_QUEUE_SIZE) < 0)
        perror_exit("listen");

    if (PRINTS)    
        printf("GeneralServer: Listening for connections to port %d\n", port);
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
        fprintf(stderr, "GeneralServer: Terminating!\n");
    }

    if( fileFullPath != NULL)
        free(fileFullPath);

    free(cwd);
    free(dirFullPath);
    delays_free(&delays_list);
    close(sock);
    pthread_exit(NULL);
}