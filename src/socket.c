#include "socket.h"
#include "log_msg.h"

kermit_protocol_header* global_header_buffer = NULL;
kermit_protocol_header* last_header = NULL;
unsigned int expected_sequence = 0;

int create_raw_socket(){
    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock == -1){
        log_err("Error creating socket\n");
        exit(-1);
    }

    return sock;
}

void copy_header_deep(kermit_protocol_header** dest, const kermit_protocol_header* src) {
    if (dest == NULL || src == NULL) return;

    if (*dest == NULL) {
        *dest = (kermit_protocol_header*) malloc(sizeof(kermit_protocol_header));
        if (*dest == NULL) {
            log_err("Failed to allocate memory for header copy");
            return;
        }
        (*dest)->data = NULL; // Initialize
    }

    // Free previous data if present
    if ((*dest)->data != NULL) {
        free((*dest)->data);
        (*dest)->data = NULL;
    }

    // Copy scalar fields
    (*dest)->start = src->start;
    (*dest)->size = src->size;
    (*dest)->sequence = src->sequence;
    (*dest)->type = src->type;
    (*dest)->checksum = src->checksum;

    // Copy data if present
    if (src->data != NULL && src->size > 0) {
        unsigned int data_size = (unsigned int) src->size;
        (*dest)->data = malloc(data_size);
        if ((*dest)->data != NULL) {
            memcpy((*dest)->data, src->data, data_size);
        } else {
            log_err("Failed to allocate memory for header data copy");
        }
    } else {
        (*dest)->data = NULL;
    }
}



// unsigned int convert_binary_to_decimal(const unsigned char binary, size_t size) {
//     // If binary is a pointer to a single byte (unsigned char), interpret its bits
//     unsigned int decimal = 0;
//     for (size_t i = 0; i < size; ++i) {
//         decimal <<= 1;
//         decimal |= (value >> (size - 1 - i)) & 0x01;
//     }
//     return decimal;
// }

// unsigned char convert_decimal_to_binary(unsigned int value, size_t size) {
//     unsigned char result = 0;
//     for (size_t i = 0; i < size; i++) {
//         if (value & (1 << i)) {
//             result |= (1 << i);
//         }
//     }
//     return result;
// }

