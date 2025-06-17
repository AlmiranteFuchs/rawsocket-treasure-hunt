#include "socket.h"

kermit_protocol_header* global_header_buffer = NULL;
kermit_protocol_header* last_header = NULL;
unsigned int expected_sequence = 0;

int create_raw_socket(){
    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock == -1){
        fprintf(stderr, "Error creating socket\n");
        exit(-1);
    }

    return sock;
}


void copy_header_deep(kermit_protocol_header** dest, const kermit_protocol_header* src) {
    if (*dest == NULL){
        *dest = (kermit_protocol_header*) malloc(sizeof(kermit_protocol_header));
    }

    if ((*dest)->data != NULL) {
        free((*dest)->data);
        (*dest)->data = NULL;
    }

    memcpy((*dest)->start, src->start, START_SIZE);
    memcpy((*dest)->size, src->size, SIZE_SIZE);
    memcpy((*dest)->sequence, src->sequence, SEQUENCE_SIZE);
    memcpy((*dest)->type, src->type, TYPE_SIZE);
    memcpy((*dest)->checksum, src->checksum, CHECKSUM_SIZE);

    if (src->data != NULL) {
        unsigned int data_size = convert_binary_to_decimal(src->size, SIZE_SIZE);
        (*dest)->data = malloc(data_size);
        if ((*dest)->data != NULL) {
            memcpy((*dest)->data, src->data, data_size);
        }
    } else {
        (*dest)->data = NULL;
    }
}


unsigned int convert_binary_to_decimal(const unsigned char* binary, size_t size) {
    unsigned int decimal = 0;
    for (size_t i = 0; i < size; ++i) {
        decimal = (decimal << 1) | (binary[i] - '0');
    }
    return decimal;
}

unsigned char* convert_decimal_to_binary_ascii(unsigned int value, size_t size) {
    unsigned char* binary = malloc(size + 1);
    if (!binary) return NULL;

    for (size_t i = 0; i < size; i++) {
        binary[size - 1 - i] = (value & (1 << i)) ? '1' : '0';
    }

    // Optional null-terminator for printing/debugging (not used in memcpy)
    binary[size] = '\0';

    return binary;
}

