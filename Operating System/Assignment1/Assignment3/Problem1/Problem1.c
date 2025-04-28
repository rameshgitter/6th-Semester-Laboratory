/******************************************************************************
 * Program: String Reversal using Multiple Processes
 * Description: This program takes n strings as command line arguments and creates
 *              n child processes. Each child process reverses one string and
 *              prints the result.
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* Function to reverse a string */
void reverse_string(char *str) {
    int length = strlen(str);
    int i, j;
    char temp;
    
    for (i = 0, j = length - 1; i < j; i++, j--) {
        temp = str[i];
        str[i] = str[j];
        str[j] = temp;
    }
}

int main(int argc, char *argv[]) {
    pid_t pid;
    int i;
    
    /* Check if at least one string argument is provided */
    if (argc < 2) {
        printf("Usage: %s string1 string2 ... stringn\n", argv[0]);
        exit(1);
    }
    
    /* Create n child processes, where n is the number of strings */
    for (i = 1; i < argc; i++) {
        pid = fork();
        
        if (pid < 0) {
            /* Fork failed */
            perror("Fork failed");
            exit(1);
        }
        else if (pid == 0) {
            /* Child process */
            char *str = strdup(argv[i]);  // Create a copy of the string
            
            if (str == NULL) {
                perror("Memory allocation failed");
                exit(1);
            }
            
            reverse_string(str);
            printf("Child process %d reversed string %d: %s\n", 
                   getpid(), i, str);
            
            free(str);  // Free allocated memory
            exit(0);
        }
    }
    
    /* Parent process waits for all child processes to complete */
    for (i = 1; i < argc; i++) {
        wait(NULL);
    }
    
    return 0;
}
