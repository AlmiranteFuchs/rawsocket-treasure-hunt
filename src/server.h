#ifndef SERVER_H
#define SERVER_H
#include "socket.h"
#include "game.h"
#include "receiveBuffer.h"
#include <time.h>
#include "log_msg.h"
#include <dirent.h>

typedef struct {
    Position* pos;
    char* file_name;
} Treasure;

void initialize_connection_context(char* interface, int port);
void server();
void receive_and_buffer_packet();
void listen_server(Treasure** treasures, Position* player_pos, char** grid);
int send_package_until_ack(int sock, char* interface, unsigned char* mac, 
const unsigned char* message, size_t message_size, kermit_protocol_header* header);
unsigned int wait_for_ack_or_nack(kermit_protocol_header *sent_header);

char** initialize_server_grid(Position* player_pos, Treasure** tresures);
void process_message(kermit_protocol_header* header, char** grid, Position* player_pos, Treasure** tresures);
void send_file(char* filename);

#endif