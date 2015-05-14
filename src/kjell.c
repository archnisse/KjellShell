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
#include <stdio.h>

#define _XOPEN_SOURCE 500
#include <unistd.h>


#define ANSI_GREEN "\x1b[0;32m"
#define ANSI_RESET "\x1b[0;0m"
#define ANSI_CYAN "\x1b[1;36m"

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
    int i, listIndex, kill;
    listIndex = 1;
    kill = 0;

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
            /* -only- if there is no double space */
            if(buffer[i+1] != ' ') {
                args[listIndex] = &buffer[i + 1];
                listIndex++;
            }
            /* Set space to null pointer to end previous arg here */
            buffer[i] = 0;
        }

        /* If we encounter newline */
        if (buffer[i] == '\n' || buffer[i] == '\r' || kill) {
            /* Just null it */
            buffer[i] = 0;
            kill = 1;
        }
    }
    buffer[i] = 0;

    return;
}

void read_command2(char* args[BUFFERSIZE]) {
    char buffer[BUFFERSIZE];
    int c = 0;
    int i = 0;
    /* Read input from stdin */
    fgets(buffer, BUFFERSIZE, stdin);
    /* Tokenize it */
    args[i] = strtok(buffer, " ");
    while(args[i]) {
        i++;
        args[i] = strtok(NULL, " ");
    }

    /* Remove newline */
    while(args[i-1][c] != 0) {
        printf("%c", args[i-1][c]);
	    c++;
    }

    args[i-1][c-1] = 0;
    args[i] = 0;

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
    int childStatus, childErrno;
    int fileDescriptor[2];
    pipe(fileDescriptor);
    childPid = fork();

    if(childPid > 0) {
        waitpid(childPid, &childStatus, 0);
    } else if(childPid == 0) {
        childErrno = execvp(args[0], args);

        if(childErrno == -1 && errno == 2) {
            printf("%s: command not found: %s\n", SHELL_NAME, "--cmd entered--");
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

/*
 * Function:    prompt
 * -------------------
 * Prints the command line prompt
 */
void prompt() {
    char cwd[256];
    getcwd(cwd, 256);
    printf("%s %s\xE2\x9E\xA1 %s%s %s",
           getenv("LOGNAME"), ANSI_GREEN, ANSI_CYAN, cwd, ANSI_RESET);
    return;
}


void checkEnv(char ** args) {
    char *printEnvArg[] = { "printenv", NULL };
    char *sortArg[] = { "sort", NULL };
    char *pagerArg[] = { "less", NULL };

    int fd1[2], fd2[2], fd3[2];
    pid_t printC = -2, sortC = -2, pagerC = -2, grepC = -2;
    int childStatus;
    int startGrep = 0;

    if(args[1] != 0) {
        /* We have arguments */
        args[0] = "grep";
        startGrep = 1;
    }

    pipe(fd1);
    pipe(fd2);
    pipe(fd3);

    printC = fork();
    if(printC > 0) sortC = fork();
    if(sortC > 0) pagerC = fork();

    if(startGrep) {
        if(pagerC > 0) grepC = fork();
    } else {
        grepC = 1;
    }

    if(printC == 0) {
        fprintf(stderr, "Print start, (p: %i, s: %i, pp: %i)\n", printC, sortC, pagerC);
        close(STDOUT_FILENO);
        /* We do this because if we are not using grep we want sort to read
         * directly from print */
        if(startGrep) {
            dup2(fd1[1], STDOUT_FILENO);
        } else {
            dup2(fd2[1], STDOUT_FILENO);
        }
        close(fd1[0]);
        close(fd1[1]);
        close(fd2[0]);
        close(fd2[1]);
        close(fd3[0]);
        close(fd3[1]);
        execvp(printEnvArg[0], printEnvArg);
    }

    if(startGrep) {
        if(grepC == 0) {
            fprintf(stderr, "Grep start\n");
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            dup2(fd1[0], STDIN_FILENO);
            dup2(fd2[1], STDOUT_FILENO);
            close(fd1[0]);
            close(fd1[1]);
            close(fd2[0]);
            close(fd2[1]);
            close(fd3[0]);
            close(fd3[1]);
            execvp(args[0], args);
        }
    }

    if(sortC == 0) {
        fprintf(stderr, "Sort start, (p: %i, s: %i, pp: %i)\n", printC, sortC, pagerC);
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        dup2(fd2[0], STDIN_FILENO);
        dup2(fd3[1], STDOUT_FILENO);
        close(fd1[0]);
        close(fd1[1]);
        close(fd2[0]);
        close(fd2[1]);
        close(fd3[0]);
        close(fd3[1]);
        execvp(sortArg[0], sortArg);
    }


    if(pagerC == 0) {
        fprintf(stderr, "Pager start, (p: %i, s: %i, pg: %i)\n", printC, sortC, pagerC);
        close(STDIN_FILENO);
        dup2(fd3[0], STDIN_FILENO);
        close(fd1[0]);
        close(fd1[1]);
        close(fd2[0]);
        close(fd2[1]);
        close(fd3[0]);
        close(fd3[1]);
        execvp(pagerArg[0], pagerArg);
    }

    if(printC > 0 && sortC > 0 && pagerC > 0 && grepC > 0) {
        close(fd1[0]);
        close(fd1[1]);
        close(fd2[0]);
        close(fd2[1]);
        close(fd3[0]);
        close(fd3[1]);
        waitpid(printC, &childStatus, 0);
        if(startGrep) waitpid(grepC, &childStatus, WUNTRACED|WCONTINUED);
        waitpid(sortC, &childStatus, 0);
        printf("sort: %i\n", childStatus);
        waitpid(pagerC, &childStatus, WUNTRACED|WCONTINUED);

    }

    printf("lol");

    return;
}


/*
 * Function:    system_commands
 * -------------------
 * Looks for system commands in the input
 *
 * input: char* args[buffersize] containing the user input
 * returns: 1 if it matched a system command (?), 0 if not
 */
int system_commands(char* args[BUFFERSIZE]) {
    if (!strcmp("exit", args[0])) {
        /* fprintf(stderr, "exiting\n"); */
        kill(getpid(), SIGTERM);
        return 1;
    }

    if (!strcmp("cd", args[0])) {
        /* fprintf(stderr, "changing directory\n");
        fprintf(stderr, "to: %s\n", args[1]); */
        chdir(args[1]);
        return 1;
    }

    if(!strcmp("checkEnv", args[0])) {
        checkEnv(args);
        return 1;
    }

    return 0;
}

int parse_background_process(char** args) {
    /* Get last character of last word in args */
    int c = 0;
    int w = 0;
    while(args[++w] != 0) {}
    while(args[w-1][++c] != 0) {}

    if(args[w-1][c-1] == '&') {
        args[w-1][c-1] = 0;
    }

    return 0;
}

int main(void) {
    char* args[BUFFERSIZE];
    while(TRUE) {
        prompt();
        read_command(args);

        /* check if there are any system commands */
        if (system_commands(args))
            continue;

        if(parse_background_process(args)) {
            /* bg_forker(args); */
        } else {
            forker(args);
        }
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
        if(childPid == 0) {
            printf("Child.\n");
            /*close(STDOUT_FILENO);*//*
            dup2(fileDescriptor[0], STDIN_FILENO);
            read(fileDescriptor[0], &child_buffer, BUFFERSIZE);
            close(fileDescriptor[0]);
            /*close(fileDescriptor[1]);*//*
            execvp(child_buffer[0], child_buffer);
        } else {
            printf("Parent\n");
            dup2(fileDescriptor[1], STDOUT_FILENO);
            write(fileDescriptor[1], buffer, BUFFERSIZE);
            /*close(fileDescriptor[0]);*//*
            close(fileDescriptor[1]);
            waitpid(childPid, &childStatus, WUNTRACED|WCONTINUED);
            printf("childstatus: %i\n", childStatus);
        }
    } else {
        printf("Fork failed");
    }
    return;
}*/

