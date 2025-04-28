#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

int main() {
    pid_t pid = fork(); // Create a child process

    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }

    if (pid == 0) {
        // Child process
        printf("Child process: PID = %d\n", getpid());
        sleep(2); // Simulate some work
        exit(42); // Exit with status 42
    } else {
        // Parent process
        siginfo_t info;

        // Wait for the child process using waitid
        if (waitid(P_PID, pid, &info, WEXITED) == 0) {
            // Check child process status
            printf("Parent: Child process %d has exited.\n", info.si_pid);
            printf("Exit status: %d\n", info.si_status);
            printf("Exit reason: %s\n",
                   (info.si_code == CLD_EXITED) ? "Normal termination" : "Abnormal termination");
        } else {
            perror("waitid failed");
        }
    }

    return 0;
}

