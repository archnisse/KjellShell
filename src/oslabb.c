/*
 ============================================================================
 Name        : oslabb.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <wait.h> /* included to make the WUNTRACES stuff work */

#define TRUE 1
#define BUFFERSIZE 80 /* */
void prompt();
void read_command(char [(BUFFERSIZE/2) + 1][BUFFERSIZE], char*);
void forker(char*);

int main(void) {
    char buffer[BUFFERSIZE];
    char argvs[(BUFFERSIZE/2) + 1][BUFFERSIZE];
    int i;
    /*char* command;*/
    /*while(TRUE) {
        prompt(buffer);
    }*/

    prompt();
    read_command(argvs, buffer);
    for(i = 0; i < (BUFFERSIZE/2) +1; i++) {
        if (argvs[i][0] == '\0')
            break;
        printf("%s\n",argvs[i]);
    }

/*    char* arg[] = {"ls", "-la", NULL};
    execvp(arg[0], arg);
    /*forker(buffer);*/
	return EXIT_SUCCESS;
}

void prompt() {
    printf("> ");
    return;
}

void read_command(char argvs[(BUFFERSIZE/2) + 1][BUFFERSIZE], char* buffer) {
    int i, wordIndex, listIndex;
    fgets(buffer, BUFFERSIZE, stdin);
    wordIndex = 0;
    listIndex = 0;
    for (i = 0; i < BUFFERSIZE; i++) {
        argvs[listIndex][wordIndex] = buffer[i];
        wordIndex++;
        if (buffer[i] == ' ') {
            argvs[listIndex][wordIndex] = '\0';
            wordIndex = 0;
            listIndex++;
        }
        if (buffer[i] == '\0') {
            listIndex++;
            argvs[listIndex][0] = '\0';
            break;
        }
    }
    argvs[(BUFFERSIZE/2) + 1][0] = '\0';
    return;
}



void forker(char* buffer) {
    int childPid;
    /*pid_t childPid;*/
    int childStatus;
    int fileDescriptor[2];
    char *const argvs;
    pipe(fileDescriptor);

    childStatus = 0;
    childPid = fork();

    if(childPid >= 0) {
        /* Successful fork */
        if(childPid == 0) {
            /* Child! */
            close(STDOUT_FILENO);
            dup2(fileDescriptor[1], STDOUT_FILENO);
            close(fileDescriptor[0]);
            close(fileDescriptor[1]);

            execvp(buffer, &argvs);
        } else {
            /* Write command to child */

            waitpid(childPid, &childStatus, WUNTRACED|WCONTINUED);
            printf("%i", childStatus);
        }
    } else {
        /* Fork failed */
        printf("What");
    }
    return;
}
