#include <stdio.h>
#include <unistd.h>

int main() {
    pid_t pid = vfork();

    if (pid < 0) {
        perror("vfork failed");
    } else if (pid == 0) {
        printf("Child process: PID = %d\n", getpid());
        _exit(0); // Child must exit or call exec() to prevent undefined behavior
    } else {
        printf("Parent process: PID = %d\n", getpid());
    }

    return 0;
}

