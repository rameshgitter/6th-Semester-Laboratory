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
    char *message = "What's the time?";
    char buffer[BUFFER_SIZE] = {0};

    // Create a TCP socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    // Set up the server address structure
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    /* 
       IMPORTANT:
       Replace "YOUR_HOST_IP" with the actual IP address of your host computer 
       where the server is running.
       
       For example, if your host computer's IP is 192.168.1.100, use:
       inet_pton(AF_INET, "192.168.1.100", &serv_addr.sin_addr)
    */
    if (inet_pton(AF_INET, "10.2.46.210", &serv_addr.sin_addr) <= 0) {
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

    // Send the predefined message "What's the time?" to the server
    send(sock, message, strlen(message), 0);
    printf("Message sent: %s\n", message);

    // Read the server's response (the current time)
    int valread = read(sock, buffer, BUFFER_SIZE);
    if (valread < 0) {
        perror("read failed");
    } else {
        buffer[valread] = '\0'; // Null-terminate the string
        printf("Server time: %s\n", buffer);
    }

    // Close the socket
    close(sock);
    return 0;
}
