#include "functions.h"

void perror_exit(char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

int matchesDirOrFile(char* requested, char* given, char* saveDir, char** localPath)
{
	int givenSize = strlen(given);
	int requestedSize = strlen(requested);
	if (requested[requestedSize-1] == '/')//if user gave '/' at the end,'remove' it to avoid confusion
	{
		requested[requestedSize-1] = '\0';
		requestedSize--;
	}
	int i;
	*localPath = NULL;

	//if dirorfile requested is of bigger size than the one we got from LIST
	//there is no way we need it
	if (requestedSize > givenSize)
		return -1;

	int requested_index = 0;
	int given_chars_forward;
	int requested_last_part_size;
	int requested_start;//e.g if requested is /dir/dir2/fileA , last part is fileA .we need to know where that begins

	for (i=0; i<givenSize; i++)
	{
		if (given[i] == requested[requested_index])
		{//move on on the requested str (dirorfile)
			requested_index++;
		}
		else
		{//start over!
			requested_index = 0;
		}

		//if we got a match for the requested file or dir
		//and the next char in the given str is either nothing (i + 1 == givenSize) or '/' (its a dir and has more)
		//then given matches requested!
		if (requested_index == requestedSize && ( (i+1) == givenSize || given[i+1] == '/'))
		{
			given_chars_forward = givenSize - i - 1;//chars ahead after given[i]

			//find where the last_part of requested string begins (the index)
			requested_start = requestedSize;
			while (requested_start > 0)
			{
				if (requested[requested_start - 1] == '/')
					break;
				else
					requested_start--;
			}

			requested_last_part_size = requestedSize - requested_start;//size of requested last part

			*localPath = malloc( (strlen(saveDir) + requested_last_part_size + given_chars_forward + 4)*sizeof(char) );
			sprintf(*localPath, "./%s/%s%s", saveDir, requested + requested_start, given + (i+1)*sizeof(char));
			return 1;
		}
	}

	return -1;
}


int count_digits(int number)
{
	int counter = 0;

	do
	{
		counter++;
		number /= 10;
	}
	while (number != 0);

	return counter;
}