/******************************************************************************
 * Program: Find Kth Largest Numbers Using Multiple Processes
 * Description: This program reads n numbers into an array and creates n child
 *              processes. Each child process finds and prints the ith largest
 *              number from the array (where i is the process number).
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
    
    /* Allocate memory for the array */
    int *arr = malloc(n * sizeof(int));
    if (arr == NULL) {
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
            exit(1);
        }
        else if (pid == 0) {
            /* Child process */
            int kth_largest = find_kth_largest(arr, n, i);
            printf("Child process %d found the %d%s largest number: %d\n",
                   getpid(), i,
                   (i == 1) ? "st" : (i == 2) ? "nd" : (i == 3) ? "rd" : "th",
                   kth_largest);
            
            free(arr);
            exit(0);
        }
    }
    
    /* Parent process waits for all child processes to complete */
    for (int i = 0; i < n; i++) {
        wait(NULL);
    }
    
    free(arr);
    return 0;
}
