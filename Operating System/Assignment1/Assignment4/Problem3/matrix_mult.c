#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

// Structure to hold matrix dimensions and data
struct shared_data {
    int n, m, p;           // Matrix dimensions
    int a[20][20];         // Matrix A
    int b[20][20];         // Matrix B
    int c[20][20];         // Result Matrix C
};

void read_matrix(int rows, int cols, int matrix[20][20], char name) {
    printf("Enter elements of matrix %c (%dx%d):\n", name, rows, cols);
    for(int i = 0; i < rows; i++) {
        for(int j = 0; j < cols; j++) {
            printf("%c[%d][%d]: ", name, i, j);
            scanf("%d", &matrix[i][j]);
        }
    }
}

void print_matrix(int rows, int cols, int matrix[20][20]) {
    for(int i = 0; i < rows; i++) {
        for(int j = 0; j < cols; j++) {
            printf("%d\t", matrix[i][j]);
        }
        printf("\n");
    }
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

    // Read matrix dimensions
    printf("Enter dimensions:\n");
    printf("Matrix A (n x m): ");
    scanf("%d %d", &shared->n, &shared->m);
    printf("Matrix B (m x p): ");
    scanf("%d %d", &shared->m, &shared->p);

    // Read matrices
    read_matrix(shared->n, shared->m, shared->a, 'A');
    read_matrix(shared->m, shared->p, shared->b, 'B');

    // Create n child processes
    for(int i = 0; i < shared->n; i++) {
        pid_t pid = fork();
        
        if(pid < 0) {
            perror("Fork failed");
            exit(1);
        }
        else if(pid == 0) {
            // Child process - compute one row of result
            for(int j = 0; j < shared->p; j++) {
                shared->c[i][j] = 0;
                for(int k = 0; k < shared->m; k++) {
                    shared->c[i][j] += shared->a[i][k] * shared->b[k][j];
                }
            }
            exit(0);
        }
    }

    // Parent waits for all children to complete
    for(int i = 0; i < shared->n; i++) {
        wait(NULL);
    }

    // Print result matrix
    printf("\nResultant Matrix C (%dx%d):\n", shared->n, shared->p);
    print_matrix(shared->n, shared->p, shared->c);

    // Clean up shared memory
    if(shmdt(shared) == -1) {
        perror("shmdt failed");
    }
    if(shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl failed");
    }

    return 0;
}
