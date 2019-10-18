#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/types.h>

#define MAX_LINE 80 //max number of characters on a line

char* historyCache;	//variable to save the latest command

void execLine(const char* line)
{

	int tokensSize = 0;
	char* tokens[MAX_LINE / 2 + 1];
	int fd[2] = { -1, -1 };
	int setWait = 0;
	int status;

	//split user command into tokens
	char* inputLine = strdup(line);
	tokens[tokensSize++] = strtok(inputLine, " ");

	while (tokens[tokensSize - 1] != NULL)
	{
		tokens[tokensSize++] = strtok(NULL, " ");
	}

	tokensSize--;

	//check if user want to create a parallel process
	if (strcmp(tokens[tokensSize - 1], "&") == 0)
	{
		setWait = 1;
		tokensSize--;
		tokens[tokensSize] = NULL;
	}

	while (tokensSize > 2)
	{
		if (strcmp(tokens[tokensSize - 2], ">") == 0) //check if user want to redirect output
		{
			creat(tokens[tokensSize - 1], S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); //create a new file to write in

			fd[1] = open(tokens[tokensSize - 1], O_WRONLY | O_TRUNC);	//open file to write
			if (fd[1] == -1) //open failed
			{
				free(inputLine);
				return;
			}

			tokens[tokensSize - 2] = NULL;
			tokensSize -= 2;
		}
		else if (strcmp(tokens[tokensSize - 2], "<") == 0) //check if user want to redirect input
		{
			fd[0] = open(tokens[tokensSize - 1], O_RDONLY); //open data source user want to input
			if (fd[0] == -1) //open failed
			{
				free(inputLine);
				return;
			}
			
			tokens[tokensSize - 2] = NULL; //delete redirect characters and source of it
			tokensSize -= 2;
		}
		else
		{
			break;
		}
	}


	pid_t pid = fork(); //create a new process
	
	switch (pid) //check pid
	{
		case -1: //fork failed
			break;
		case 0:
			if (fd[0] != -1) 
			{
				if (dup2(fd[0], STDIN_FILENO) != STDIN_FILENO)  //check read - end is ready
				{
					exit(1);
				}
			}
			if (fd[1] != -1)
			{
				if (dup2(fd[1], STDOUT_FILENO) != STDOUT_FILENO) //check write - end is ready
				{
					exit(1);
				}
			}
			execvp(tokens[0], tokens); //syscall to process user's command
			exit(0);
		default:
			close(fd[0]);	//close read - end
			close(fd[1]);	//close write - end
			
			if (!setWait)
				waitpid(pid, &status, 0); //if there isn't any parallel process => wait for the pid_process to finish before a new command
			break;
	}
	
	free(inputLine); //free memory
}

void saveCmd(const char* cmd)
{
	free(historyCache); //free memory to reset data
	historyCache = strdup(cmd);	//new data to save
}

void history(const char* cmd)
{
	if (historyCache == NULL)
	{
		printf("No commands in history\n");
		return;
	}
	
	printf("osh>%s\n", historyCache); //printf lastest user's command
	execLine(historyCache);	//exec the command

}

void execPipe(const char* line)
{
	char* inputLine = strdup(line);
	char* tokens[MAX_LINE / 2 + 1];
	
	//parse the inputLine to tokens
	tokens[0] = strtok(inputLine, " "); //take the first word of command line
	int size = 1;
	while (tokens[size - 1] != NULL) //if the latest word is not null => continue to take
	{
		tokens[size++] = strtok(NULL, " ");
	}

	size--;

	char* firstCmd[MAX_LINE / 2 + 1]; //the first part 
	char* secondCmd[MAX_LINE / 2 + 1]; //the second part
	int sizeCmd1 = 0, sizeCmd2 = 0, posOfSplit = 0;
	int type = 1;

	//find char "|"
	int pos = 0;
	while (pos < size)
	{
		if (strcmp(tokens[pos],"|")==0)
		{
			type = 2;
			posOfSplit = pos;
		}
		else
			if (type == 1)
				firstCmd[sizeCmd1++] = tokens[pos]; //add tokens to new arr
			else
				secondCmd[sizeCmd2++] = tokens[pos]; //add tokens to new arr

		pos++;
	}

	firstCmd[sizeCmd1] = NULL; //add NULL to arr to use in syscall execvp()
	secondCmd[sizeCmd2] = NULL;	//add NULL to arr to use in syscall execvp()
	
	
	int fd[2];

	if (pipe(fd) != 0) //init pipe
	{
		printf("\nPipe could not be initialized\n");
		return;
	}

	pid_t p1 = fork();  //create a new process - process 1
	if (p1 < 0)
	{
		printf("\nFailed fork\n");
		return;
	}
	//check if process is parent or child

	if (p1 == 0) //check if process is child
	{
		close(fd[0]); //close read - end because it will process and output data to process 2
		dup2(fd[1], STDOUT_FILENO); //write data to fd[1]
		close(fd[1]);				//out all data - finish writing => close write - end
		
		//begin exec by syscall
		if (execvp(tokens[0], firstCmd) < 0) //syscall to process cmd in first part
		{
			printf("\nError when exec first part\n");
		}
		exit(0);//exit child 1 to return parent
	}
	else
	{
		//create a new process: child - process 2
		pid_t p2 = fork();

		if (p2 < 0)
		{
			printf("\nCould not fork\n");
			return;
		}

		if (p2 == 0) //check if process is child 
		{
			close(fd[1]); //close write - end because it receive data from process - child 1 to process
			dup2(fd[0], STDIN_FILENO);	//get data to fd[0]
			close(fd[0]);	//get all data => close read - end

			if (execvp(tokens[posOfSplit + 1], secondCmd) < 0) //syscall to process cmd in the second part
			{
				printf("Error when exec second part\n");
			}
			exit(0); //exit child 2 to return parent
		}
		else //parent running
		{
			close(fd[1]); //close write - end of parent to sign that it isn't process any thing and ready to get data - result from its children
			//waiting for 2 children
			wait(NULL);
			wait(NULL);
		}
	}

}

int checkPipeCmd(const char* line)
{
	int len = strlen(line);
	int i = 0;
	while (i < len)
	{
		if (line[i] == '|')
			return 1;
		i++;
	}
	return 0;
}

int main(int argc, char* argv[])
{

	size_t line_size = 100;
	char* line = (char*)malloc(sizeof(char) * line_size);
	int continueRun = 1;

	//run still user type "exit" or close terminal
	while (continueRun)
	{
		printf("osh>");
		fflush(stdout);

		getline(&line, &line_size, stdin); //get command line string

		int line_len = strlen(line);
		if (line_len != 1)
		{
			line[line_len - 1] = '\0'; //add end signal to string

			if (strcmp(line, "exit") == 0) //user input exit => exit
			{
				continueRun = 0;
			}
			else
				if (strcmp(line, "!!") == 0) //user want to know the latest command line
				{
					history(line);
				}
				else
					if (checkPipeCmd(line)) //user want to use pipe to command
					{
						execPipe(line);
					}
					else //normal command
					{
						saveCmd(line);
						execLine(line);
					}
		}
	}

	free(line);
	return 0;
}
