/*
 ============================================================================
 Name        : Kjell.c
 Author      : Viktor Björkholm & Jesper Bränn
 Version     : 0.112
 Copyright   : 2015
 Description : Kjell Shell. A C linux Shell.


       * Check all commands if they fail (error return values and errno)
         * Give feedback
       * (Maybe use WIFEXITED etc)
       * TODO: Try both more and less in pager after fail, if they have not been tested already
       * TODO: testa kör med valgrid för att visa att inget läcker
 ============================================================================
 */
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>  /* gettimeofday() */
#include <signal.h>
#include <errno.h>

#define ANSI_GREEN "\x1b[0;32m"
#define ANSI_RESET "\x1b[0;0m"
#define ANSI_CYAN "\x1b[1;36m"

#define TRUE 1
#define BUFFERSIZE 80 /* */

#ifndef SIGDET
    #define SIGDET 0
#endif

pid_t wait4(pid_t pid, int *status, int options, struct rusage *rusage);

static const char SHELL_NAME[] = "Kjell Shell";
static char prev_path[BUFFERSIZE];
static char prev_cmd[BUFFERSIZE];

void poll_background_process(int);

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
int read_command(char buffer[BUFFERSIZE], char* args[BUFFERSIZE]) {
    int i = 0;

    /* Read from STDIN to buffer */
    if(fgets(buffer, BUFFERSIZE, stdin) == NULL) {
        perror(NULL);
    }
    buffer[strlen(buffer) - 1] = 0;
    strncpy(prev_cmd, buffer, BUFFERSIZE);

    args[0] = strtok(buffer, " ");
    while(args[i] != NULL) {
        i++;
        args[i] = strtok(NULL, " ");
    }

    if(args[0] == 0) {
        return 0;
    }

    return 1;
}

/* Function:    timesubtract
 * -------------------------
 * A reimplementation for timersub to not have to define _BSD_SOURCE
 * input: after timeval, before timeval and the timeval to store the difference.
 * returns: void
 */
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
 * ------------------------------
 * Forks a process that runs a command in the foreground.
 *
 * input: char* const* args containing in first position the command to execute
 * and the rest as arguments
 * returns: void
 */
void foreground_forker(char* const* args) {
    pid_t childPid;
    int childStatus, childErrno, fileDescriptor[2], i;
    struct rusage rus;
    struct timeval time_begin, time_end, time_spent;

    gettimeofday(&time_begin, NULL);
    if(pipe(fileDescriptor) == -1) {
        perror(NULL);
        return;
    }

    sighold(SIGCHLD);
    childPid = fork();

    if(childPid > 0) {
        if (wait4(childPid, &childStatus, 0, &rus) <= 0) {
            fprintf(stderr, "Error waiting for child");
        }
        sigrelse(SIGCHLD);

        gettimeofday(&time_end, NULL);
        timesubtract(&time_end, &time_begin, &time_spent);

        printf("%lu.%05lis user\t %lu.%05lis system\t %lu.%05lis total\n",
               rus.ru_utime.tv_sec, rus.ru_utime.tv_usec,
               rus.ru_stime.tv_sec, rus.ru_stime.tv_usec,
               time_spent.tv_sec, time_spent.tv_usec);

    } else if(childPid == 0) {
        /* Start listening to SIGQUIT again with default handling */
        signal(SIGQUIT, SIG_DFL);

        childErrno = execvp(args[0], args);

        if(childErrno == -1 && errno == 2) {
            fprintf(stderr, "%s: command not found: %s\n", SHELL_NAME, prev_cmd);
            for(i = 0; i < BUFFERSIZE; i++) {
                if(args[i] == 0) break;
                fprintf(stderr, "%s ", args[i]);
            }
            fprintf(stderr, "\n");
            exit(-1);
        }
    } else {
        printf("Couldn't fork");
    }
}

/*
 * Function:    background_forker
 * ------------------------------
 * Forks a process into the background and prints its pid.
 * input: the argument list to the process
 * returns: void
 */
void background_forker(char* const* args) {
    pid_t childPid;
    int childErrno;
    childPid = fork();
    if(childPid == 0) {
        /* Start listening to SIGQUIT again with default handling */
        signal(SIGQUIT, SIG_DFL);

        childErrno = execvp(args[0], args);

        /*Den här if-satsen är inte nödvändig va?*/
        if(childErrno == -1 && errno == 2) {
            printf("%s: command not found: %s\n", SHELL_NAME, prev_cmd);
        }
    } else if(childPid > 0) {
        printf("Process [%i] started in the background\n", childPid);
    }
    return;
}

