/*
 ============================================================================
 Name        : Kjell.c
 Author      : Viktor Björkholm & Jesper Bränn
 Version     : 0.111
 Copyright   : 2015
 Description : Kjell Shell. A C linux Shell.

 TODO:       : Check if fork went bad in checkEnv
 TODO:       : Check all system commands if they fail
 TODO:       : BG processes
 TODO:       : Vi borde kolla på WIFEXITED och de där kommandona.
 ============================================================================
 */
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h> /* included to make the WUNTRACES stuff work */
#include <sys/types.h> /* pid_t */
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h> /* time() */
#include <sys/time.h>  /* gettimeofday() */
#include <sys/resource.h>
#include <unistd.h>  /* needed on some linux systems */


#define ANSI_GREEN "\x1b[0;32m"
#define ANSI_RESET "\x1b[0;0m"
#define ANSI_CYAN "\x1b[1;36m"

#define TRUE 1
#define BUFFERSIZE 80 /* */
#define SIGDET 1

static const char SHELL_NAME[] = "Kjell Shell";
static char prev_path[BUFFERSIZE];

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
int read_command(char* args[BUFFERSIZE]) {
    char buffer[BUFFERSIZE];
    int i, listIndex, kill, insideQuote;
    listIndex = 1;
    kill = 0;
    insideQuote = 0;

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
        if(buffer[i] == '"' && insideQuote == 1) { insideQuote = 0; }
        else if(buffer[i] == '"' && !insideQuote) { insideQuote = 1; }

        if (buffer[i] == ' ' && !insideQuote) {
            /* Set the argument to point at the next character in buffer */
            /* -only- if there is no double space */
            if(buffer[i+1] != ' ' && buffer[i+1] != 0 && buffer[i+1] != '\n') {
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

    if(!strcmp(args[0], "")) {
        return 0;
    }

    return 1;
}

void timesubtract(struct timeval *a, struct timeval *b, struct timeval *res) {
    res->tv_sec = 0;
    res->tv_usec = 0;
    res->tv_sec = a->tv_sec - b->tv_sec;
    res->tv_usec = a->tv_usec - b->tv_usec;
    if (res->tv_usec < 0) {
        res->tv_sec -= 1;
        res->tv_usec += 1000000;
    }
}

/*
 * Function:    foreground_forker
 * -------------------
 * Forks a process that runs a command in the foreground.
 *
 * input: char* const* args containing in first position the commnand to execute
 * and the rest as arguments
 * returns: void
 */
void foreground_forker(char* const* args) {
    pid_t childPid;
    int childStatus, childErrno, fileDescriptor[2];
    struct rusage usage_before, usage_after, usage_spent;
    struct timeval time_begin, time_end, time_spent;

    gettimeofday(&time_begin, NULL);
    pipe(fileDescriptor);
    sighold(SIGCHLD);
    childPid = fork();

    getrusage(RUSAGE_CHILDREN, &usage_before);
    if(childPid > 0) {
        waitpid(childPid, &childStatus, 0);
        sigrelse(SIGCHLD);
        getrusage(RUSAGE_CHILDREN, &usage_after);
        timesubtract(&usage_after.ru_utime, &usage_before.ru_utime, &usage_spent.ru_utime);
        timesubtract(&usage_after.ru_stime, &usage_before.ru_stime, &usage_spent.ru_stime);

        gettimeofday(&time_end, NULL);
        timesubtract(&time_end, &time_begin, &time_spent);

        printf("%lu.%05lis user\t %lu.%05lis system\t %lu.%05li total\n",
               usage_spent.ru_utime.tv_sec, usage_spent.ru_utime.tv_usec,
               usage_spent.ru_stime.tv_sec, usage_spent.ru_stime.tv_usec,
               time_spent.tv_sec, time_spent.tv_usec);

    } else if(childPid == 0) {
        childErrno = execvp(args[0], args);

        if(childErrno == -1 && errno == 2) {
            printf("%s: command not found: %s\n", SHELL_NAME, "--cmd entered--");
        }
    } else {
        printf("Couldn't fork");
    }
}

void background_forker(char* const* args) {
    pid_t childPid;
    int childErrno;
    childPid = fork();
    if(childPid == 0) {
        childErrno = execvp(args[0], args);

        if(childErrno == -1 && errno == 2) {
            printf("%s: command not found: %s\n", SHELL_NAME, "--cmd entered--");
        }
    }
    return;
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

/*
 * Function:    pipeCloser
 * -----------------------
 * Closes the three pipes used in checkEnv
 * input: the three pipes
 *
 */
void pipeCloser(int fd1[2], int fd2[2], int fd3[2]) {
    close(fd1[0]);
    close(fd1[1]);
    close(fd2[0]);
    close(fd2[1]);
    close(fd3[0]);
    close(fd3[1]);
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
        close(STDOUT_FILENO);
        /* We do this because if we are not using grep we want sort to read
         * directly from print */
        if(startGrep) {
            dup2(fd1[1], STDOUT_FILENO);
        } else {
            dup2(fd2[1], STDOUT_FILENO);
        }
        pipeCloser(fd1, fd2, fd3);
        execvp(printEnvArg[0], printEnvArg);
    }

    if(startGrep) {
        if(grepC == 0) {
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            dup2(fd1[0], STDIN_FILENO);
            dup2(fd2[1], STDOUT_FILENO);
            pipeCloser(fd1, fd2, fd3);
            execvp(args[0], args);
        }
    }

    if(sortC == 0) {
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        dup2(fd2[0], STDIN_FILENO);
        dup2(fd3[1], STDOUT_FILENO);
        pipeCloser(fd1, fd2, fd3);
        execvp(sortArg[0], sortArg);
    }


    if(pagerC == 0) {
        /*
         * TODO: Handle if it fails and retry with another pager
         */
        pagerArg[0] = getenv("PAGER");
        if(!strcmp(pagerArg[0], "")) {
            pagerArg[0] = "less";
        }
        close(STDIN_FILENO);
        dup2(fd3[0], STDIN_FILENO);
        pipeCloser(fd1, fd2, fd3);
        execvp(pagerArg[0], pagerArg);
    }

    if(printC > 0 && sortC > 0 && pagerC > 0 && grepC > 0) {
        pipeCloser(fd1, fd2, fd3);
        waitpid(printC, &childStatus, 0);
        if(startGrep) waitpid(grepC, &childStatus, 0);
        waitpid(sortC, &childStatus, 0);
        waitpid(pagerC, &childStatus, 0);
    }

    return;
}

int system_cd_prev(char * args[BUFFERSIZE]) {
    if(args[1][0] == '-') {
        if(args[1][1] == 0) {
            if(prev_path != 0) {
                strcpy(args[1], prev_path);
            } else {
                return 1;
            }
        } else {
            fprintf(stderr, strerror(20));
            return 1;
        }
    }

    return 0;
}

void system_cd_home(char * args[BUFFERSIZE]) {
    int lineLen;
    int i;
    char addedPath[BUFFERSIZE];
    char * home;

    if (args[1][0] == '~') {
        if (args[1][1] == '/' || args[1][1] == 0) {
            for(i = 0; i < sizeof(addedPath); i++) {
                addedPath[i] = 0;
            }

            /* Home of user */
            home = getenv("HOME");
            lineLen = strlen(home);
            for(i = 0; i < lineLen; i++) {
                addedPath[i] = home[i];
            }
            addedPath[lineLen] = '/';
            addedPath[lineLen+1] = 0;
            if(args[1][1] == '/') {
                for (i = 1; i < strlen(args[1]); i++) {
                    addedPath[lineLen + i - 1] = args[1][i];
                    if (args[1][i] == 0) break;
                }
                addedPath[lineLen + i] = 0;
                addedPath[strlen(addedPath)] = 0;
            }
            args[1] = addedPath;
        } else {
            /* Home of someone else */
        }
    }

    return;
}

void system_cd(char * args[BUFFERSIZE]) {
    /* Perform expansion */
    if(args[1] != 0) {
        if(system_cd_prev(args)) return;
        system_cd_home(args);
    } else {
        args[1] = getenv("HOME");
    }

    args[1][strlen(args[1])] = 0;
    printf("Trying: %s\n", args[1]);
    getcwd(prev_path, BUFFERSIZE);
    if(chdir(args[1]) < 0) {
        fprintf(stderr, "%s\n", strerror(errno));
    }
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
        /* Ignore the SIGQUIT signal for this process */
        signal(SIGQUIT, SIG_IGN);

        /* Ask the other processes in the group to quit */
        kill(0, SIGQUIT);

        /* Clean up zombies */
        poll_background_process();

        /* Start listening to SIGQUIT again with default handling */
        signal(SIGQUIT, SIG_DFL);

        /* Kill itself */
        kill(getpid(), SIGQUIT);

        return 1;
    }

    if (!strcmp("cd", args[0])) {
        system_cd(args);
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
    if(!strcmp(args[w-1], "&")) {
        args[w-1] = 0;
        return 1;
    }
    while(args[w-1][++c] != 0) {}
    if(args[w-1][c-1] == '&') {
        args[w - 1][c - 1] = 0;
        return 1;
    }

    return 0;
}

int stdin_open() {
    return !feof(stdin);
}

void poll_background_process() {
    int childStatus;
    int counter;
    int waitRet;
    counter = 0;
    while(counter < 10) {
        waitRet = waitpid(0, &childStatus, WNOHANG);
        if (waitRet < 0) {
            if (errno == ECHILD) {
                return;
            }
        }
        if (waitRet == 0) counter++;
        if (waitRet > 0) {
            printf("Process [%i] finished\n", waitRet);
        }
    }
}

void sigchild_handler(int signo, siginfo_t* info, void * context) {
    int childStatus;
    int waitRet;
    /* Don't allow children to kill their parents */
    /* if(info->si_pid != getppid() && info->si_pid != getpid()) return; */

    waitRet = waitpid(0, &childStatus, WNOHANG);
    if(waitRet > 0) {
        printf("\nProcess [%i] finished\n", waitRet);
        prompt();
        fflush(stdout);
    }
}

void sigint_handler(int signo, siginfo_t* info, void * context) {
    printf("\n");
    fflush(stdout);
}

void register_children_handlers() {
    struct sigaction sa_chld;
    struct sigaction sa_int;

    if(SIGDET == 1) {
        /* Set up handler */
        sa_chld.sa_sigaction = &sigchild_handler;
        sa_chld.sa_flags = SA_RESTART | SA_SIGINFO;
        sigaction(SIGCHLD, &sa_chld, 0);
    }

    sa_int.sa_sigaction = &sigint_handler;
    sa_int.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction(SIGINT, &sa_int, 0);

}

int main(void) {
    char* args[BUFFERSIZE];

    register_children_handlers();


    while(stdin_open()) {

        if(SIGDET != 1) {
            poll_background_process();
        }

        prompt();
        if(read_command(args) == 0)
            continue;

        /* check if there are any system commands */
        if (system_commands(args))
            continue;

        if(parse_background_process(args)) {
            background_forker(args);
        } else {
            foreground_forker(args);
        }
    }

	return EXIT_SUCCESS;
}
