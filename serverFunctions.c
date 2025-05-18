#include "server.h"
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

void listen_server(int sock){
    unsigned char buffer[4096];
    struct sockaddr_ll sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    kermit_protocol_header* header = NULL;

    printf("Server waiting for packets on loopback...\n");
    while (1) {
        receive_package(sock, buffer, &sender_addr, &addr_len); 
        header = read_bytes_into_header(buffer);
        print_header(header);
    }
}

void server(char* interface, int port){
    int sock = create_raw_socket();

    bind_raw_socket(sock, interface, port);
    listen_server(sock);
}

unsigned int convert_binary_to_decimal(const unsigned char* binary, size_t size) {
    unsigned int decimal = 0;
    for (size_t i = 0; i < size; ++i) {
        decimal = (decimal << 1) | (binary[i] - '0');
    }
    return decimal;
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

    unsigned int data_size = convert_binary_to_decimal(header->size, SIZE_SIZE);
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
