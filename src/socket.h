#ifndef SOCKET_H
#define SOCKET_H

#include <sys/socket.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>      
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <unistd.h>

#define DEFAULT_PORT 8080
#define START "01111110"

// Message types
#define ACK "0000"
#define NAK "0001"
#define OK_ACK "0010"
#define SIZE "0100"
#define DATA "0101"
#define TEXT_ACK_NAME "0110"
#define VIDEO_ACK_NAME "0111"
#define IMAGE_ACK_NAME "1000"
#define END "1001"
#define RIGHT "1010"
#define UP "1011"
#define DOWN "1100"
#define LEFT "1101"
#define MAC "1110"
#define ERROR "1111"

// Error types
#define PERMISSION_DENIED "0000"
#define NO_SPACE "0001"

#define MAX_DATA_SIZE 127

#define START_SIZE 8
#define SIZE_SIZE 7
#define SEQUENCE_SIZE 5
#define TYPE_SIZE 4
#define CHECKSUM_SIZE 8

// Kermit protocol header ( ASCII for binary representation )
typedef struct {
    unsigned char start[START_SIZE];
    unsigned char size[SIZE_SIZE];
    unsigned char sequence[SEQUENCE_SIZE];
    unsigned char type[TYPE_SIZE];
    unsigned char checksum[CHECKSUM_SIZE];
    unsigned char* data;
} kermit_protocol_header;

// api
int create_raw_socket();
void destroy_raw_socket(int sock);
int bind_raw_socket(int sock, char* interface, int port);
void connect_raw_socket(int sock, char* interface, unsigned char address[6]);
void send_package(int sock, char* interface, unsigned char dest_mac[6], const unsigned char* message, size_t message_len);
void send_ack_or_nack(int sock, char* interface, unsigned char server_mac[6], kermit_protocol_header* received, const char* type_str);
void receive_package(int sock, unsigned char* buffer, struct sockaddr_ll* sender_addr, socklen_t* addr_len);

int get_mac_address(int sock, const char* ifname, unsigned char* mac);

kermit_protocol_header* create_header(unsigned char size[SIZE_SIZE], unsigned char type[TYPE_SIZE], unsigned char* data);
kermit_protocol_header* create_ack_or_nack_header(kermit_protocol_header* original, const char* type_str);
kermit_protocol_header* read_bytes_into_header(unsigned char* buffer);
unsigned char* calculate_checksum(const unsigned char size[], size_t size_len, const unsigned char seq[], size_t seq_len, const unsigned char type[], size_t type_len, const unsigned char* data, size_t data_len);
unsigned int getHeaderSize(kermit_protocol_header* header);
void destroy_header(kermit_protocol_header* header);
void copy_header_deep(kermit_protocol_header** dest, const kermit_protocol_header* src);
const unsigned char* generate_message(kermit_protocol_header* header);
unsigned int check_if_same(kermit_protocol_header* header1, kermit_protocol_header* header2);
unsigned int checksum_if_valid(kermit_protocol_header* header);
void print_header(kermit_protocol_header* header);

unsigned int convert_binary_to_decimal(const unsigned char* binary, size_t size);
unsigned char* convert_decimal_to_binary_ascii(unsigned int value, size_t size);
unsigned char* convert_decimal_to_binary(unsigned int decimal, size_t size);
unsigned int checkIfNumberIsBigger(unsigned int a, unsigned int b);



#endif