#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_NAME "MyServer"

// Original Problem‑2 server code is kept unchanged in a separate file if needed.

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    char response[BUFFER_SIZE] = {0};

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

    // Read the command from the client (an integer)
    int command;
    int bytes_read = recv(client_fd, &command, sizeof(command), 0);
    if (bytes_read != sizeof(command)) {
        perror("failed to read command");
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    // Convert command from network byte order to host order
    command = ntohl(command);

    // Process the command based on its value
    time_t now;
    struct tm *tm_info;
    switch(command) {
        case 1:
            // Query for the time (only time in HH:MM:SS)
            now = time(NULL);
            tm_info = localtime(&now);
            strftime(response, sizeof(response), "%H:%M:%S", tm_info);
            break;
        case 2:
            // Query for date and time (YYYY-MM-DD HH:MM:SS)
            now = time(NULL);
            tm_info = localtime(&now);
            strftime(response, sizeof(response), "%Y-%m-%d %H:%M:%S", tm_info);
            break;
        case 3:
            // Query for the server name
            strncpy(response, SERVER_NAME, sizeof(response) - 1);
            response[sizeof(response) - 1] = '\0';
            break;
        default:
            // Unsupported command
            strncpy(response, "Unsupported command.", sizeof(response) - 1);
            response[sizeof(response) - 1] = '\0';
            break;
    }

    // Send the response back to the client
    send(client_fd, response, strlen(response), 0);
    printf("Sent response: %s\n", response);

    // Close the client and server sockets
    close(client_fd);
    close(server_fd);
    return 0;
}

