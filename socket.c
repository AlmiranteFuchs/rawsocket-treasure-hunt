#include "socket.h"

int create_raw_socket(){
    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock == -1){
        fprintf(stderr, "Error creating socket\n");
        exit(-1);
    }

    return sock;
}
/*
void destroy_raw_socket(int sock){
    if (close(sock) == -1){
        fprintf(stderr, "Error closing socket\n");
        exit(-1);
    }
}
*/

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

void send_package(int sock, char* interface, unsigned char dest_mac[6], const unsigned char* message, size_t message_len){
    struct sockaddr_ll dest_addr = {0};
    dest_addr.sll_family = AF_PACKET;
    dest_addr.sll_protocol = htons(ETH_P_ALL);
    dest_addr.sll_ifindex = if_nametoindex(interface);
    dest_addr.sll_halen = ETH_ALEN;
    memcpy(dest_addr.sll_addr, dest_mac, ETH_ALEN);

    ssize_t sent = sendto(sock, message, message_len, 0,
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

kermit_protocol_header* create_header(unsigned char size[SIZE_SIZE], unsigned char sequence[SEQUENCE_SIZE], unsigned char type[TYPE_SIZE], unsigned char* data){
    kermit_protocol_header* header = (kermit_protocol_header*) malloc(sizeof(kermit_protocol_header));
    if (header == NULL)
        return NULL;

    memcpy(header->size, size, SIZE_SIZE);
    sprintf((char*) header->start, "%d", START);
    memcpy(header->sequence, sequence, SEQUENCE_SIZE);
    memcpy(header->type, type, TYPE_SIZE);

    if (data != NULL){
        header->data = (unsigned char*) malloc((atoi((const char*) header->size)) * 8);
        if (header->data == NULL){
            free(header);
            return NULL;
        }
        memcpy(header->data, data, (atoi((const char *)header->size)) * 8);
    } else 
        header->data = NULL;
    
    // create checksum function later
    memset(header->checksum, 0, sizeof(header->checksum));

    return header;
}

unsigned int getHeaderSize(kermit_protocol_header* header) {
    if (header == NULL) return 0;

    unsigned int size = 0;
    size += sizeof(header->start);
    size += sizeof(header->size);
    size += sizeof(header->sequence);
    size += sizeof(header->type);
    size += sizeof(header->checksum);
    if (header->data != NULL) {
        size += atoi((const char*) header->size) * 8;
    }
    return size;
}

void destroy_header(kermit_protocol_header* header){
    if (header != NULL){
        free(header->data);
        free(header);
    }
}

const unsigned char* generate_message(kermit_protocol_header* header) {
    if (!header) return NULL;

    size_t total_size = sizeof(header->start) + sizeof(header->size) + 
                       sizeof(header->sequence) + sizeof(header->type) + 
                       sizeof(header->checksum);
    
    if (header->data) {
        total_size += atoi((const char*)header->size) * 8;
    }

    unsigned char* message = malloc(total_size);
    if (!message) return NULL;

    unsigned char* ptr = message;
    memcpy(ptr, header->start, sizeof(header->start)); ptr += sizeof(header->start);
    memcpy(ptr, header->size, sizeof(header->size)); ptr += sizeof(header->size);
    memcpy(ptr, header->sequence, sizeof(header->sequence)); ptr += sizeof(header->sequence);
    memcpy(ptr, header->type, sizeof(header->type)); ptr += sizeof(header->type);
    memcpy(ptr, header->checksum, sizeof(header->checksum)); ptr += sizeof(header->checksum);
    
    if (header->data) {
        memcpy(ptr, header->data, atoi((const char*)header->size) * 8);
    }

    return message;
}