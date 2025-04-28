#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>

int main(int argc, char* argv[]) {
    pid_t id = fork(); // Create a child process
    int n;

    if (id < 0) {
        perror("fork failed");
        exit(1);
    }

    if (id == 0) {
        // Child process
        n = 1; // Start counting from 1
    } else {
        // Parent process
        n = 6; // Start counting from 6

        // Wait for the child to finish
        int status;
        struct rusage usage;
        pid_t child_pid = wait4(-1, &status, 0, &usage);

        if (child_pid > 0) {
            if (WIFEXITED(status)) {
                printf("Child process %d exited with status %d\n", child_pid, WEXITSTATUS(status));
            }
            printf("Child's resource usage:\n");
            printf("User time: %ld.%06lds\n", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
            printf("System time: %ld.%06lds\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
        } else {
            perror("wait4 failed");
        }
    }

    // Loop to print numbers
    for (int i = n; i < n + 5; i++) {
        printf("%d ", i);
    }
    printf("\n");

    return 0;
}
