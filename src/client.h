#ifndef CLIENT_H
#define CLIENT_H
#include "socket.h"
#include "game.h"
#include "receiveBuffer.h"
#include <termios.h>

// Initiates default client behaviour
void initialize_connection_context(char* interface, unsigned char server_mac[6], int port);
void client();

kermit_protocol_header* move(unsigned char* direction);

kermit_protocol_header* move_left(char** grid, Position* pos);
kermit_protocol_header* move_right(char** grid, Position* pos);
kermit_protocol_header* move_up(char** grid, Position* pos);
kermit_protocol_header* move_down(char** grid, Position* pos);

void process_message(kermit_protocol_header* header);   // process server message

#endif