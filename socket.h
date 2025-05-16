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
#define START 01111110

// Message types
#define ACK 0000
#define NAK 0001
#define OK_ACK 0002
#define SIZE 0004
#define DATA 0005
#define TEXT_ACK_NAME 0006
#define VIDEO_ACK_NAME 0007
#define IMAGE_ACK_NAME 0008
#define END 0009
#define RIGHT 0010
#define UP 0011
#define DOWN 0012
#define LEFT 0013
#define ERROR 0015

// Error types
#define PERMISSION_DENIED 0000
#define NO_SPACE 0001

#define MAX_DATA_SIZE 1024

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

kermit_protocol_header* create_header(unsigned char size[SIZE_SIZE], unsigned char sequence[SEQUENCE_SIZE], unsigned char type[TYPE_SIZE], unsigned char* data);
unsigned int getHeaderSize(kermit_protocol_header* header);
void destroy_header(kermit_protocol_header* header);

const unsigned char* generate_message(kermit_protocol_header* header);

#endif