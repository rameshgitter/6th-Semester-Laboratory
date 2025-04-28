/******************************************************************************
 * Program: File Writing Between Parent and Child Processes - Simple Version
 * Description: Demonstrates how parent and child processes share file descriptors
 *              and how their writes interact with the same file.
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    FILE *fp;
    pid_t pid;

    /* Open file for writing */
    fp = fopen("output.txt", "w");
    if (fp == NULL) {
        perror("Error opening file");
        exit(1);
    }

    /* Write initial message */
    fprintf(fp, "Starting message\n");
    fflush(fp);

    /* Create child process */
    pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }
    else if (pid == 0) {
        /* Child process */
        sleep(1);  // Wait to ensure clear sequencing
        fprintf(fp, "This is written by child process\n");
        fflush(fp);
        exit(0);
    }
    else {
        /* Parent process */
        fprintf(fp, "This is written by parent process\n");
        fflush(fp);
        
        /* Wait for child to complete */
        wait(NULL);
    }

    fclose(fp);

    /* Now let's read and display the contents of the file */
    printf("\nContents of output.txt:\n");
    printf("------------------------\n");
    
    fp = fopen("output.txt", "r");
    if (fp != NULL) {
        char buffer[100];
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            printf("%s", buffer);
        }
        fclose(fp);
    }
    printf("------------------------\n");

    return 0;
}
