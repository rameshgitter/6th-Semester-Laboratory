#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 4096

int compare_int(const void *a, const void *b) {
    return (*(int32_t *)a - *(int32_t *)b);
}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);  // Free the allocated memory for the socket descriptor

    uint16_t num_elements;
    int bytes_received = recv(client_socket, &num_elements, sizeof(num_elements), 0);
    if (bytes_received != sizeof(num_elements)) {
        perror("failed to read element count");
        close(client_socket);
        return NULL;
    }
    num_elements = ntohs(num_elements);

    int32_t *elements = malloc(num_elements * sizeof(int32_t));
    if (!elements) {
        perror("malloc failed");
        close(client_socket);
        return NULL;
    }

    int total_bytes = num_elements * sizeof(int32_t);
    int bytes_read = 0;
    char *buffer = (char *)elements;
    while (bytes_read < total_bytes) {
        int r = recv(client_socket, buffer + bytes_read, total_bytes - bytes_read, 0);
        if (r <= 0) {
            perror("failed to read elements");
            free(elements);
            close(client_socket);
            return NULL;
        }
        bytes_read += r;
    }

    // Convert received numbers from network to host order
    for (int i = 0; i < num_elements; i++) {
        elements[i] = ntohl(elements[i]);
    }

    qsort(elements, num_elements, sizeof(int32_t), compare_int);

    // Convert the sorted numbers back to network order
    for (int i = 0; i < num_elements; i++) {
        elements[i] = htonl(elements[i]);
    }

    uint16_t net_num = htons(num_elements);
    send(client_socket, &net_num, sizeof(net_num), 0);
    send(client_socket, elements, total_bytes, 0);

    free(elements);
    printf("Sorted elements sent to client.\n");
    close(client_socket);
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
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Enable address reuse to avoid "Address already in use" errors
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
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

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);

    while (1) {
        // Allocate memory for the client socket descriptor pointer
        int *new_socket = malloc(sizeof(int));
        if (!new_socket) {
            perror("malloc failed");
            continue;
        }

        *new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (*new_socket < 0) {
            perror("accept");
            free(new_socket);
            continue;
        }

        printf("Client connected from %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, new_socket) != 0) {
            perror("pthread_create failed");
            close(*new_socket);
            free(new_socket);
        } else {
            // Detach the thread so that resources are freed when it finishes
            pthread_detach(tid);
        }
    }

    close(server_fd);
    return 0;
}

