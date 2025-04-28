#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    char *args[] = {"ls", "-l", NULL};  // Arguments for the program
    char *env[] = {NULL};              // Environment variables

    // Open the directory containing the executable
    int dirfd = open("/bin", O_RDONLY);
    if (dirfd < 0) {
        perror("open failed");
        exit(EXIT_FAILURE);
    }

    // Execute the 'ls' program using execveat()
    if (execveat(dirfd, "ls", args, env, 0) == -1) {
        perror("execveat failed");
        close(dirfd);
        exit(EXIT_FAILURE);
    }

    // Close the directory (this won't be reached if execveat() is successful)
    close(dirfd);
    return 0;
}