unsigned char* convert_decimal_to_binary(unsigned int decimal, size_t size) {
    unsigned char* binary = malloc(size);
    if (binary == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < size; ++i) {
        binary[size - 1 - i] = (decimal & (1 << i)) ? '1' : '0';
    }
    return binary;
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

void send_package(int sock, char* interface, unsigned char dest_mac[6], const unsigned char* message, size_t message_len) {
    ssize_t sent = send(sock, message, message_len, 0);
    if (sent == -1) {
        perror("send failed");
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

kermit_protocol_header* create_header(unsigned char size[SIZE_SIZE], unsigned char type[TYPE_SIZE], unsigned char* data){
    // Initializes header 
    kermit_protocol_header* header = (kermit_protocol_header*) malloc(sizeof(kermit_protocol_header));
    if (header == NULL)
        return NULL;

    memcpy(header->start, START, strlen(START));
    memcpy(header->size, size, SIZE_SIZE);
    memcpy(header->type, type, TYPE_SIZE);

    // Creates data
    unsigned int data_size = convert_binary_to_decimal(size, SIZE_SIZE);
    if (data != NULL) {
        header->data = malloc(data_size);
        if (header->data != NULL) {
            memcpy(header->data, data, data_size);
        }
    } else {
        header->data = NULL;
    }

    // Calculates checksum
    unsigned char* checksum = calculate_checksum(
        header->size, SIZE_SIZE,           // ASCII binary
        header->sequence, SEQUENCE_SIZE,     // ASCII binary
        header->type, TYPE_SIZE,           // ASCII binary
        header->data, data_size            // Raw binary
    );
    
    memcpy(header->checksum, checksum, CHECKSUM_SIZE);
    free(checksum);

    // Manages global buffer
    if (global_header_buffer) {
        unsigned int seq = convert_binary_to_decimal(global_header_buffer->sequence, SEQUENCE_SIZE);
        seq = (seq + 1) % (1 << SEQUENCE_SIZE);
        unsigned char* seq_bin = convert_decimal_to_binary(seq, SEQUENCE_SIZE);
        memcpy(header->sequence, seq_bin, SEQUENCE_SIZE);
        free(seq_bin);
    } else {
        memset(header->sequence, '0', SEQUENCE_SIZE);
    }

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
    size += START_SIZE;
    size += SIZE_SIZE;
    size += SEQUENCE_SIZE;
    size += TYPE_SIZE;
    size += CHECKSUM_SIZE;
    if (header->data != NULL) {
        size += convert_binary_to_decimal(header->size, SIZE_SIZE);
    }
    return size;
}

void print_header(kermit_protocol_header* header){
    printf("Start: %.*s\n", START_SIZE, header->start);
    printf("Size: %.*s\n", SIZE_SIZE, header->size);
    printf("Sequence: %.*s\n", SEQUENCE_SIZE, header->sequence);
    printf("Type: %.*s\n", TYPE_SIZE, header->type);
    printf("Checksum: %.*s\n", CHECKSUM_SIZE, header->checksum);
    if (header->data != NULL) {
        unsigned int data_size = convert_binary_to_decimal(header->size, SIZE_SIZE);
        printf("Data: ");
        for (unsigned int i = 0; i < data_size; i++) {
            printf("%02x ", header->data[i]);
        }
        printf("\n");
    } else {
        printf("Data: NULL\n");
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

const unsigned char* generate_message(kermit_protocol_header* header) {
    if (!header) return NULL;

    size_t total_size = START_SIZE + SIZE_SIZE + SEQUENCE_SIZE + TYPE_SIZE + CHECKSUM_SIZE;

    unsigned int data_size = 0;
    if (header->data) {
        data_size = convert_binary_to_decimal(header->size, SIZE_SIZE);
        total_size += data_size;
    }

    unsigned char* message = malloc(total_size);
    if (!message) return NULL;

    unsigned char* ptr = message;
    memcpy(ptr, header->start, sizeof(header->start)); ptr += sizeof(header->start);
    memcpy(ptr, header->size, sizeof(header->size)); ptr += sizeof(header->size);
    memcpy(ptr, header->sequence, sizeof(header->sequence)); ptr += sizeof(header->sequence);
    memcpy(ptr, header->type, sizeof(header->type)); ptr += sizeof(header->type);
    memcpy(ptr, header->checksum, sizeof(header->checksum)); ptr += sizeof(header->checksum);

    if (header->data && data_size > 0) {
        memcpy(ptr, header->data, data_size);
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
kermit_protocol_header* read_bytes_into_header(unsigned char* buffer){
    kermit_protocol_header* header = malloc(sizeof(kermit_protocol_header));
    if (header == NULL) {
        fprintf(stderr, "Failed to allocate memory for header\n");
        return NULL;
    }

    unsigned int offset = START_SIZE;
    memcpy(header->start, buffer, START_SIZE); 

    if (strcmp((char*) header->start, START) != 0) {
        fprintf(stderr, "Invalid start sequence\n");
        free(header);
        return NULL;
    }

    memcpy(header->size, buffer + offset, SIZE_SIZE); offset += SIZE_SIZE;
    memcpy(header->sequence, buffer + offset, SEQUENCE_SIZE); offset += SEQUENCE_SIZE;
    memcpy(header->type, buffer + offset, TYPE_SIZE); offset += TYPE_SIZE;
    memcpy(header->checksum, buffer + offset, CHECKSUM_SIZE); offset += CHECKSUM_SIZE;

    unsigned int data_size = convert_binary_to_decimal(header->size, SIZE_SIZE);    // does it need to be multiplied by 8?
    if (data_size > 0) {
        header->data = malloc(data_size);
        if (header->data == NULL) {
            fprintf(stderr, "Failed to allocate memory for data\n");
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

unsigned char* calculate_checksum(
    const unsigned char size[], size_t size_len,
    const unsigned char seq[], size_t seq_len,
    const unsigned char type[], size_t type_len,
    const unsigned char* data, size_t data_len
) {
    unsigned int sum = 0;

    for (size_t i = 0; i < size_len; ++i)
        sum += (size[i] == '1') ? 1 : 0;

    for (size_t i = 0; i < seq_len; ++i)
        sum += (seq[i] == '1') ? 1 : 0;

    for (size_t i = 0; i < type_len; ++i)
        sum += (type[i] == '1') ? 1 : 0;

    for (size_t i = 0; i < data_len; ++i) 
        sum += count_ones_in_byte(data[i]); // Actual bytes, not ascii representation

    // Wrap result to 8 bits
    sum = sum % 256;

    // Return ASCII binary string (e.g., 13 → "00001101")
    return convert_decimal_to_binary_ascii(sum, CHECKSUM_SIZE);
}

unsigned int check_if_same(kermit_protocol_header* header1, kermit_protocol_header* header2) {
    if (header1 == NULL || header2 == NULL) return 0;

    unsigned int seq1 = convert_binary_to_decimal(header1->sequence, SEQUENCE_SIZE);
    unsigned int seq2 = convert_binary_to_decimal(header2->sequence, SEQUENCE_SIZE);
    unsigned int type1 = convert_binary_to_decimal(header1->type, TYPE_SIZE);
    unsigned int type2 = convert_binary_to_decimal(header2->type, TYPE_SIZE);

    if (seq1 == seq2 && type1 == type2) {
        return 1; // Headers are the same
    }
    
    return 0;
}