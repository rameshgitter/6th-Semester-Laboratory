#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/stat.h>

// Define union semun if not defined by the system.
union semun {
    int val;                    // value for SETVAL
    struct semid_ds *buf;       // buffer for IPC_STAT, IPC_SET
    unsigned short *array;      // array for GETALL, SETALL
#if defined(__linux__)
    struct seminfo *__buf;      // buffer for IPC_INFO (Linux specific)
#endif
};

// Function to print usage information.
void print_usage(const char *progname) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <file_name> <project_id> create <semnum>\n", progname);
    fprintf(stderr, "  %s <file_name> <project_id> set <semnum> <sem_val>\n", progname);
    fprintf(stderr, "  %s <file_name> <project_id> get [<semnum>]\n", progname);
    fprintf(stderr, "  %s <file_name> <project_id> inc <semnum> <val>\n", progname);
    fprintf(stderr, "  %s <file_name> <project_id> dcr <semnum> <val>\n", progname);
    fprintf(stderr, "  %s <file_name> <project_id> rm\n", progname);
    fprintf(stderr, "  %s <file_name> <project_id> listp [<semnum>]\n", progname);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    char *file_name = argv[1];
    int proj_id = atoi(argv[2]);
    char *command = argv[3];

    // Generate a unique key using ftok.
    key_t key = ftok(file_name, proj_id);
    if (key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    int semid;  // Semaphore set identifier.

    if (strcmp(command, "create") == 0) {
        if (argc != 5) {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
        int num_sems = atoi(argv[4]);
        if (num_sems <= 0) {
            fprintf(stderr, "Invalid number of semaphores: %d\n", num_sems);
            exit(EXIT_FAILURE);
        }
        // Create a new semaphore set with the specified number of semaphores.
        semid = semget(key, num_sems, IPC_CREAT | IPC_EXCL | 0777);
        if (semid == -1) {
            perror("semget (create)");
            exit(EXIT_FAILURE);
        }
        printf("Semaphore set created successfully.\n");
        printf("Key: %d, SemID: %d, Number of semaphores: %d\n", key, semid, num_sems);
    }
    else if (strcmp(command, "set") == 0) {
        if (argc != 6) {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
        int sem_index = atoi(argv[4]);
        int sem_val = atoi(argv[5]);

        // Get the existing semaphore set.
        semid = semget(key, 0, 0);
        if (semid == -1) {
            perror("semget (set)");
            exit(EXIT_FAILURE);
        }
        union semun arg;
        arg.val = sem_val;
        if (semctl(semid, sem_index, SETVAL, arg) == -1) {
            perror("semctl (set)");
            exit(EXIT_FAILURE);
        }
        printf("Semaphore %d value set to %d.\n", sem_index, sem_val);
    }
    else if (strcmp(command, "get") == 0) {
        // Get the existing semaphore set.
        semid = semget(key, 0, 0);
        if (semid == -1) {
            perror("semget (get)");
            exit(EXIT_FAILURE);
        }
        // If a semaphore number is specified, print its value; otherwise, print values for all semaphores.
        if (argc == 5) {
            int sem_index = atoi(argv[4]);
            int val = semctl(semid, sem_index, GETVAL);
            if (val == -1) {
                perror("semctl (get)");
                exit(EXIT_FAILURE);
            }
            printf("Semaphore %d value: %d\n", sem_index, val);
        } else if (argc == 4) {
            struct semid_ds sem_ds;
            union semun arg;
            arg.buf = &sem_ds;
            if (semctl(semid, 0, IPC_STAT, arg) == -1) {
                perror("semctl (IPC_STAT)");
                exit(EXIT_FAILURE);
            }
            int nsems = sem_ds.sem_nsems;
            for (int i = 0; i < nsems; i++) {
                int val = semctl(semid, i, GETVAL);
                if (val == -1) {
                    perror("semctl (get all)");
                    exit(EXIT_FAILURE);
                }
                printf("Semaphore %d value: %d\n", i, val);
            }
        } else {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(command, "inc") == 0) {
        if (argc != 6) {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
        int sem_index = atoi(argv[4]);
        int inc_val = atoi(argv[5]);

        semid = semget(key, 0, 0);
        if (semid == -1) {
            perror("semget (inc)");
            exit(EXIT_FAILURE);
        }
        struct sembuf op;
        op.sem_num = sem_index;
        op.sem_op = inc_val;  // Positive value increments the semaphore.
        op.sem_flg = SEM_UNDO;
        if (semop(semid, &op, 1) == -1) {
            perror("semop (inc)");
            exit(EXIT_FAILURE);
        }
        printf("Incremented semaphore %d by %d.\n", sem_index, inc_val);
    }
    else if (strcmp(command, "dcr") == 0) {
        if (argc != 6) {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
        int sem_index = atoi(argv[4]);
        int dcr_val = atoi(argv[5]);

        semid = semget(key, 0, 0);
        if (semid == -1) {
            perror("semget (dcr)");
            exit(EXIT_FAILURE);
        }
        struct sembuf op;
        op.sem_num = sem_index;
        op.sem_op = -dcr_val;  // Negative value decrements the semaphore.
        op.sem_flg = SEM_UNDO;
        if (semop(semid, &op, 1) == -1) {
            perror("semop (dcr)");
            exit(EXIT_FAILURE);
        }
        printf("Decremented semaphore %d by %d.\n", sem_index, dcr_val);
    }
    else if (strcmp(command, "rm") == 0) {
        if (argc != 4) {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
        semid = semget(key, 0, 0);
        if (semid == -1) {
            perror("semget (rm)");
            exit(EXIT_FAILURE);
        }
        if (semctl(semid, 0, IPC_RMID) == -1) {
            perror("semctl (rm)");
            exit(EXIT_FAILURE);
        }
        printf("Semaphore set removed successfully.\n");
    }
    else if (strcmp(command, "listp") == 0) {
        // Note: Listing all waiting process IDs is not directly supported by System V semaphores.
        // Here we display the PID of the last process that performed an operation (GETPID)
        // as well as the counts of processes waiting (GETNCNT) and waiting for zero (GETZCNT).
        semid = semget(key, 0, 0);
        if (semid == -1) {
            perror("semget (listp)");
            exit(EXIT_FAILURE);
        }
        if (argc == 5) {
            int sem_index = atoi(argv[4]);
            int last_pid = semctl(semid, sem_index, GETPID);
            int ncnt = semctl(semid, sem_index, GETNCNT);
            int zcnt = semctl(semid, sem_index, GETZCNT);
            if (last_pid == -1) {
                perror("semctl (listp)");
                exit(EXIT_FAILURE);
            }
            printf("Semaphore %d:\n", sem_index);
            printf("  Last operation performed by PID: %d\n", last_pid);
            printf("  Number of processes waiting (GETNCNT): %d\n", ncnt);
            printf("  Number of processes waiting for zero (GETZCNT): %d\n", zcnt);
        } else if (argc == 4) {
            struct semid_ds sem_ds;
            union semun arg;
            arg.buf = &sem_ds;
            if (semctl(semid, 0, IPC_STAT, arg) == -1) {
                perror("semctl (IPC_STAT)");
                exit(EXIT_FAILURE);
            }
            int nsems = sem_ds.sem_nsems;
            for (int i = 0; i < nsems; i++) {
                int last_pid = semctl(semid, i, GETPID);
                int ncnt = semctl(semid, i, GETNCNT);
                int zcnt = semctl(semid, i, GETZCNT);
                if (last_pid == -1) {
                    perror("semctl (listp all)");
                    exit(EXIT_FAILURE);
                }
                printf("Semaphore %d:\n", i);
                printf("  Last operation performed by PID: %d\n", last_pid);
                printf("  Number of processes waiting (GETNCNT): %d\n", ncnt);
                printf("  Number of processes waiting for zero (GETZCNT): %d\n", zcnt);
            }
        } else {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", command);
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }
    return 0;
}
