#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <time.h>

struct task {
    char data[100];
    pid_t worker_pid;
    int status;
};

struct task *solve;
int shmid;

void cleanup(int signo) {
    solve->status = -1;
    printf("\nServer shutting down...\n");
    exit(0);
}

// Function to generate random string
void generate_random_string(char *str, int length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < length; i++) {
        str[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    str[length] = '\0';
}

int main() {
    key_t key = ftok(".", 'S');
    if (key == -1) {
        perror("ftok");
        exit(1);
    }

    // Try to create new shared memory
    shmid = shmget(key, sizeof(struct task), IPC_CREAT | IPC_EXCL | 0666);
    if (shmid == -1) {
        // If already exists, get the existing one
        shmid = shmget(key, sizeof(struct task), 0666);
        if (shmid == -1) {
            perror("shmget");
            exit(1);
        }
    }

    solve = (struct task *)shmat(shmid, NULL, 0);
    if (solve == (struct task *)-1) {
        perror("shmat");
        exit(1);
    }

    // Initialize shared memory
    solve->status = 0;
    
    // Set up signal handler for Ctrl+C
    signal(SIGINT, cleanup);
    
    // Seed random number generator
    srand(time(NULL));

    printf("Server started. Press Ctrl+C to terminate.\n");

    while (1) {
        // Generate random string length between 5 and 20
        int len = rand() % 16 + 5;
        generate_random_string(solve->data, len);
        
        printf("Server: Generated string: %s\n", solve->data);
        solve->status = 1;
        
        printf("Server: Waiting for worker to process...\n");
        while (solve->status != 3) {
            usleep(100000); // Sleep for 100ms to reduce CPU usage
        }
        
        solve->status = 4;
        printf("Server: String length computed by worker (PID: %d): %s\n", 
               solve->worker_pid, solve->data);
        
        solve->status = 0;
        sleep(2); // Wait before generating next string
    }

    return 0;
}