/*
 * Function:    print_buffer
 * -------------------------
 * Prints a string array.
 * input: the string array to be printed.
 * returns: void
 */
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
 * Prints the command line prompt with username and current path.
 */
void prompt() {
    char cwd[256];
    if(getcwd(cwd, 256) == NULL) {
        cwd[0] = 'x';
    }
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

/*
 * Function:    checkEnv
 * ---------------------
 * Paginates a sorted version of all environment variables in the system.
 * input: the command and whether or not to grep for a particular variable.
 * returns: void
 */
void checkEnv(char ** args) {
    char *printEnvArg[] = { "printenv", NULL };
    char *sortArg[] = { "sort", NULL };
    char *pagerArg[] = { "less", "-C", NULL };

    int fd1[2], fd2[2], fd3[2];
    pid_t printC = -2, sortC = -2, pagerC = -2, grepC = -2;
    int childStatus;
    int startGrep = 0;

    if(sighold(SIGCHLD) == -1) {
        perror(NULL);
        return;
    }
    if(args[1] != 0) {
        /* We have arguments */
        args[0] = "grep";
        startGrep = 1;
    }
    if (pipe(fd1) == -1) {
        fprintf(stderr, "Pipe failed\n");
        return;
    }
    if (pipe(fd2) == -1) {
        fprintf(stderr, "Pipe failed\n");
        return;
    }
    if (pipe(fd3) == -1) {
        fprintf(stderr, "Pipe failed\n");
        return;
    }

    printC = fork();
    if(printC == -1) { perror(NULL); return; }
    if(printC > 0) sortC = fork();
    if(sortC == -1) { perror(NULL); return; }
    if(sortC > 0) pagerC = fork();
    if(pagerC == -1) { perror(NULL); return; }
    if(startGrep) {
        if(pagerC > 0) grepC = fork();
        if(grepC == -1) { perror(NULL); return; }
    } else {
        grepC = 1;
    }
   

    if(printC == 0) {
        close(STDOUT_FILENO);
        /* We do this because if we are not using grep we want sort to read
         * directly from print */
        if(startGrep) {
            if(dup2(fd1[1], STDOUT_FILENO) == -1) { perror(NULL); pipeCloser(fd1, fd2, fd3); return; }
        } else {
            if(dup2(fd2[1], STDOUT_FILENO) == -1) { perror(NULL); pipeCloser(fd1, fd2, fd3); return; }
        }
        pipeCloser(fd1, fd2, fd3);
        execvp(printEnvArg[0], printEnvArg);
        perror("Could not run printenv");
        return;
    }

    if(startGrep) {
        if(grepC == 0) {
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            if(dup2(fd1[0], STDIN_FILENO) == -1)  { perror(NULL); pipeCloser(fd1, fd2, fd3); return; }
            if(dup2(fd2[1], STDOUT_FILENO) == -1) { perror(NULL); pipeCloser(fd1, fd2, fd3); return; }
            pipeCloser(fd1, fd2, fd3);
            execvp(args[0], args);
            perror("Could not run grep");
        }
    }

    if(sortC == 0) {
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        if(dup2(fd2[0], STDIN_FILENO) == -1) { perror(NULL); pipeCloser(fd1, fd2, fd3); return; }
        if(dup2(fd3[1], STDOUT_FILENO) == -1) { perror(NULL); pipeCloser(fd1, fd2, fd3); return; }
        pipeCloser(fd1, fd2, fd3);
        execvp(sortArg[0], sortArg);
        perror("Could not run sort");
    }


    if(pagerC == 0) {
        close(STDIN_FILENO);
        if(dup2(fd3[0], STDIN_FILENO) == -1) { perror(NULL); pipeCloser(fd1, fd2, fd3); return; }
        pagerArg[0] = getenv("PAGER");
        if(pagerArg[0] == '\0') {
            pagerArg[0] = "less";
        }
        pipeCloser(fd1, fd2, fd3);

        execvp(pagerArg[0], pagerArg);
        /* exec-functions only return if an error has occured, so lets try the different pager down here. */
        if (!strcmp(pagerArg[0], "less")) {
            /* Is equal to less */
            pagerArg[0] = "more";
        } else {
            pagerArg[0] = "less";
        }
        execvp(pagerArg[0], pagerArg);
    }

    if(printC > 0 && sortC > 0 && pagerC > 0 && grepC > 0) {
        pipeCloser(fd1, fd2, fd3);
        waitpid(printC, &childStatus, 0);
        if(startGrep) waitpid(grepC, &childStatus, 0);
        waitpid(sortC, &childStatus, 0);
        waitpid(pagerC, &childStatus, 0);
        if(sigrelse(SIGCHLD) == -1) {
            perror(NULL);
            exit(EXIT_FAILURE);
        }
    }

    return;
}

/*
 * Function:    system_cd_prev
 * ---------------------------
 * Gets the previous path if necessary
 * input: argument list
 * returns: an int indicating whether or not there was a previous path
 *          0 if there was, 1 if there was not.
 */
int system_cd_prev(char * args[BUFFERSIZE]) {
    if(args[1][0] == '-') {
        if(args[1][1] == 0) {
            if(prev_path != 0) {
                strcpy(args[1], prev_path);
            } else {
                return 1;
            }
        } else {
            fprintf(stderr, "%s", strerror(20));
            return 1;
        }
    }

    return 0;
}

/*
 * Function:    system_cd_home
 * ---------------------------
 * Checks if the user has entered a tilde in the beginning of the path to cd to.
 * If so, stores the expanded path in the argument list.
 * input: argument list and a buffer to store the expanded path.
 * returns: void.
 */
void system_cd_home(char * args[BUFFERSIZE], char addedPath[BUFFERSIZE]) {
    int lineLen;
    int i;
    char* home;
    
    if (args[1][0] == '~') {
        if (args[1][1] == '/' || args[1][1] == 0) {
            /* Home of user */
            home = getenv("HOME");
            strcpy(addedPath, home);
            lineLen = strlen(addedPath);
            /*addedPath[lineLen] = 0;*/
            if(args[1][1] == '/') {

                addedPath[lineLen] = '/';
                for (i = 1; i < strlen(args[1]); i++) {
                    addedPath[lineLen + i - 1] = args[1][i];
                    if (args[1][i] == 0) break;
                }
                addedPath[lineLen + i - 1] = 0;
                addedPath[strlen(addedPath)] = 0;
            }
            args[1] = addedPath;
        } else {
            /* Home of someone else */
            /* This does not need to be implemented
             * since there are no others user on your system. */
        }
    }

    return;
}

/*
 * Function:    system_cd
 * ----------------------
 * Handles the cd command, and changes directory.
 * input: argument list
 * returns: void
 */
void system_cd(char * args[BUFFERSIZE]) {
    char addedPath[BUFFERSIZE];
    memset(addedPath, 0, BUFFERSIZE);

    /* Perform expansion */
    if(args[1] != 0) {
        if(system_cd_prev(args)) return;
        system_cd_home(args, addedPath);
    } else {
        args[1] = getenv("HOME");
    }

    if(getcwd(prev_path, BUFFERSIZE) == NULL) {
        fprintf(stderr, "%s: could not get CWD %s\n", SHELL_NAME, strerror(errno));
    }

    if(chdir(args[1]) < 0) {
        fprintf(stderr, "%s\n", strerror(errno));

    }

}

/*
 * Function:    internal_commands
 * -------------------
 * Looks for system commands in the input
 *
 * input: char* args[buffersize] containing the user input
 * returns: 1 if it matched a system command, 0 if not
 */
int internal_commands(char* args[BUFFERSIZE]) {
    if (!strcmp("exit", args[0])) {

        /* Ask the other processes in the group to quit */
        if(kill(0, SIGQUIT) == -1) {
            perror(NULL);
        }

        /* Clean up zombies */
        poll_background_process(0);

        /* Start listening to SIGQUIT again with default handling */
        signal(SIGQUIT, SIG_DFL);

        /* Kill itself */
        if(kill(getpid(), SIGQUIT) == -1) {
            perror(NULL);
        }

        return 1;
    }

    if (!strcmp("cd", args[0])) {
        system_cd(args);
        return 1;
    }

    if(!strcmp("checkEnv", args[0])) {
        /* Don't allow interrupts when we're in checkEnv */
        checkEnv(args);
        return 1;
    }

    return 0;
}

/*
 * Function:    parse_background_process
 * -------------------------------------
 * Parses the argument list to see if an ampersand (&) was entered to see if
 * the user wants a background process.
 * input: the argument list
 * returns: an int, 1 if an ampersand was entered, 0 otherwise.
 */
int parse_background_process(char** args) {
    int i;    
    /* Get last character of last word in args */
    for(i = 0; i < BUFFERSIZE; i++) {
        if(args[i] == 0) break;
        if(!strcmp(args[i], "&")) {
            args[i] = 0;
            return 1;
        }
        if(args[i][strlen(args[i])-1] == '&') {
            args[i][strlen(args[i])-1] = 0;
            return 1;
        }
    }


    return 0;
}

/*
 * Function:    stdin_open
 * -----------------------
 * Checks whether or not stdin is open for reading.
 * returns: 1 if it is open, 0 otherwise.
 */
int stdin_open() {
    return !feof(stdin);
}

/*
 * Function:    sigchild_handler
 * -----------------------------
 * The SIGCHLD signal handler.
 * input: signal number, signal info, and context
 * returns: void
 */
void sigchild_handler(int signo, siginfo_t* info, void * context) {
    int childStatus;
    pid_t waitRet;
    struct rusage rus;

    waitRet = wait4(info->si_pid, &childStatus, WNOHANG, &rus);
    if(waitRet > 0) {
        printf("\nProcess [%i] finished\n", waitRet);
        printf("%lu.%05lis user\t %lu.%05lis system\n ",
               rus.ru_utime.tv_sec, rus.ru_utime.tv_usec,
               rus.ru_stime.tv_sec, rus.ru_stime.tv_usec);
        prompt();
        if(fflush(stdout) == -1) {
            perror(NULL);
        }
    }
}

/*
 * Function:    sigint_handler
 * ---------------------------
 * Signal handler for the SIGINT signal.
 * input: signal number, signal info, and context
 * returns: void
 */
void sigint_handler(int signo, siginfo_t* info, void * context) {
/*    printf("\n");
    prompt();
    fflush(stdout);*/
}

/*
 * Function:    register_signal_handlers
 * -------------------------------------
 * Registers handlers for both of the SIGCHLD and SIGINT signals.
 * input: void
 * returns: void
 */
void register_signal_handlers() {
    struct sigaction sa_chld;
    struct sigaction sa_int;

    if(SIGDET == 1) {
        /* Set up handler */
        sa_chld.sa_sigaction = &sigchild_handler;
        if(sigemptyset(&sa_chld.sa_mask) == -1) {
            perror(NULL);
            exit(EXIT_FAILURE);
        }
        sa_chld.sa_flags = SA_RESTART | SA_SIGINFO;
        if(sigaction(SIGCHLD, &sa_chld, 0) == -1) {
            perror(NULL);
            exit(EXIT_FAILURE);
        }
    }

    sa_int.sa_sigaction = &sigint_handler;
    if(sigemptyset(&sa_int.sa_mask) == -1) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }
    sa_int.sa_flags = SA_RESTART | SA_SIGINFO | SA_NOCLDWAIT;
    if(sigaction(SIGINT, &sa_int, 0) == -1) {
        perror(NULL);
        exit(EXIT_FAILURE);
    }

}

