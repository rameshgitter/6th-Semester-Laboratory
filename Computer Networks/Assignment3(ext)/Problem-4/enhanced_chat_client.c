#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

// Global flag shared by both threads
volatile int running = 1;
int sock;

void *send_thread(void *arg) {
    char buffer[BUFFER_SIZE];
    while (running) {
        printf("Enter your message: ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            perror("fgets error");
            break;
        }
        buffer[strcspn(buffer, "\n")] = '\0';  // Remove newline

        if (send(sock, buffer, strlen(buffer), 0) < 0) {
            perror("send error");
            break;
        }

        // If the user types "Bye", signal termination
        if (strcmp(buffer, "Bye") == 0) {
            running = 0;
            break;
        }
    }
    return NULL;
}

void *recv_thread(void *arg) {
    char buffer[BUFFER_SIZE];
    int bytes_read;
    while (running) {
        bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read <= 0) {
            running = 0;
            break;
        }
        buffer[bytes_read] = '\0';
        printf("\nServer: %s\n", buffer);

        // If the peer sends "Bye", signal termination
        if (strcmp(buffer, "Bye") == 0) {
            running = 0;
            break;
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in serv_addr;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("invalid address");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }
    printf("Connected to server.\n");

    // Create threads for sending and receiving messages concurrently
    pthread_t send_tid, recv_tid;
    if (pthread_create(&send_tid, NULL, send_thread, NULL) != 0) {
        perror("pthread_create (send_thread) failed");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&recv_tid, NULL, recv_thread, NULL) != 0) {
        perror("pthread_create (recv_thread) failed");
        exit(EXIT_FAILURE);
    }

    // Wait for both threads to finish
    pthread_join(send_tid, NULL);
    pthread_join(recv_tid, NULL);

    printf("Closing connection...\n");
    close(sock);
    return 0;
}

