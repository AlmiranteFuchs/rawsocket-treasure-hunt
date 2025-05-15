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

// Message types
#define ACK 0x00
#define NAK 0x01
#define OK_ACK 0x02
#define SIZE 0x04
#define DATA 0x05
#define TEXT_ACK_NAME 0x06
#define VIDEO_ACK_NAME 0x07
#define IMAGE_ACK_NAME 0x08
#define END 0x09
#define RIGHT 0x0A
#define UP 0x0B
#define DOWN 0x0C
#define LEFT 0x0D
#define ERROR 0x0F

// Error types
#define PERMISSION_DENIED 0x00
#define NO_SPACE 0x01

#define MAX_DATA_SIZE 1024

#define START 01111110
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
int bind_raw_socket(int sock, char* interface, int port);
void connect_raw_socket(int sock, char* interface, unsigned char address[6]);
void send_package(int sock, char* interface, unsigned char dest_mac[6], const unsigned char* message);
void receive_package(int sock, unsigned char* buffer, struct sockaddr_ll* sender_addr, socklen_t* addr_len);

kermit_protocol_header* create_header(unsigned char size[SIZE_SIZE], unsigned char sequence[SEQUENCE_SIZE], unsigned char type[TYPE_SIZE], unsigned char* data);
void destroy_header(kermit_protocol_header* header);
void destroy_raw_socket(int sock);

const unsigned char* generate_message(kermit_protocol_header* header);

#endif