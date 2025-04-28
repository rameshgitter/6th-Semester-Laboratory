/*
 * forkNexec2.c - A program that creates child processes to print filenames
 * The program can be terminated using Ctrl+C
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_FILENAME 256

int main() {
    char filename[MAX_FILENAME];
    pid_t pid;
    int status;
    
    printf("Enter executable filenames (Ctrl+C to quit):\n");
    
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
            printf("Child process printing: %s\n", filename);
            exit(0);
        }
        else {
            // Parent process
            wait(&status);
            printf("Child process completed\n");
        }
    }
    
    return 0;
}
