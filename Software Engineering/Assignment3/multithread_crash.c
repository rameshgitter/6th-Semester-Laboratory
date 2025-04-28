// multithread_crash.c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

void* thread_func(void* arg) {
    int id = *(int*)arg;
    printf("Thread %d started.\n", id);
    
    // Introduce a deliberate segmentation fault in thread 2.
    if (id == 2) {
        int *ptr = NULL;
        *ptr = 42;  // This will cause a segmentation fault.
    }
    
    // Sleep to simulate work
    sleep(2);
    printf("Thread %d ending.\n", id);
    return NULL;
}

int main() {
    pthread_t threads[3];
    int thread_ids[3] = {1, 2, 3};

    // Create three threads.
    for (int i = 0; i < 3; i++) {
        if (pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    // Wait for all threads to finish.
    for (int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }
    
    return 0;
}

