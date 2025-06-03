#ifndef SERVER_H
#define SERVER_H
#include "socket.h"
#include "game.h"
#include <time.h>
#include <dirent.h>

typedef struct {
    Position* pos;
    char* file_name;
} Treasure;

void server(char* interface, int port);
void listen_server(int sock);

kermit_protocol_header* read_bytes_into_header(unsigned char* buffer);

char** initialize_server_grid(Position* player_pos, Treasure** tresures);
void process_message(kermit_protocol_header* header, char** grid, Position* player_pos, Treasure** tresures);
void send_file_packages(unsigned int file_idx);

#endif