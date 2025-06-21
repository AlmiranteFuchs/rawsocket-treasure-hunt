#ifndef SERVER_H
#define SERVER_H
#include "socket.h"
#include "game.h"
#include "receiveBuffer.h"
#include <time.h>
#include "log.h"
#include <dirent.h>

typedef struct {
    Position* pos;
    char* file_name;
} Treasure;

void initialize_connection_context(char* interface, unsigned char client_mac[6],int port);
void server();
void receive_and_buffer_packet();
void listen_server(Treasure** treasures, Position* player_pos, char** grid);

char** initialize_server_grid(Position* player_pos, Treasure** tresures);
void process_message(kermit_protocol_header* header, char** grid, Position* player_pos, Treasure** tresures);
void send_file(char* filename);

#endif