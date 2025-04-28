/*
 * forkNexec4.c - A program that creates child processes to execute files with arguments
 * The program reads a line where:
 * - First word is the executable filename
 * - Remaining words are command line arguments
 * The program can be terminated using Ctrl+C
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

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

// Parse input line into command and arguments
int parse_command(char *line, char **args) {
    int argc = 0;
    char *token;
    
    // Get first token (command)
    token = strtok(line, " \t\n");
    while (token != NULL && argc < MAX_ARGS - 1) {
        args[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[argc] = NULL;  // Null terminate argument list
    
    return argc;
}

int main() {
    char input_line[MAX_LINE];
    char *args[MAX_ARGS];
    pid_t pid;
    int status;
    
    printf("Enter commands (executable filename followed by arguments)\n");
    printf("Examples:\n");
    printf("  /bin/ls -l\n");
    printf("  /bin/ps aux\n");
    printf("  ./myprogram arg1 arg2\n");
    printf("Press Ctrl+C to quit\n\n");
    
    while (1) {
        printf("\nEnter command: ");
        if (fgets(input_line, MAX_LINE, stdin) == NULL) {
            break;
        }
        
        // Parse input line into command and arguments
        if (parse_command(input_line, args) == 0) {
            continue;  // Empty line
        }
        
        pid = fork();
        
        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        }
        else if (pid == 0) {
            // Child process
            printf("Executing: %s", args[0]);
            for (int i = 1; args[i] != NULL; i++) {
                printf(" %s", args[i]);
            }
            printf("\n");
            
            // Execute the command with arguments
            execvp(args[0], args);
            
            // If execvp returns, there was an error
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
