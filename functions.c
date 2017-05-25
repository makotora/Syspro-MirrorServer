#include "functions.h"

void perror_exit(char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}