int bind_raw_socket(int sock, char* interface, int port){
    int ifindex = if_nametoindex(interface);

    struct sockaddr_ll address = {0};
    address.sll_family = AF_PACKET;
    address.sll_protocol = htons(ETH_P_ALL);
    address.sll_ifindex = ifindex;

    if(bind(sock, (struct sockaddr*)&address, sizeof(address)) == -1){
        log_err("Error binding socket to interface\n");
        exit(-1);
    }

    struct packet_mreq mr = {0};
    mr.mr_ifindex = ifindex;
    mr.mr_type = PACKET_MR_PROMISC;
    if (setsockopt(sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1){
        log_err("Error setting setsockopt\n"); 
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
        log_err("connect() failed: %s (errno=%d)\n", strerror(errno), errno);
        exit(-1);
    }
}

void send_ack_or_nack(int sock, char* interface, unsigned char server_mac[6], kermit_protocol_header* received, const char type){
    if (!received) return;

    kermit_protocol_header* response = create_ack_or_nack_header(received, type);
    if (!response) {
        log_err("Failed to create %s header\n", type == ACK ? "ACK" : "NAK");
        return;
    }

    const unsigned char* message = generate_message(response);
    unsigned int message_size = getHeaderSize(response);

    send_package(sock, interface, server_mac, message, message_size);

    free((void*) message);
    destroy_header(response);
}

// void send_package(int sock, char* interface, unsigned char dest_mac[6], const unsigned char* message, size_t message_len) {
//     struct sockaddr_ll addr = {0};
//     addr.sll_family = AF_PACKET;
//     addr.sll_protocol = htons(ETH_P_ALL);
//     addr.sll_ifindex = if_nametoindex(interface);
//     addr.sll_halen = ETH_ALEN;
//     memcpy(addr.sll_addr, dest_mac, ETH_ALEN);

//     // need to ensure the message is at least 14 bytes
//     if (message_len < 14){
//         // add padding
//         unsigned char padding[14 - message_len];
//         memset(padding, 0, sizeof(padding));
//         unsigned char* padded_message = malloc(14);
//         if (padded_message == NULL) {
//             log_err("Failed to allocate memory for padded message\n");
//             return;
//     }

//         memcpy(padded_message, message, message_len);
//         memcpy(padded_message + message_len, padding, sizeof(padding));
//         ssize_t sent = sendto(sock, padded_message, 14, 0, (struct sockaddr*)&addr, sizeof(addr));
//         if (sent == -1) {
//             perror("sendto failed");
//             free(padded_message);
//             return;
//         }
//         log_msg_v("Sent %zd bytes with padding\n", sent);
//         free(padded_message);

//         return;
//     }

//     ssize_t sent = sendto(sock, message, message_len, 0, (struct sockaddr *)&addr, sizeof(addr));
//     if (sent == -1){
//         perror("sendto failed");
//         return;
//     }
//     log_msg_v("Sent %zd bytes with padding\n", sent);
// }

void send_package(int sock, char* interface, unsigned char dest_mac[6], const unsigned char* message, size_t message_len) {
    struct sockaddr_ll addr = {0};
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex = if_nametoindex(interface);
    addr.sll_halen = ETH_ALEN;
    memcpy(addr.sll_addr, dest_mac, ETH_ALEN);

    unsigned char* msg_to_send = malloc(message_len);
    if (!msg_to_send) {
        log_err("Memory allocation failed in send_package");
        return;
    }
    memcpy(msg_to_send, message, message_len);

    // VLAN spoofing prevention and encoding flag
    if (message_len >= 15 && msg_to_send[12] == 0x81 && msg_to_send[13] == 0x00) {
        log_info("Detected accidental VLAN sequence 0x8100, patching...");

        // Mark a flag in message[14] (first byte after header)
        msg_to_send[14] |= 0x80; // set MSB
        msg_to_send[13] = 0xF0;  // replace 0x00 → 0xF0
    }

    if (message_len < 14) {
        unsigned char* padded = calloc(1, 14);
        if (!padded) {
            free(msg_to_send);
            return;
        }
        memcpy(padded, msg_to_send, message_len);

        ssize_t sent = sendto(sock, padded, 14, 0, (struct sockaddr*)&addr, sizeof(addr));
        free(padded);
        free(msg_to_send);
        return;
    }

    ssize_t sent = sendto(sock, msg_to_send, message_len, 0, (struct sockaddr *)&addr, sizeof(addr));
    free(msg_to_send);
}


// void receive_package(int sock, unsigned char* buffer, struct sockaddr_ll* sender_addr, socklen_t* addr_len){
//     ssize_t bytes = recvfrom(sock, buffer, 4096, 0,
//                             (struct sockaddr*)sender_addr, addr_len);
//     if (bytes == -1) {
//         perror("recvfrom failed");
//         exit(-1);
//     }
//     log_msg_v("Received %zd bytes\n", bytes);
// }

void receive_package(int sock, unsigned char* buffer, struct sockaddr_ll* sender_addr, socklen_t* addr_len){
    ssize_t bytes = recvfrom(sock, buffer, 4096, 0, (struct sockaddr*)sender_addr, addr_len);
    if (bytes == -1) {
        perror("recvfrom failed");
        exit(-1);
    }

    if (bytes >= 15 && buffer[12] == 0x81 && buffer[13] == 0xF0) {
        // Check if MSB of next byte is set
        if (buffer[14] & 0x80) {
            log_info("Undoing patched VLAN-like sequence 0x81F0 to 0x8100");
            buffer[13] = 0x00;       // restore original 0x00
            buffer[14] &= 0x7F;      // clear flag
        }
    }
}


int get_mac_address(int sock, const char* ifname, unsigned char* mac){
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIFHWADDR, &ifr) == -1) {
        perror("ioctl failed");
        return -1;
    }

    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
    return 0;
}

