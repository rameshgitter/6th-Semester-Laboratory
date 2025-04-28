#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFFER_SIZE 1024

// Global flag shared by both threads
volatile int running = 1;
int client_socket;

void *send_thread(void *arg) {
    char buffer[BUFFER_SIZE];
    while (running) {
        printf("Enter your message: ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            perror("fgets error");
            break;
        }
        buffer[strcspn(buffer, "\n")] = '\0';  // Remove newline

        if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
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
        bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read <= 0) {
            running = 0;
            break;
        }
        buffer[bytes_read] = '\0';
        printf("\nClient: %s\n", buffer);

        // If the peer sends "Bye", signal termination
        if (strcmp(buffer, "Bye") == 0) {
            running = 0;
            break;
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    printf("Server is listening on port %d...\n", port);

    // Accept an incoming connection
    if ((client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }
    printf("Client connected.\n");

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
    close(client_socket);
    close(server_fd);
    return 0;
}

