#ifndef FUNCTIONS_H
#define FUNCTIONS_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE 1024
#define perror2(s,e) fprintf(stderr, "%s: %s\n", s, strerror(e))

void perror_exit(char *message);
int matchesDirOrFile(char* requested, char* given, char* saveDir, char** localPath);
int count_digits(int number);

#endif