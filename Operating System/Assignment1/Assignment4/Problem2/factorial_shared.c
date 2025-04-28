/*
 * Shared Memory Communication between Parent and Child
 * Parent puts random numbers, Child calculates factorial
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <signal.h>

// Structure to hold shared data
struct shared_data {
    long number;        // The number to calculate factorial of
    long factorial;     // Result of factorial calculation
    int new_number;     // Flag to indicate new number from parent
    int new_factorial;  // Flag to indicate new factorial from child
};

// Function to calculate factorial
long calculate_factorial(long n) {
    if (n <= 1) return 1;
    return n * calculate_factorial(n - 1);
}

int main() {
    // Create shared memory
    key_t key = ftok(".", 'a');
    int shmid = shmget(key, sizeof(struct shared_data), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }

    // Attach shared memory
    struct shared_data *shared = (struct shared_data *)shmat(shmid, NULL, 0);
    if (shared == (void *)-1) {
        perror("shmat failed");
        exit(1);
    }

    // Initialize shared memory
    shared->new_number = 0;
    shared->new_factorial = 0;

    // Seed random number generator
    srand(time(NULL));

    // Fork process
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }
    else if (pid == 0) {
        // Child process - calculates factorial
        printf("Child process started\n");
        
        while(1) {
            if (shared->new_number) {
                long num = shared->number;
                printf("Child: Calculating factorial of %ld\n", num);
                
                shared->factorial = calculate_factorial(num);
                shared->new_factorial = 1;
                shared->new_number = 0;
                
                printf("Child: Factorial calculated: %ld\n", shared->factorial);
                sleep(1);  // Slow down child process
            }
            usleep(100000);  // Small delay to prevent busy waiting
        }
    }
    else {
        // Parent process - generates random numbers
        printf("Parent process started\n");
        
        // Handle Ctrl+C for clean shutdown
        for(int i = 0; i < 10; i++) {  // Run for 10 iterations
            if (!shared->new_number) {
                long random_num = (rand() % 10) + 1;  // Random number between 1 and 10
                shared->number = random_num;
                shared->new_number = 1;
                
                printf("\nParent: Generated number: %ld\n", random_num);
                
                // Wait for factorial
                while (!shared->new_factorial) {
                    usleep(100000);  // Small delay
                }
                
                printf("Parent: Received factorial: %ld\n", shared->factorial);
                shared->new_factorial = 0;
            }
            sleep(2);  // Wait before generating next number
        }

        // Clean up
        printf("\nParent: Cleaning up...\n");
        if (shmdt(shared) == -1) {
            perror("shmdt failed");
        }
        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
            perror("shmctl failed");
        }
        printf("Parent: Shared memory released\n");
        
        // Kill child process
        kill(pid, SIGTERM);
        printf("Parent: Child process terminated\n");
        exit(0);
    }

    return 0;
}
