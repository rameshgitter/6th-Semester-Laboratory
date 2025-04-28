/*
 * forkNexec3.c - A program that creates child processes to execute files
 * The program can be terminated using Ctrl+C
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_FILENAME 256

void print_exit_status(int status) {
    if (WIFEXITED(status)) {
        printf("Child exited normally with status %d\n", WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status)) {
        printf("Child terminated by signal %d (%s)\n", 
               WTERMSIG(status), strsignal(WTERMSIG(status)));
    }
    else if (WIFSTOPPED(status)) {
        printf("Child stopped by signal %d\n", WSTOPSIG(status));
    }
}

int main() {
    char filename[MAX_FILENAME];
    pid_t pid;
    int status;
    
    printf("Enter executable filenames (Ctrl+C to quit):\n");
    printf("Examples: /bin/ls, /bin/ps, ./a.out\n");
    
    while (1) {
        printf("\nEnter filename: ");
        if (fgets(filename, MAX_FILENAME, stdin) == NULL) {
            break;
        }
        
        // Remove trailing newline if present
        filename[strcspn(filename, "\n")] = 0;
        
        pid = fork();
        
        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        }
        else if (pid == 0) {
            // Child process
            printf("Attempting to execute: %s\n", filename);
            execl(filename, filename, NULL);
            
            // If execl returns, there was an error
            perror("Execution failed");
            exit(1);
        }
        else {
            // Parent process
            wait(&status);
            printf("\nChild process status:\n");
            print_exit_status(status);
        }
    }
    
    return 0;
}
