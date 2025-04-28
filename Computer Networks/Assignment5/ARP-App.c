#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <linux/if_ether.h>
#include <time.h>

#define ETH_FRAME_LEN 1514
#define ARP_TIMEOUT 3  // Timeout in seconds for ARP response

// ARP header structure
struct arp_header {
    unsigned short hardware_type;
    unsigned short protocol_type;
    unsigned char hardware_size;
    unsigned char protocol_size;
    unsigned short opcode;
    unsigned char sender_mac[6];
    unsigned char sender_ip[4];
    unsigned char target_mac[6];
    unsigned char target_ip[4];
};

// Global variables
int raw_sock = -1;
time_t request_time;
int request_active = 0;
struct sockaddr_ll saddr;

// Function to print MAC address
void print_mac(unsigned char *mac) {
    printf("%02X:%02X:%02X:%02X:%02X:%02X", 
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Function to get interface information
int get_interface_info(char *interface, unsigned char *mac, struct in_addr *ip) {
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (sock < 0) {
        perror("Socket creation failed");
        return -1;
    }
    
    // Get MAC address
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);
    
    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
        perror("SIOCGIFHWADDR");
        close(sock);
        return -1;
    }
    
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
    
    // Get IP address
    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
        perror("SIOCGIFADDR");
        close(sock);
        return -1;
    }
    
    *ip = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr;
    
    // Get interface index
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("SIOCGIFINDEX");
        close(sock);
        return -1;
    }
    
    saddr.sll_ifindex = ifr.ifr_ifindex;
    saddr.sll_family = AF_PACKET;
    saddr.sll_protocol = htons(ETH_P_ARP);
    
    close(sock);
    return 0;
}

// Function to send ARP request
int send_arp_request(char *interface, unsigned char *src_mac, struct in_addr src_ip, struct in_addr target_ip) {
    unsigned char buffer[ETH_FRAME_LEN];
    struct ethhdr *eth = (struct ethhdr *)buffer;
    struct arp_header *arp = (struct arp_header *)(buffer + sizeof(struct ethhdr));
    
    // Set Ethernet header
    memset(eth->h_dest, 0xFF, 6);  // Broadcast MAC
    memcpy(eth->h_source, src_mac, 6);
    eth->h_proto = htons(ETH_P_ARP);
    
    // Set ARP header
    arp->hardware_type = htons(ARPHRD_ETHER);
    arp->protocol_type = htons(ETH_P_IP);
    arp->hardware_size = 6;
    arp->protocol_size = 4;
    arp->opcode = htons(ARPOP_REQUEST);
    
    memcpy(arp->sender_mac, src_mac, 6);
    memcpy(arp->sender_ip, &src_ip.s_addr, 4);
    memset(arp->target_mac, 0, 6);
    memcpy(arp->target_ip, &target_ip.s_addr, 4);
    
    // Send packet
    if (sendto(raw_sock, buffer, sizeof(struct ethhdr) + sizeof(struct arp_header), 0, 
              (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        perror("sendto() failed");
        return -1;
    }
    
    request_time = time(NULL);
    request_active = 1;
    
    printf("ARP request sent for IP: %s\n", inet_ntoa(target_ip));
    return 0;
}

// Function to process received packets
int process_arp_packet(unsigned char *buffer, struct in_addr target_ip) {
    struct ethhdr *eth = (struct ethhdr *)buffer;
    struct arp_header *arp = (struct arp_header *)(buffer + sizeof(struct ethhdr));
    
    // Check if it's an ARP packet
    if (ntohs(eth->h_proto) != ETH_P_ARP)
        return 0;
    
    // Check if it's an ARP response
    if (ntohs(arp->opcode) != ARPOP_REPLY)
        return 0;
    
    // Check if it's the response we're waiting for
    if (memcmp(arp->sender_ip, &target_ip.s_addr, 4) != 0)
        return 0;
    
    // Found our response!
    printf("ARP Reply received from: %s\n", inet_ntoa(target_ip));
    printf("MAC Address: ");
    print_mac(arp->sender_mac);
    printf("\n");
    
    request_active = 0;
    return 1;
}

// Timeout handler
void timeout_handler(int signum) {
    if (request_active) {
        printf("ARP request timed out. No response received.\n");
        request_active = 0;
    }
}

// Main function
int main(int argc, char *argv[]) {
    char *interface;
    struct in_addr src_ip, target_ip;
    unsigned char src_mac[6];
    unsigned char buffer[ETH_FRAME_LEN];
    
    // Check arguments
    if (argc != 3) {
        printf("Usage: %s <interface> <target_ip>\n", argv[0]);
        return 1;
    }
    
    interface = argv[1];
    
    // Convert target IP from string to in_addr
    if (inet_aton(argv[2], &target_ip) == 0) {
        printf("Invalid target IP address: %s\n", argv[2]);
        return 1;
    }
    
    // Initialize raw socket
    raw_sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (raw_sock < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    // Get interface info
    if (get_interface_info(interface, src_mac, &src_ip) < 0) {
        printf("Failed to get interface information for %s\n", interface);
        close(raw_sock);
        return 1;
    }
    
    printf("Using interface: %s\n", interface);
    printf("Source MAC: ");
    print_mac(src_mac);
    printf("\n");
    printf("Source IP: %s\n", inet_ntoa(src_ip));
    printf("Target IP: %s\n", inet_ntoa(target_ip));
    
    // Set up signal handler for timeout
    signal(SIGALRM, timeout_handler);
    
    // Send ARP request
    if (send_arp_request(interface, src_mac, src_ip, target_ip) < 0) {
        close(raw_sock);
        return 1;
    }
    
    // Set timeout alarm
    alarm(ARP_TIMEOUT);
    
    // Wait for response
    while (request_active) {
        fd_set readfds;
        struct timeval tv;
        int ret;
        
        FD_ZERO(&readfds);
        FD_SET(raw_sock, &readfds);
        
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        ret = select(raw_sock + 1, &readfds, NULL, NULL, &tv);
        
        if (ret < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, probably our timeout
                continue;
            }
            perror("select() failed");
            break;
        }
        
        if (ret == 0) {
            // Timeout on select, but not our alarm yet
            continue;
        }
        
        if (FD_ISSET(raw_sock, &readfds)) {
            int recv_len = recvfrom(raw_sock, buffer, ETH_FRAME_LEN, 0, NULL, NULL);
            
            if (recv_len < 0) {
                perror("recvfrom() failed");
                break;
            }
            
            // Process the packet
            if (process_arp_packet(buffer, target_ip)) {
                // Cancel the alarm if we got our response
                alarm(0);
                break;
            }
        }
    }
    
    close(raw_sock);
    return 0;
}
