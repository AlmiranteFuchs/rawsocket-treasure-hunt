#ifndef CLIENT_H
#define CLIENT_H
#include "socket.h"
#include "game.h"
#include "receiveBuffer.h"
#include <termios.h>

void client(char* interface, unsigned char server_mac[6], int port);

kermit_protocol_header* move(int sock, char* interface, unsigned char server_mac[6], unsigned char* direction);

kermit_protocol_header* move_left(char** grid, Position* pos, int sock, char* interface, unsigned char server_mac[6]);
kermit_protocol_header* move_right(char** grid, Position* pos, int sock, char* interface, unsigned char server_mac[6]);
kermit_protocol_header* move_up(char** grid, Position* pos, int sock, char* interface, unsigned char server_mac[6]);
kermit_protocol_header* move_down(char** grid, Position* pos, int sock, char* interface, unsigned char server_mac[6]);

void process_message(kermit_protocol_header* header);   // process server message

#endif