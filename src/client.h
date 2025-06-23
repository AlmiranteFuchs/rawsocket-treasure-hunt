#ifndef CLIENT_H
#define CLIENT_H
#include "socket.h"
#include "game.h"
#include "receiveBuffer.h"
#include "log.h"
#include <termios.h>

// Initiates default client behaviour
void initialize_connection_context(char* interface, int port);
void client();

int send_package_until_ack(int sock, char* interface, unsigned char* mac, 
    const unsigned char* message, size_t message_size, kermit_protocol_header* header);
unsigned int wait_for_ack_or_nack(kermit_protocol_header *sent_header);
int move(unsigned char* direction);
void move_left(char** grid, Position* pos);
void move_right(char** grid, Position* pos);
void move_up(char** grid, Position* pos);
void move_down(char** grid, Position* pos);

void process_message(kermit_protocol_header* header);   // process server message
void wait_process_broadcast_message(); 

#endif