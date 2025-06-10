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

// Kermit protocol header
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
void receive_package(int sock, unsigned char* buffer, struct sockaddr_ll* sender_addr, socklen_t* addr_len);

kermit_protocol_header* create_header(unsigned char size[SIZE_SIZE], unsigned char type[TYPE_SIZE], unsigned char* data);
kermit_protocol_header* read_bytes_into_header(unsigned char* buffer);
unsigned int getHeaderSize(kermit_protocol_header* header);
void destroy_header(kermit_protocol_header* header);
void copy_header_deep(kermit_protocol_header** dest, const kermit_protocol_header* src);
const unsigned char* generate_message(kermit_protocol_header* header);
unsigned int check_if_same(kermit_protocol_header* header1, kermit_protocol_header* header2);
void print_header(kermit_protocol_header* header);

unsigned int convert_binary_to_decimal(const unsigned char* binary, size_t size);
unsigned char* convert_decimal_to_binary(unsigned int decimal, size_t size);
unsigned int checkIfNumberIsBigger(unsigned int a, unsigned int b);



#endif