/******************************************************************************
 * Program: Find Kth Largest Numbers Using Multiple Processes with Parent Communication
 * Description: This program reads n numbers into an array and creates n child
 *              processes. Each child process finds the ith largest number and
 *              passes it to the parent through exit status. The parent process
 *              collects these numbers and prints them in descending order.
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

/* Function to swap two integers */
void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

/* Function to find the kth largest element in an array */
int find_kth_largest(int arr[], int n, int k) {
    /* Create a copy of the array to avoid modifying original */
    int *temp = malloc(n * sizeof(int));
    if (temp == NULL) {
        perror("Memory allocation failed");
        exit(1);
    }
    
    for (int i = 0; i < n; i++) {
        temp[i] = arr[i];
    }
    
    /* Sort the array in descending order using bubble sort */
    for (int i = 0; i < n-1; i++) {
        for (int j = 0; j < n-i-1; j++) {
            if (temp[j] < temp[j+1]) {
                swap(&temp[j], &temp[j+1]);
            }
        }
    }
    
    /* Get the kth largest element */
    int result = temp[k-1];
    free(temp);
    return result;
}

int main() {
    int n;
    pid_t pid;
    
    /* Get the size of array from user */
    printf("Enter the number of elements: ");
    scanf("%d", &n);
    
    /* Input validation */
    if (n <= 0) {
        printf("Please enter a positive number\n");
        exit(1);
    }
    
    /* Allocate memory for the array and result array */
    int *arr = malloc(n * sizeof(int));
    int *result = malloc(n * sizeof(int));
    if (arr == NULL || result == NULL) {
        perror("Memory allocation failed");
        exit(1);
    }
    
    /* Read array elements */
    printf("Enter %d numbers:\n", n);
    for (int i = 0; i < n; i++) {
        scanf("%d", &arr[i]);
    }
    
    /* Create n child processes */
    for (int i = 1; i <= n; i++) {
        pid = fork();
        
        if (pid < 0) {
            /* Fork failed */
            perror("Fork failed");
            free(arr);
            free(result);
            exit(1);
        }
        else if (pid == 0) {
            /* Child process */
            int kth_largest = find_kth_largest(arr, n, i);
            free(arr);
            free(result);
            exit(kth_largest); /* Pass the result to parent through exit status */
        }
    }
    
    /* Parent process collects results from children */
    int status;
    pid_t child_pid;
    int index = 0;
    
    while ((child_pid = wait(&status)) > 0) {
        if (WIFEXITED(status)) {
            result[index++] = WEXITSTATUS(status);
        }
    }
    
    /* Print the sorted array in descending order */
    printf("\nNumbers in descending order:\n");
    for (int i = 0; i < n; i++) {
        printf("%d ", result[i]);
    }
    printf("\n");
    
    /* Clean up */
    free(arr);
    free(result);
    return 0;
}
