#ifndef SERVER_H
#define SERVER_H
#include "socket.h"

void server(char* interface, int port);
void listen_server(int sock);
kermit_protocol_header* read_bytes_into_header(unsigned char* buffer);
unsigned int convert_binary_to_decimal(const unsigned char* binary, size_t size);

#endif