kermit_protocol_header* create_ack_or_nack_header(kermit_protocol_header* original, const char type) {
    if (original == NULL) {
        log_err("Original header is NULL, cannot create ACK/NAK header\n");
        return NULL;
    }

    // Allocate and initialize
    kermit_protocol_header* header = (kermit_protocol_header*) malloc(sizeof(kermit_protocol_header));
    if (header == NULL) {
        log_err("Failed to allocate memory for ACK/NAK header\n");
        return NULL;
    }

    header->start = (unsigned char) START;

    // size = 0
    header->size = (unsigned char) 0;

    // Copy sequence from original header
    header->sequence = original->sequence;

    // Type: ACK ("0000") or NAK ("0001")
    header->type = type;

    // No data
    header->data = NULL;

    // Checksum
    unsigned char checksum = calculate_checksum(
        header->size,
        header->sequence,
        header->type,
        header->data
    );

    header->checksum = checksum;

    copy_header_deep(&global_header_buffer, header);

    return header;
}

void send_error(int sock, char* interface, char* to_mac, char error_message) {
    unsigned char size = (unsigned char) 1;
    unsigned char type = ERROR;

    unsigned char* data = malloc(sizeof(unsigned char)); 
    if (data == NULL) {
        log_err("Failed to allocate memory for error message\n");
        return;
    }
    *data = error_message;

    log_msg_v("Sending error message: %s\n", error_message);

    kermit_protocol_header* header = create_header(size, type, data);
    if (header == NULL) {
        log_err("Failed to create header for error message\n");
        return;
    }

    const unsigned char* generatedM = generate_message(header);
    unsigned int sizeMessage = getHeaderSize(header);

    log_info("Sending error message");
    send_package(sock, interface, (unsigned char*) to_mac, generatedM, sizeMessage);

    free((void*) generatedM);
    free(header);
}

kermit_protocol_header* create_header(unsigned char size, unsigned char type, unsigned char* data){
    
    // Initializes header 
    kermit_protocol_header* header = (kermit_protocol_header*) malloc(sizeof(kermit_protocol_header));
    if (header == NULL)
    return NULL;
    header->start = START;
    header->size = size;
    header->type = type;
    
    // Creates data
    unsigned int data_size = (int) size;
    if (data != NULL) {
        header->data = malloc(data_size);
        if (header->data != NULL) {
            memcpy(header->data, data, data_size);
        }
    } else {
        header->data = NULL;
    }
    
    // Manages global buffer
    if (global_header_buffer) {
        unsigned int seq = (int) global_header_buffer->sequence;
        seq = (seq + 1) % (1 << 1);
        header->sequence = (unsigned char) seq;
    } else {
        header->sequence = (unsigned char) 0;
    }
    
    // Calculates checksum
    unsigned char checksum = calculate_checksum(
        header->size, 
        header->sequence, 
        header->type, 
        header->data
    );

    header->checksum = checksum;

    if (global_header_buffer == NULL) {
        global_header_buffer = (kermit_protocol_header*) malloc(sizeof(kermit_protocol_header));
        if (global_header_buffer == NULL) {
            free(header);
            return NULL;
        }
        global_header_buffer->data = NULL;
    }
    
    copy_header_deep(&global_header_buffer, header);
    
    return header;
}

unsigned int getHeaderSize(kermit_protocol_header* header) {
    if (header == NULL) return 0;

    unsigned int size = 0;
    size += 5;
    if (header->data != NULL) {
        size += (int) header->size;
    }
    return size;
}

void print_header(kermit_protocol_header* header){
    log_msg_v("Header:\n");
    log_msg_v("Start: %u\n", (int) header->start);
    log_msg_v("Size: %u\n", (int) header->size);
    log_msg_v("Sequence: %u\n", (int) header->sequence);
    log_msg_v("Type: %u\n", (int) header->type);
    log_msg_v("Checksum: %u\n", (int) header->checksum);

    if (header->data != NULL) {
        log_msg_v("Data: ");
        for (unsigned int i = 0; i < (int) header->size; i++) {
            log_msg_v("%02x ", header->data[i]);
        }
        log_msg_v("\n");
    } else {
        log_msg_v("Data: NULL\n");
    }
}

void destroy_header(kermit_protocol_header* header){
    if (header != NULL){
        if (header->data != NULL) {
            free(header->data);
            header->data = NULL;
        }
        free(header);
    }
}

