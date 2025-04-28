#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main() {
    char *args[] = {"/bin/ls", "-l", NULL};  // Arguments for the program
    char *env[] = {NULL};                   // Environment variables

    printf("Before execve()\n");

    // Replace the current process with 'ls -l'
    if (execve("/bin/ls", args, env) == -1) {
        perror("execve failed");
        exit(EXIT_FAILURE);
    }

    // This line will not be executed if execve() is successful
    printf("This will not print\n");

    return 0;
}

