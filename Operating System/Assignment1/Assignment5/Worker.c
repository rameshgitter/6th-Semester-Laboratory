#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

struct task {
    char data[100];
    pid_t worker_pid;
    int status;
};

int main() {
    key_t key = ftok(".", 'S');
    if (key == -1) {
        perror("ftok");
        exit(1);
    }

    int shmid = shmget(key, sizeof(struct task), 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    struct task *solve = (struct task *)shmat(shmid, NULL, 0);
    if (solve == (struct task *)-1) {
        perror("shmat");
        exit(1);
    }

    printf("Worker started (PID: %d)\n", getpid());

    while (1) {
        // Check if server has terminated
        if (solve->status == -1) {
            printf("Worker %d: Server terminated, exiting...\n", getpid());
            break;
        }

        // Wait for work
        if (solve->status == 1) {
            // Claim the task
            solve->status = 2;
            solve->worker_pid = getpid();
            
            printf("Worker %d: Processing string: %s\n", getpid(), solve->data);
            
            // Compute string length
            int length = strlen(solve->data);
            
            // Convert length to string and store it back
            sprintf(solve->data, "%d", length);
            
            printf("Worker %d: Computed length: %s\n", getpid(), solve->data);
            
            // Mark as complete
            solve->status = 3;
        }
        
        usleep(100000); // Sleep for 100ms to reduce CPU usage
    }

    return 0;
}
