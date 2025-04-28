/*
 * forkNexec1.c - A program that reads and prints executable filenames in an infinite loop
 * The program can be terminated using Ctrl+C
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILENAME 256

int main() {
    char filename[MAX_FILENAME];
    
    printf("Enter executable filenames (Ctrl+C to quit):\n");
    
    while (1) {
        printf("\nEnter filename: ");
        if (fgets(filename, MAX_FILENAME, stdin) == NULL) {
            break;
        }
        
        // Remove trailing newline if present
        filename[strcspn(filename, "\n")] = 0;
        
        printf("You entered: %s\n", filename);
    }
    
    return 0;
}
