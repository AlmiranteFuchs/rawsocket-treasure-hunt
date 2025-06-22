#ifndef CLIENT_H
#define CLIENT_H
#include "socket.h"
#include "game.h"
#include "receiveBuffer.h"
#include "log.h"
#include <termios.h>

// Initiates default client behaviour
void initialize_connection_context(char* interface, unsigned char server_mac[6], int port);
void client();

int move(unsigned char* direction);

void move_left(char** grid, Position* pos);
void move_right(char** grid, Position* pos);
void move_up(char** grid, Position* pos);
void move_down(char** grid, Position* pos);

void process_message(kermit_protocol_header* header);   // process server message

#endif