/*
 * Function:    main
 * -----------------
 * The main function. Starts up the shell.
 * returns: the exit status
 */
int main(void) {
    char buffer[BUFFERSIZE] = {0};
    char* args[BUFFERSIZE] = {0};

    /* Ignore the SIGQUIT signal for the shell process */
    signal(SIGQUIT, SIG_IGN);

    register_signal_handlers();

    while(stdin_open()) {
        memset(args, 0, BUFFERSIZE);
        memset(buffer, 0, BUFFERSIZE);

        if(SIGDET != 1) {
            poll_background_process(WNOHANG);
        }

        prompt();
        if(read_command(buffer, args) == 0)
            continue;


        /* check if there are any system commands */
        if (internal_commands(args))
            continue;

        if(parse_background_process(args)) {
            background_forker(args);
        } else {
            foreground_forker(args);
        }
    }

	return EXIT_SUCCESS;
}

void poll_background_process(int options) {
    int childStatus;
    int waitRet;
    struct rusage rus;

    waitRet = wait4(0, &childStatus, options, &rus);
    while(waitRet > 0) {
        if (waitRet < 0) {
            if (errno == ECHILD) {
                return;
            }
        }
        if (waitRet > 0) {
            printf("Process [%i] finished\n", waitRet);
            printf("%lu.%05lis user\t %lu.%05lis system\n ",
                   rus.ru_utime.tv_sec, rus.ru_utime.tv_usec,
                   rus.ru_stime.tv_sec, rus.ru_stime.tv_usec);
            fflush(stdout);
        }
        waitRet = wait4(0, &childStatus, options, &rus);
    }
}

