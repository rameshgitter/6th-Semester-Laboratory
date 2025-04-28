#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <string.h>

// Define the semun union if not already defined
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <file_name> <project_id> [create|set|get|inc|dcr|rm|listp] ...\n", argv[0]);
        exit(1);
    }

    key_t key;
    int proj_id = atoi(argv[2]);
    key = ftok(argv[1], proj_id);
    if (key == -1) {
        perror("ftok");
        exit(1);
    }

    char *cmd = argv[3];
    int semid;

    if (strcmp(cmd, "create") == 0) {
        if (argc != 5) {
            fprintf(stderr, "Usage: %s <file> <proj> create <semnum>\n", argv[0]);
            exit(1);
        }
        int semnum = atoi(argv[4]);
        semid = semget(key, semnum, IPC_CREAT | IPC_EXCL | 0666);
        if (semid == -1) {
            perror("semget create");
            exit(1);
        }
        printf("Semaphore set created with ID %d and %d semaphores\n", semid, semnum);
    } else if (strcmp(cmd, "rm") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage: %s <file> <proj> rm\n", argv[0]);
            exit(1);
        }
        semid = semget(key, 0, 0);
        if (semid == -1) {
            perror("semget rm");
            exit(1);
        }
        if (semctl(semid, 0, IPC_RMID, 0) == -1) {
            perror("semctl IPC_RMID");
            exit(1);
        }
        printf("Semaphore set removed\n");
    } else {
        semid = semget(key, 0, 0);
        if (semid == -1) {
            perror("semget");
            exit(1);
        }

        if (strcmp(cmd, "set") == 0) {
            if (argc != 6) {
                fprintf(stderr, "Usage: %s <file> <proj> set <semnum> <sem_val>\n", argv[0]);
                exit(1);
            }
            int semnum = atoi(argv[4]);
            int sem_val = atoi(argv[5]);
            union semun arg;
            arg.val = sem_val;
            if (semctl(semid, semnum, SETVAL, arg) == -1) {
                perror("semctl SETVAL");
                exit(1);
            }
            printf("Semaphore %d set to %d\n", semnum, sem_val);
        } else if (strcmp(cmd, "get") == 0) {
            struct semid_ds ds;
            union semun arg;
            arg.buf = &ds;
            if (semctl(semid, 0, IPC_STAT, arg) == -1) {
                perror("semctl IPC_STAT");
                exit(1);
            }
            int nsems = ds.sem_nsems;
            if (argc == 5) {
                int semnum = atoi(argv[4]);
                if (semnum < 0 || semnum >= nsems) {
                    fprintf(stderr, "Invalid semnum\n");
                    exit(1);
                }
                int val = semctl(semid, semnum, GETVAL);
                if (val == -1) {
                    perror("semctl GETVAL");
                    exit(1);
                }
                printf("Semaphore %d: %d\n", semnum, val);
            } else {
                for (int i = 0; i < nsems; i++) {
                    int val = semctl(semid, i, GETVAL);
                    if (val == -1) {
                        perror("semctl GETVAL");
                        exit(1);
                    }
                    printf("Semaphore %d: %d\n", i, val);
                }
            }
        } else if (strcmp(cmd, "inc") == 0 || strcmp(cmd, "dcr") == 0) {
            if (argc != 6) {
                fprintf(stderr, "Usage: %s <file> <proj> %s <semnum> <val>\n", argv[0], cmd);
                exit(1);
            }
            int semnum = atoi(argv[4]);
            int val = atoi(argv[5]);
            struct sembuf sop;
            sop.sem_num = semnum;
            sop.sem_op = (strcmp(cmd, "inc") == 0) ? val : -val;
            sop.sem_flg = SEM_UNDO;
            if (semop(semid, &sop, 1) == -1) {
                perror("semop");
                exit(1);
            }
            int new_val = semctl(semid, semnum, GETVAL);
            if (new_val == -1) {
                perror("semctl GETVAL after op");
                exit(1);
            }
            printf("%s semaphore %d by %d. New value: %d\n",
                   (strcmp(cmd, "inc") == 0) ? "Incremented" : "Decremented",
                   semnum, val, new_val);
        } else if (strcmp(cmd, "listp") == 0) {
            struct semid_ds ds;
            union semun arg;
            arg.buf = &ds;
            if (semctl(semid, 0, IPC_STAT, arg) == -1) {
                perror("semctl IPC_STAT");
                exit(1);
            }
            int nsems = ds.sem_nsems;
            if (argc == 5) {
                int semnum = atoi(argv[4]);
                if (semnum < 0 || semnum >= nsems) {
                    fprintf(stderr, "Invalid semnum\n");
                    exit(1);
                }
                pid_t pid = semctl(semid, semnum, GETPID);
                if (pid == -1) {
                    perror("semctl GETPID");
                    exit(1);
                }
                printf("Semaphore %d: Last process PID = %d\n", semnum, (int)pid);
            } else {
                for (int i = 0; i < nsems; i++) {
                    pid_t pid = semctl(semid, i, GETPID);
                    if (pid == -1) {
                        perror("semctl GETPID");
                        exit(1);
                    }
                    printf("Semaphore %d: Last process PID = %d\n", i, (int)pid);
                }
            }
        } else {
            fprintf(stderr, "Unknown command: %s\n", cmd);
            exit(1);
        }
    }

    return 0;
}
