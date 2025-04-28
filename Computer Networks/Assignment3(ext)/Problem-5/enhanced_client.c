#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    // Create the TCP socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Prompt the user to enter the command number
    int command;
    printf("Enter command number (1: Time, 2: Date and Time, 3: Server Name): ");
    if (scanf("%d", &command) != 1) {
        fprintf(stderr, "Invalid input.\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Convert command to network byte order and send it to the server
    int net_command = htonl(command);
    send(sock, &net_command, sizeof(net_command), 0);

    // Read the response from the server
    int read_bytes = read(sock, buffer, BUFFER_SIZE - 1);
    if (read_bytes < 0) {
        perror("read failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    buffer[read_bytes] = '\0';
    printf("Server response: %s\n", buffer);

    close(sock);
    return 0;
}

