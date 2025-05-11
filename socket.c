#include "socket.h"

int create_raw_socket(){
    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock == -1){
        fprintf(stderr, "Error creating socket\n");
        exit(-1);
    }

    return sock;
}

int bind_raw_socket(int sock, char* interface, int port){
    int ifindex = if_nametoindex(interface);

    struct sockaddr_ll address = {0};
    address.sll_family = AF_PACKET;
    address.sll_protocol = htons(ETH_P_ALL);
    address.sll_ifindex = ifindex;

    if(bind(sock, (struct sockaddr*)&address, sizeof(address)) == -1){
        fprintf(stderr, "Error binding socket to interface\n");
        exit(-1);
    }

    struct packet_mreq mr = {0};
    mr.mr_ifindex = ifindex;
    mr.mr_type = PACKET_MR_PROMISC;
    if (setsockopt(sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1){
        fprintf(stderr, "Error setting setsockopt\n"); 
        exit(-1);
    }

    return sock;
}

void connect_raw_socket(int sock, char* interface, unsigned char dest_mac[6]) {
    struct sockaddr_ll addr = {0};
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex = if_nametoindex(interface);
    addr.sll_halen = ETH_ALEN;
    memcpy(addr.sll_addr, dest_mac, ETH_ALEN);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        fprintf(stderr, "connect() failed: %s (errno=%d)\n", strerror(errno), errno);
        exit(-1);
    }
}

void send_package(int sock, char* interface, unsigned char dest_mac[6], unsigned char* message){
    struct sockaddr_ll dest_addr = {0};
    dest_addr.sll_family = AF_PACKET;
    dest_addr.sll_protocol = htons(ETH_P_ALL);
    dest_addr.sll_ifindex = if_nametoindex(interface);
    dest_addr.sll_halen = ETH_ALEN;
    memcpy(dest_addr.sll_addr, dest_mac, ETH_ALEN);

    ssize_t sent = sendto(sock, message, strlen(message), 0,
                        (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (sent == -1) {
        perror("sendto failed");
        exit(-1);
    }
    printf("Sent %zd bytes\n", sent);
}

void receive_package(int sock, unsigned char* buffer, struct sockaddr_ll* sender_addr, socklen_t* addr_len){
    ssize_t bytes = recvfrom(sock, buffer, 4096, 0,
                            (struct sockaddr*)sender_addr, addr_len);
    if (bytes == -1) {
        perror("recvfrom failed");
        exit(-1);
    }
    printf("Received %zd bytes\n", bytes);
}