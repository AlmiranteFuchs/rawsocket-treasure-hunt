#ifndef SOCKET_H
#define SOCKET_H

#include <math.h>
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
#include <sys/statvfs.h>

#define DEFAULT_PORT 8080
#define START 0x7E 

// Message types
#define ACK 0x0
#define NAK 0x1
#define OK_ACK 0x2
#define SIZE 0x4
#define DATA 0x5
#define TEXT_ACK_NAME 0x6
#define VIDEO_ACK_NAME 0x7
#define IMAGE_ACK_NAME 0x8
#define END 0x9
#define RIGHT 0xA
#define UP 0xB
#define DOWN 0xC
#define LEFT 0xD
#define MAC 0xE
#define ERROR 0xF

// Error types
#define PERMISSION_DENIED 0x0
#define NO_SPACE 0x1

#define MAX_DATA_SIZE 127

#define START_SIZE 8
#define SIZE_SIZE 7
#define SEQUENCE_SIZE 5
#define TYPE_SIZE 4
#define CHECKSUM_SIZE 8

// Kermit protocol header ( ASCII for binary representation )
typedef struct {
    unsigned char start;
    unsigned char size;
    unsigned char sequence;
    unsigned char type;
    unsigned char checksum;
    unsigned char* data;
} kermit_protocol_header;

// api
int create_raw_socket(void);
void destroy_raw_socket(int sock);
void connect_raw_socket(int sock, char* interface, unsigned char dest_mac[6]);
int bind_raw_socket(int sock, char* interface, int port);
void send_package(int sock, char* interface, unsigned char dest_mac[6], const unsigned char* message, size_t message_len);
void send_ack_or_nack(int sock, char* interface, unsigned char server_mac[6], kermit_protocol_header* received, const char type);
void send_error(int sock, char* interface, char* to_mac, char error_message);
void receive_package(int sock, unsigned char* buffer, struct sockaddr_ll* sender_addr, socklen_t* addr_len);

int get_mac_address(int sock, const char* ifname, unsigned char* mac);

kermit_protocol_header* create_header(unsigned char size, unsigned char type, unsigned char* data);
kermit_protocol_header* create_ack_or_nack_header(kermit_protocol_header* original, const char type);
kermit_protocol_header* read_bytes_into_header(unsigned char* buffer);
unsigned char calculate_checksum(unsigned char size, unsigned char seq, unsigned char type, const unsigned char* data);
unsigned int getHeaderSize(kermit_protocol_header* header);

void destroy_header(kermit_protocol_header* header);
void copy_header_deep(kermit_protocol_header** dest, const kermit_protocol_header* src);
unsigned char* generate_message(kermit_protocol_header* header);
unsigned int check_if_same(kermit_protocol_header* header1, kermit_protocol_header* header2);
unsigned int checksum_if_valid(kermit_protocol_header* header);
void print_header(kermit_protocol_header* header);

// unsigned int convert_binary_to_decimal(const unsigned char* binary, size_t size);
// void convert_decimal_to_binary_ascii(unsigned int value, size_t size, unsigned char* out);
// void convert_decimal_to_binary(unsigned int decimal, size_t size, unsigned char* out);
unsigned int checkIfNumberIsBigger(unsigned int a, unsigned int b);


#endif