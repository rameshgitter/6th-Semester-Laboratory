/******************************************************************************
 * Program: File Sharing Between Parent and Child Processes
 * Description: This program demonstrates how file descriptors are shared between
 *              parent and child processes after fork(), and explores various
 *              aspects of file access and manipulation.
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define BUFFER_SIZE 100

int main() {
    FILE *fp;
    char buffer[BUFFER_SIZE];
    pid_t pid;

    /* Open file before fork() */
    fp = fopen("test.txt", "r");
    if (fp == NULL) {
        perror("Error opening file");
        exit(1);
    }

    printf("Initial file position: %ld\n", ftell(fp));

    /* Read first line to demonstrate where parent starts */
    if (fgets(buffer, BUFFER_SIZE, fp) != NULL) {
        printf("Parent read before fork: %s", buffer);
        printf("File position after parent's first read: %ld\n", ftell(fp));
    }

    /* Create child process */
    pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }
    else if (pid == 0) {
        /* Child process */
        printf("\nChild process starting...\n");
        printf("Child's initial file position: %ld\n", ftell(fp));

        /* Child reads from file */
        if (fgets(buffer, BUFFER_SIZE, fp) != NULL) {
            printf("Child read: %s", buffer);
            printf("File position in child: %ld\n", ftell(fp));
        }

        /* Test if child can still read after parent closes file */
        sleep(2);  // Wait for parent to close file
        printf("\nChild attempting to read after parent closes file:\n");
        if (fgets(buffer, BUFFER_SIZE, fp) != NULL) {
            printf("Child successfully read: %s", buffer);
        } else {
            printf("Child failed to read from file\n");
        }

        fclose(fp);
        exit(0);
    }
    else {
        /* Parent process */
        printf("\nParent process continuing...\n");
        printf("Parent's file position: %ld\n", ftell(fp));

        /* Parent reads from file */
        if (fgets(buffer, BUFFER_SIZE, fp) != NULL) {
            printf("Parent read after fork: %s", buffer);
            printf("File position in parent: %ld\n", ftell(fp));
        }

        printf("\nParent closing file...\n");
        fclose(fp);

        /* Wait for child to complete */
        wait(NULL);
    }

    return 0;
}
