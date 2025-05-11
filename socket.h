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


// Kermit protocol header
typedef struct {
    unsigned char start[8];
    unsigned char size[7];
    unsigned char sequence[5];
    unsigned char type[4];
    unsigned char checksum[8];
    unsigned char data[1016];
} kermit_protocol_header;

// api
int create_raw_socket();
int bind_raw_socket(int sock, char* interface, int port);
void connect_raw_socket(int sock, char* interface, unsigned char address[6]);
void send_package(int sock, char* interface, unsigned char dest_mac[6], const unsigned char* message);
void receive_package(int sock, unsigned char* buffer, struct sockaddr_ll* sender_addr, socklen_t* addr_len);

#endif