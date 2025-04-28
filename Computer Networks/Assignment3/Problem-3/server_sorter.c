#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 4096

int compare_int(const void *a, const void *b) {
    return (*(int32_t *)a - *(int32_t *)b);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
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
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    printf("Client connected.\n");

    uint16_t num_elements;
    int bytes_received = recv(new_socket, &num_elements, 2, 0);
    if (bytes_received != 2) {
        perror("failed to read element count");
        close(new_socket);
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    num_elements = ntohs(num_elements);

    int32_t *elements = malloc(num_elements * sizeof(int32_t));
    if (!elements) {
        perror("malloc failed");
        close(new_socket);
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    int total_bytes = num_elements * sizeof(int32_t);
    int bytes_read = 0;
    char *buffer = (char *)elements;

    while (bytes_read < total_bytes) {
        int r = recv(new_socket, buffer + bytes_read, total_bytes - bytes_read, 0);
        if (r <= 0) {
            perror("failed to read elements");
            free(elements);
            close(new_socket);
            close(server_fd);
            exit(EXIT_FAILURE);
        }
        bytes_read += r;
    }

    for (int i = 0; i < num_elements; i++) {
        elements[i] = ntohl(elements[i]);
    }

    qsort(elements, num_elements, sizeof(int32_t), compare_int);

    for (int i = 0; i < num_elements; i++) {
        elements[i] = htonl(elements[i]);
    }

    uint16_t net_num = htons(num_elements);
    send(new_socket, &net_num, sizeof(net_num), 0);
    send(new_socket, elements, total_bytes, 0);

    free(elements);
    printf("Sorted elements sent to client.\n");
    close(new_socket);
    close(server_fd);
    return 0;
}
