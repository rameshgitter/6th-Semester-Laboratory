#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#define MAX_PACKET_SIZE 1008

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <ServerPort>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int server_port = atoi(argv[1]);
    if (server_port <= 0) {
        fprintf(stderr, "Invalid port\n");
        exit(EXIT_FAILURE);
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(server_port);

    if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Server running on port %d\n", server_port);

    while (1) {
        char buffer[MAX_PACKET_SIZE];
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int recv_len = recvfrom(sockfd, buffer, MAX_PACKET_SIZE, 0, (struct sockaddr*)&client_addr, &addr_len);
        if (recv_len < 0) {
            perror("recvfrom failed");
            continue;
        }

        // Check for too small packet (EC1)
        if (recv_len < 8) {
            char error_packet[4];
            error_packet[0] = 2; // MT=2

            uint16_t sn = 0;
            if (recv_len >= 3) {
                sn = ntohs(*(uint16_t*)(buffer + 1));
            }
            uint16_t sn_network = htons(sn);
            memcpy(error_packet + 1, &sn_network, 2);
            error_packet[3] = 1; // EC1

            sendto(sockfd, error_packet, 4, 0, (struct sockaddr*)&client_addr, addr_len);
            continue;
        }

        // Parse fields
        unsigned char mt = buffer[0];
        uint16_t sn = ntohs(*(uint16_t*)(buffer + 1));
        unsigned char ttl = buffer[3];
        uint32_t pl = ntohl(*(uint32_t*)(buffer + 4));

        // Check PL too large (EC3)
        if (pl > 1000) {
            char error_packet[4];
            error_packet[0] = 2;
            uint16_t sn_network = htons(sn);
            memcpy(error_packet + 1, &sn_network, 2);
            error_packet[3] = 3; // EC3
            sendto(sockfd, error_packet, 4, 0, (struct sockaddr*)&client_addr, addr_len);
            continue;
        }

        // Check payload inconsistency (EC2)
        if (recv_len != 8 + pl) {
            char error_packet[4];
            error_packet[0] = 2;
            uint16_t sn_network = htons(sn);
            memcpy(error_packet + 1, &sn_network, 2);
            error_packet[3] = 2; // EC2
            sendto(sockfd, error_packet, 4, 0, (struct sockaddr*)&client_addr, addr_len);
            continue;
        }

        // Check TTL even (EC4)
        if (ttl % 2 != 0) {
            char error_packet[4];
            error_packet[0] = 2;
            uint16_t sn_network = htons(sn);
            memcpy(error_packet + 1, &sn_network, 2);
            error_packet[3] = 4; // EC4
            sendto(sockfd, error_packet, 4, 0, (struct sockaddr*)&client_addr, addr_len);
            continue;
        }

        // Valid packet: decrement TTL and send back
        buffer[3] = ttl - 1;
        sendto(sockfd, buffer, recv_len, 0, (struct sockaddr*)&client_addr, addr_len);
    }

    close(sockfd);
    return 0;
}
