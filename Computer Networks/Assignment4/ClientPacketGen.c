#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

#define MAX_PACKET_SIZE 1008

const char* error_strings[] = {
    "TOO SMALL PACKET RECEIVED",
    "PAYLOAD LENGTH AND PAYLOAD INCONSISTENT",
    "TOO LARGE PAYLOAD LENGTH",
    "TTL VALUE IS NOT EVEN"
};

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <ServerIP> <ServerPort> <P> <TTL> <NumPackets>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int P = atoi(argv[3]);
    int TTL = atoi(argv[4]);
    int num_packets = atoi(argv[5]);

    // Validate inputs
    if (P < 100 || P > 1000) {
        fprintf(stderr, "P must be between 100 and 1000\n");
        exit(EXIT_FAILURE);
    }
    if (TTL < 2 || TTL > 20 || TTL % 2 != 0) {
        fprintf(stderr, "TTL must be even and between 2 and 20\n");
        exit(EXIT_FAILURE);
    }
    if (num_packets < 1 || num_packets > 50) {
        fprintf(stderr, "NumPackets must be between 1 and 50\n");
        exit(EXIT_FAILURE);
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set timeout
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        perror("inet_pton failed");
        exit(EXIT_FAILURE);
    }

    double total_rtt = 0.0;
    int received_count = 0;

    for (int i = 0; i < num_packets; i++) {
        char packet[MAX_PACKET_SIZE];
        memset(packet, 0, sizeof(packet));
        packet[0] = 1; // MT=1 (message type for request)

        uint16_t sn = i;
        uint16_t sn_network = htons(sn);
        memcpy(packet + 1, &sn_network, 2); // Copy SN in network byte order

        packet[3] = TTL;

        uint32_t pl_network = htonl(P);
        memcpy(packet + 4, &pl_network, 4); // Copy P in network byte order

        struct timeval send_time, recv_time;
        gettimeofday(&send_time, NULL);

        if (sendto(sockfd, packet, 8 + P, 0, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
            perror("sendto failed");
            continue;
        }

        char recv_buffer[MAX_PACKET_SIZE];
        socklen_t addr_len = sizeof(servaddr);
        int recv_len = recvfrom(sockfd, recv_buffer, MAX_PACKET_SIZE, 0, (struct sockaddr*)&servaddr, &addr_len);

        if (recv_len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("Timeout for packet SN %d\n", i);
            } else {
                perror("recvfrom failed");
            }
            continue;
        }

        gettimeofday(&recv_time, NULL);
        double rtt = (recv_time.tv_sec - send_time.tv_sec) * 1000.0;
        rtt += (recv_time.tv_usec - send_time.tv_usec) / 1000.0;

        if (recv_len < 1) {
            printf("Received packet too small for SN %d\n", i);
            continue;
        }

        unsigned char mt = recv_buffer[0];
        if (mt == 2) {
            if (recv_len < 4) {
                printf("Invalid error packet received for SN %d\n", i);
                continue;
            }
            uint16_t sn_error = ntohs(*(uint16_t*)(recv_buffer + 1));
            unsigned char ec = recv_buffer[3];
            if (ec < 1 || ec > 4) {
                printf("Unknown error code %d for SN %d\n", ec, sn_error);
            } else {
                printf("Error SN %d: %s\n", sn_error, error_strings[ec - 1]);
            }
        } else if (mt == 1) {
            if (recv_len < 8) {
                printf("Received packet too small for SN %d\n", i);
                continue;
            }
            uint16_t sn_received = ntohs(*(uint16_t*)(recv_buffer + 1));
            if (sn_received != i) {
                printf("Mismatched SN %d (expected %d)\n", sn_received, i);
                continue;se if (mt == 1) {

            }
            total_rtt += rtt;
            received_count++;
            printf("Packet SN %d RTT: %.3f ms\n", i, rtt);
        } else {
            printf("Unknown message type %d for SN %d\n", mt, i);
        }
    }

    if (received_count > 0) {
        printf("Average RTT: %.3f ms\n", total_rtt / received_count);
    } else {
        printf("No packets received\n");
    }

    close(sockfd);
    return 0;
}
