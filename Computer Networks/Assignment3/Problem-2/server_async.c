#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Create a TCP socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Prepare the sockaddr_in structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on all network interfaces
    address.sin_port = htons(PORT);

    // Bind the socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections (backlog set to 3)
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d...\n", PORT);

    // Accept an incoming connection
    if ((client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
        perror("accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Client connected.\n");

    // Read the message from the client
    int read_bytes = read(client_fd, buffer, BUFFER_SIZE);
    if (read_bytes < 0) {
        perror("read failed");
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    buffer[read_bytes] = '\0'; // Null-terminate the string
    printf("Received message: %s\n", buffer);

    // Check if the message is "What's the time?"
    if (strcmp(buffer, "What's the time?") == 0) {
        // Get the current system time
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_str[BUFFER_SIZE];

        // Format the time string, for example: "2025-02-07 14:30:00"
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

        // Send the formatted time back to the client
        send(client_fd, time_str, strlen(time_str), 0);
        printf("Sent time: %s\n", time_str);
    } else {
        // If the message is not recognized, send an error message
        char *msg = "Invalid message.";
        send(client_fd, msg, strlen(msg), 0);
    }

    // Close the client and server sockets
    close(client_fd);
    close(server_fd);
    return 0;
}