unsigned char* generate_message(kermit_protocol_header* header) {
    if (!header) return NULL;

    unsigned int data_size = 0;
    if (header->data) {
        data_size = (int) header->size;
    }
    size_t total_size = 4 + 1 + data_size; // 4 bytes for packed fields, 1 for checksum, plus data

    unsigned char* message = malloc(total_size);
    if (!message) return NULL;

    size_t offset = 0;

    // 1. Start byte
    message[offset++] = header->start;

    // 2. Pack size (7 bits) and upper 1 bit of sequence (bit 4 of sequence)
    //    size: bits 7-1, sequence[4]: bit 0
    unsigned char size_seq = ((header->size & 0x7F) << 1) | ((header->sequence >> 4) & 0x01);
    message[offset++] = size_seq;

    // 3. Lower 4 bits of sequence (bits 3-0), upper 4 bits of type (bits 3-0)
    unsigned char seq_type = ((header->sequence & 0x0F) << 4) | (header->type & 0x0F);
    message[offset++] = seq_type;

    // 4. Checksum (8 bits)
    message[offset++] = header->checksum;

    if (header->data && data_size > 0) {
        memcpy(message + offset, header->data, data_size);
    }

    return message;
}

/*
 * Check if the sequence number a is greater than b considering the wrap-around (for sequence numbers)
 */
unsigned int checkIfNumberIsBigger(unsigned int a, unsigned int b){
    unsigned int max_seq = 1 << SEQUENCE_SIZE;
    return ((a - b + max_seq) % max_seq) < (max_seq / 2);
}

/*
    * Function to read bytes from a buffer into a kermit_protocol_header structure.
    * The function allocates memory for the header and its data field, and fills
    * in the fields based on the provided buffer.
*/
kermit_protocol_header* read_bytes_into_header(unsigned char* buffer) {
    if (!buffer) return NULL;

    if (sizeof(buffer) < 5) {
        log_err("Buffer too small to contain a valid header\n");
        return NULL;
    }

    kermit_protocol_header* header = malloc(sizeof(kermit_protocol_header));
    if (!header) {
        log_err("Failed to allocate memory for header\n");
        return NULL;
    }

    unsigned int offset = 0;

    header->start = buffer[offset++];
    if (header->start != START) {
        log_err("Invalid start byte: %02x\n", header->start);
        free(header);
        return NULL;
    }

    unsigned char size_seq = buffer[offset++];
    unsigned char seq_type = buffer[offset++];

    header->size = (size_seq >> 1) & 0x7F;
    header->sequence = ((size_seq & 0x01) << 4) | ((seq_type >> 4) & 0x0F);
    header->type = seq_type & 0x0F;

    header->checksum = buffer[offset++];

    unsigned int data_size = header->size;
    if (data_size > 0) {
        header->data = malloc(data_size);
        if (!header->data) {
            log_err("Failed to allocate memory for data\n");
            free(header);
            return NULL;
        }
        memcpy(header->data, buffer + offset, data_size);
    } else {
        header->data = NULL;
    }

    return header;
}


int count_ones_in_byte(unsigned char byte) {
    int count = 0;
    while (byte) {
        count += byte & 1;
        byte >>= 1;
    }
    return count;
}

unsigned char calculate_checksum(
    unsigned char size,
    unsigned char seq,
    unsigned char type,
    const unsigned char* data
) {
    unsigned int sum = 0;

    unsigned int data_len = (unsigned int) size;
    sum += count_ones_in_byte(size);
    sum += count_ones_in_byte(seq);
    sum += count_ones_in_byte(type);

    if (data != NULL && data_len > 0) {
        for (size_t i = 0; i < data_len; ++i) {
            sum += count_ones_in_byte(data[i]);
        }
    }

    return (unsigned char)(sum % 256);
}

unsigned int check_if_same(kermit_protocol_header* header1, kermit_protocol_header* header2) {
    if (header1 == NULL || header2 == NULL) return 0;

    if (header1->sequence == header2->sequence && header1->type == header2->type) {
        return 1;
    }
    
    return 0;
}

unsigned int checksum_if_valid(kermit_protocol_header* header){
    // unsigned int data_size = (unsigned int) header->size;

    unsigned char calculated = calculate_checksum(
        header->size,
        header->sequence, 
        header->type, 
        header->data
    );

    // Compare checksum values directly
    int valid = 1;
    if (header->checksum != calculated) {
        valid = 0;
    }

    return valid;
}