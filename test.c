
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/types.h>

#define MAX_LINE 80

char* historyCache;

void execLine(const char* line)
{

	int argc = 0;
	char* args[MAX_LINE / 2 + 1];
	int fd[2] = { -1, -1 };
	int setWait = 0;
	int status;

	char* inputLine = strdup(line);
	args[argc++] = strtok(inputLine, " ");

	while (args[argc - 1] != NULL)
	{

		args[argc] = strtok(NULL, " ");
		argc++;
	}

	argc--;


	if (strcmp(args[argc - 1], "&") == 0)
	{
		setWait = 1;
		argc--;
		args[argc] = NULL;
	}



	while (argc > 2)
	{
		if (strcmp(args[argc - 2], ">") == 0)
		{
			creat(args[argc - 1], S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

			fd[1] = open(args[argc - 1], O_WRONLY | O_TRUNC);
			if (fd[1] == -1)
			{
				free(inputLine);
				return;
			}

			args[argc - 2] = NULL;
			argc -= 2;
		}
		else if (strcmp(args[argc - 2], "<") == 0)
		{
			fd[0] = open(args[argc - 1], O_RDONLY);
			if (fd[0] == -1)
			{
				free(inputLine);
				return;
			}
			args[argc - 2] = NULL;
			argc -= 2;
		}
		else
		{
			break;
		}
	}


	pid_t pid = fork();
	switch (pid)
	{
	case -1:
		break;
	case 0:
		if (fd[0] != -1)
		{
			if (dup2(fd[0], STDIN_FILENO) != STDIN_FILENO)
			{
				exit(1);
			}
		}
		if (fd[1] != -1)
		{
			if (dup2(fd[1], STDOUT_FILENO) != STDOUT_FILENO)
			{
				exit(1);
			}
		}
		execvp(args[0], args);
		exit(0);
	default:
		close(fd[0]);
		close(fd[1]);
		if (!setWait)
			waitpid(pid, &status, 0);
		break;
	}
	free(inputLine);
}

void saveCmd(const char* cmd)
{
	free(historyCache);
	historyCache = strdup(cmd);
}

void history(const char* cmd)
{
	if (historyCache == NULL)
	{
		printf("No commands in history\n");
		return;
	}
	printf("osh>%s\n", historyCache);
	execLine(historyCache);

}

void execPipe(const char* line)
{
	char* inputLine = strdup(line);
	char* token[MAX_LINE / 2 + 1];
	token[0] = strtok(inputLine, " "); //take the first word of command line
	int size=1;
	while (token[size-1]!=NULL) //if the latest word is not null => continue to take
	{
		token[size++]=strtok(inputLine, " ");
	}
	
	token[size]=NULL;
	
	char *firstCmd; //the first part 
	char *secondCmd; //the second part
	int sizeCmd1 = 0, sizeCmd2 =0 , posOfSplit=0;
	int type =1;
	
	//find char "|"
	int pos=0;
	while (pos < size)
	{
		if (token[pos]=="|")
		{
			type = 2;
			posOfSplit=pos;
		}
		else
			if (type==1)
				firstCmd[sizeCmd1++]=token[pos]; //add token to new arr
			else
				secondCmd[sizeCmd2++]=token[pos]; //add token to new arr
		
		pos++;
	}
	
	firstCmd[sizeCmd1]=NULL;
	secondCmd[sizeCmd2]=NULL;
	
	int fd[2];
	
	if (pipe(fd) != 0) //init pipe
	{
		printf("\nPipe could not be initialized\n");
		return;
	}
	
	pid_t p1 =folk();  //create a new process
	if (p1 < 0)
	{
		printf("\nCould not fork"); 
        return;
	}
	//check if process is parent or child
	
	if (p1==0) //check if process is child
	{
		close(fd[0]); 
		dup2(fd[1], STDOUT_FILENO);
		close(fd[1]);
		
		if (execvp(token[0],firstCmd)<0) //syscall to process cmd in first part
		{
			printf("\nerr when exec first part\n");
		}
		exit(0);//exit child 1 to return parent
	}
	else
	{
		pid_t p2 = folk();
		
		if (p2 < 0)
		{
			printf("\nCould not fork\n"); 
            return;
		}
		
		if (p2 == 0) //check if process is child 
		{
			close(fd[1]);
			dup2(fd[0],STDIN_FILENO);
			close(fd[0]);
			
			if (execvp(token[posOfSplit+1],secondCmd) < 0) //syscall to process cmd in the second part
			{
				printf("err when exec second part\n");
			}
			exit(0); //exit child 2 to return parent
		}
		else //parent running
		{
			wait(NULL);
			wait(NULL);
		}
	}
	
}

int checkPipeCmd(const char* line)
{
	int len=strlen(line);
	int i=0;
	while (i<len)
	{
		if (line[i]=='|')
			return 1;
	}
	return 0;
}

int main(int argc, char* argv[])
{

	size_t line_size = 100;
	char* line = (char*)malloc(sizeof(char) * line_size);
	int continueRun = 1;
	
	while (continueRun)
	{
		printf("osh>");
		fflush(stdout);

		getline(&line, &line_size, stdin);

		int line_len = strlen(line);
		if (line_len != 1)
		{
			line[line_len - 1] = '\0';
			
			if (strcmp(line, "exit") == 0) //user input exit
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