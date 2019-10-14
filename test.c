
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/types.h>

#define MAX_LINE 80

static char* historyCache;

static void execLine(const char* line)
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

static void saveCmd(const char* cmd)
{
	free(historyCache);
	historyCache = strdup(cmd);
}

static void history(const char* cmd)
{
	if (historyCache == NULL)
	{
		printf("No commands in history\n");
		return;
	}
	printf("osh>%s\n", historyCache);
	execLine(historyCache);

}

int main(int argc, char* argv[])
{

	size_t line_size = 100;
	char* line = (char*)malloc(sizeof(char) * line_size);
	int shouldrun = 1;
	while (shouldrun)
	{
		printf("osh>");
		fflush(stdout);

		getline(&line, &line_size, stdin);

		int line_len = strlen(line);
		if (line_len == 1)
		{
			continue;
		}
		line[line_len - 1] = '\0';

		if (strcmp(line, "exit") == 0)
		{
			shouldrun = 0;
			continue;
		}
		else if (strcmp(line, "!!") == 0)
		{
			history(line);
		}
		else
		{
			saveCmd(line);
			execLine(line);
		}
	}

	free(line);
	return 0;
}