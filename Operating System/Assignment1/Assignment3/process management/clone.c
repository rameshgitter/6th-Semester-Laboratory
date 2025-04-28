#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define STACK_SIZE 1024 * 1024 // Stack size for the child

// Function executed by the child process
int child_func(void *arg) {
    printf("Child process: PID = %d, Argument = %s\n", getpid(), (char *)arg);
    return 0; // Exit with status 0
}

int main() {
    // Allocate stack for the child process
    void *stack = malloc(STACK_SIZE);
    if (!stack) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Create a child process using clone
    char *child_arg = "Hello from child!";
    int clone_flags = SIGCHLD; // Ensure the child is reaped as a child process
    pid_t child_pid = clone(child_func, stack + STACK_SIZE, clone_flags, child_arg);

    if (child_pid == -1) {
        perror("clone");
        free(stack);
        exit(EXIT_FAILURE);
    }

    printf("Parent process: PID = %d, Child PID = %d\n", getpid(), child_pid);

    // Wait for the child process to complete
    if (waitpid(child_pid, NULL, 0) == -1) {
        perror("waitpid");
        free(stack);
        exit(EXIT_FAILURE);
    }

    printf("Child process has exited\n");

    free(stack); // Free the allocated stack
    return 0;
}

