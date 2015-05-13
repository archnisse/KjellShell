/*
 ============================================================================
 Name        : Kjell.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Kjell the Shell. A C linux Shell.
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h> /* included to make the WUNTRACES stuff work */
#include <sys/types.h> /* pid_t */
#include <signal.h>
#include <errno.h>

#define TRUE 1
#define BUFFERSIZE 80 /* */

static const char SHELL_NAME[] = "Kjell Shell";


/*
 * Function:    read_command
 * -------------------------
 *  Takes a char ** args as function input.
 *  Read input from stdin and parses it into the input char ** with every word
 *  as its own entry.
 *
 *  input: char ** args
 *
 *  returns: void
 *
 *  Ex. args[0] = first word, args[1] = second word, ..., args[n] = NULL.
 */
void read_command(char* args[BUFFERSIZE]) {
    char buffer[BUFFERSIZE];
    int i, listIndex;
    listIndex = 1;

    /* Read from STDIN to buffer */
    fgets(buffer, BUFFERSIZE, stdin);

    /* Empty args */
    for(i = 0; i < BUFFERSIZE; i++) {
        args[i] = 0;
    }

    /* Split buffer into words in args */
    args[0] = &buffer[0];

    /* Loop through buffer */
    for (i = 0; i < BUFFERSIZE; i++) {
        /* If current character is a space */
        if (buffer[i] == ' ') {
            /* Set the argument to point at the next character in buffer */
            args[listIndex] = &buffer[i + 1];
            /* Set space to null pointer to end previous arg here */
            buffer[i] = 0;
            listIndex++;
        }

        /* If we encounter newline */
        if (buffer[i] == '\n') {
            /* Just null it */
            buffer[i] = 0;
            break;
        }
    }

    return;
}

/*
 * Function:    forker
 * -------------------
 * Forks a process that runs a command.
 *
 * input: char* const* args containing in first position the commnand to execute and the rest as arguments
 * returns: void
 */
void forker(char* const* args) {
    pid_t childPid;
    int childStatus;
    int childErrno;
    int fileDescriptor[2];
    pipe(fileDescriptor);
    childPid = fork();

    if(childPid > 0) {
        waitpid(childPid, &childStatus, 0);
    } else if(childPid == 0) {
        childErrno = execvp(args[0], args);

        if(errno == 2) {
            printf("%s: command not found\n", SHELL_NAME);
        }
    } else {
        printf("Couldn't fork");
    }
}


void print_buffer(char ** args) {
    int i;
    for(i = 0; i < BUFFERSIZE; i++) {
        if (args[i] == '\0')
            break;
        printf("%s\n",args[i]);
    }
}

void prompt() {
    printf("> ");
    return;
}

/*
 * Function:    interpret
 * -------------------
 * Looks for system commands in the input
 *
 * input: char* args[buffersize] containing the user input
 * returns: 1 if it matched a system command (?), 0 if not
 */
int interpret(char* args[BUFFERSIZE])
{
    if (!strcmp("exit", args[0]))
    {
        fprintf(stderr, "exiting\n");

        kill(getpid(), SIGTERM);
        return 1;
    }
    if (!strcmp("cd", args[0]))
    {
        fprintf(stderr, "chaning directory\n");
        fprintf(stderr, "to: %s\n", args[1]);
        chdir(args[1]);
        return 1;
    }
    return 0;
}

int main(void) {
    char* args[BUFFERSIZE];
    int i;
    while(TRUE) {
        prompt();
        read_command(args);
        print_buffer(args);
        /* interpret if there are any system commands */
        if (interpret(args))
            continue;
        /* print_buffer(args); */
        forker(args);
    }

	return EXIT_SUCCESS;
}



/*
void forker(char* buffer) {
    pid_t childPid;
    int childStatus;
    int fileDescriptor[2];
    char *const argvs;
    char ** child_buffer;

    pipe(fileDescriptor);

    childStatus = 0;
    childPid = fork();

    if(childPid >= 0) {
        /* Successful fork *//*
        if(childPid == 0) {
            /* Child! *//*
            printf("Child.\n");
            /*close(STDOUT_FILENO);*//*
            dup2(fileDescriptor[0], STDIN_FILENO);
            read(fileDescriptor[0], &child_buffer, BUFFERSIZE);
            close(fileDescriptor[0]);
            /*close(fileDescriptor[1]);*//*
            execvp(child_buffer[0], child_buffer);
        } else {
            /* Write command to child *//*
            printf("Parent\n");
            dup2(fileDescriptor[1], STDOUT_FILENO);
            write(fileDescriptor[1], buffer, BUFFERSIZE);
            /*close(fileDescriptor[0]);*//*
            close(fileDescriptor[1]);
            waitpid(childPid, &childStatus, WUNTRACED|WCONTINUED);
            /* TODO: Den är printen kommer den aldrig till. varför? Vad händer i child?*//*
            printf("childstatus: %i\n", childStatus);
        }
    } else {
        /* Fork failed *//*
        printf("What");
    }
    return;
}*/

