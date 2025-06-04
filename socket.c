#include "socket.h"

kermit_protocol_header* global_header_buffer = NULL;
kermit_protocol_header** global_receive_buffer = NULL;

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

void copy_header_deep(kermit_protocol_header* dest, const kermit_protocol_header* src) {
    if (dest->data != NULL) {
        free(dest->data);
        dest->data = NULL;
    }

    memcpy(dest->start, src->start, START_SIZE);
    memcpy(dest->size, src->size, SIZE_SIZE);
    memcpy(dest->sequence, src->sequence, SEQUENCE_SIZE);
    memcpy(dest->type, src->type, TYPE_SIZE);
    memcpy(dest->checksum, src->checksum, CHECKSUM_SIZE);

    if (src->data != NULL) {
        unsigned int data_size = convert_binary_to_decimal(src->size, SIZE_SIZE);
        dest->data = malloc(data_size);
        if (dest->data != NULL) {
            memcpy(dest->data, src->data, data_size);
        }
    } else {
        dest->data = NULL;
    }
}

unsigned int convert_binary_to_decimal(const unsigned char* binary, size_t size) {
    unsigned int decimal = 0;
    for (size_t i = 0; i < size; ++i) {
        decimal = (decimal << 1) | (binary[i] - '0');
    }
    return decimal;
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

kermit_protocol_header* create_header(unsigned char size[SIZE_SIZE], unsigned char type[TYPE_SIZE], unsigned char* data){
    kermit_protocol_header* header = (kermit_protocol_header*) malloc(sizeof(kermit_protocol_header));
    if (header == NULL)
        return NULL;

    memcpy(header->start, START, strlen(START));
    memcpy(header->size, size, SIZE_SIZE);
    // memcpy(header->sequence, sequence, SEQUENCE_SIZE);
    memcpy(header->type, type, TYPE_SIZE);
    // create checksum function later
    memset(header->checksum, 0, sizeof(header->checksum));

    /*
    * Checks if the global header buffer is set and if the types match to calculate the next sequence number
    */
    if (global_header_buffer && memcmp(global_header_buffer->type, type, TYPE_SIZE) == 0) {
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
    }

    if (global_header_buffer->data != NULL) {
        free(global_header_buffer->data);
    }

    copy_header_deep(global_header_buffer, header);

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

/*
 * Check if the sequence number a is greater than b considering the wrap-around (for sequence numbers)
 */
unsigned int checkIfNumberIsBigger(unsigned int a, unsigned int b){
    unsigned int max_seq = 2 << (SEQUENCE_SIZE - 1);

    if (a > b && (a - b) < max_seq) {
        return 1;
    } else {
        return 0;
    }
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

unsigned int initialize_receive_buffer() {
    global_receive_buffer = malloc(sizeof(kermit_protocol_header*) * (2 << SEQUENCE_SIZE));
    if (global_receive_buffer == NULL) {
        fprintf(stderr, "Failed to allocate memory for receive buffer\n");
        return 0;
    }

    return 1;
}

unsigned int is_header_on_receive_buffer(kermit_protocol_header* header) {
    if (global_receive_buffer == NULL || global_receive_buffer[0] == NULL) {
        return 0; // Buffer is empty
    }

    unsigned int seq = convert_binary_to_decimal(header->sequence, SEQUENCE_SIZE);
    unsigned int type = convert_binary_to_decimal(header->type, TYPE_SIZE);

    for (unsigned int i = 0; global_receive_buffer[i] != NULL; i++) {
        unsigned int temp_seq = convert_binary_to_decimal(global_receive_buffer[i]->sequence, SEQUENCE_SIZE);
        unsigned int temp_type = convert_binary_to_decimal(global_receive_buffer[i]->type, TYPE_SIZE);

        if (temp_seq == seq && temp_type == type) {
            return 1; // Header found in the buffer
        }
    }

    return 0;
}

void insert_in_i_receive_buffer(kermit_protocol_header* header, unsigned int index) {
    // Shift the headers to the right to make space for the new header
    int cpy = index;
    while (global_receive_buffer[cpy] != NULL) {
        global_receive_buffer[cpy + 1] = global_receive_buffer[cpy];
        cpy++;
    }

    // Insert the new header at the specified index
    global_receive_buffer[index] = header;
}

unsigned int update_receive_buffer(kermit_protocol_header* header) {
    if (global_receive_buffer == NULL) {
        fprintf(stderr, "Receive buffer is not initialized\n");
        return 0;
    }

    if (is_header_on_receive_buffer(header)) {
        return 1; // Cabeçalho já existe
    }

    // Converte sequência e tipo para decimal para comparação
    unsigned int seq = convert_binary_to_decimal(header->sequence, SEQUENCE_SIZE);
    unsigned int type = convert_binary_to_decimal(header->type, TYPE_SIZE);

    // Encontra a posição correta para inserir o cabeçalho
    unsigned int i = 0;
    while (global_receive_buffer[i] != NULL) {
        unsigned int temp_seq = convert_binary_to_decimal(global_receive_buffer[i]->sequence, SEQUENCE_SIZE);
        unsigned int temp_type = convert_binary_to_decimal(global_receive_buffer[i]->type, TYPE_SIZE);

        // Verifica se o cabeçalho atual é maior que o cabeçalho no buffer
        if (checkIfNumberIsBigger(seq, temp_seq) && type == temp_type) {
            insert_in_i_receive_buffer(header, i);
            return 1;
        }

        i++;
    }

    // Se nenhuma posição foi encontrada, adiciona o cabeçalho no final do buffer
    global_receive_buffer[i] = header;

    return 1;
}

kermit_protocol_header* get_first_in_line_receive_buffer() {
    if (global_receive_buffer == NULL || *global_receive_buffer == NULL) {
        return NULL; // Buffer is empty
    }

    kermit_protocol_header* first_header = *global_receive_buffer;

    // Shift the buffer to remove the first header
    unsigned int i = 0;
    while (global_receive_buffer[i] != NULL) {
        global_receive_buffer[i] = global_receive_buffer[i + 1];
        i++;
    }

    return first_header;
}

void print_receive_buffer(){
    if (global_receive_buffer == NULL) {
        printf("Receive buffer is not initialized.\n");
        return;
    }

    printf("Receive Buffer:\n");
    for (unsigned int i = 0; global_receive_buffer[i] != NULL; i++) {
        kermit_protocol_header* header = global_receive_buffer[i];
        printf("Header %u:\n", i);
        // printf("  Start: %.*s\n", START_SIZE, header->start);
        // printf("  Size: %.*s\n", SIZE_SIZE, header->size);
        printf("  Sequence: %.*s\n", SEQUENCE_SIZE, header->sequence);
        printf("  Type: %.*s\n", TYPE_SIZE, header->type);
        // printf("  Checksum: %.*s\n", CHECKSUM_SIZE, header->checksum);
        // if (header->data) {
        //     printf("  Data: %s\n", header->data);
        // } else {
        //     printf("  Data: NULL\n");
        // }
    }
}