#ifndef SERVER_H
#define SERVER_H
#include "socket.h"
#include "game.h"
#include "receiveBuffer.h"
#include <time.h>
#include <dirent.h>

typedef struct {
    Position* pos;
    char* file_name;
} Treasure;

void server(char* interface, int port);

void listen_server(int sock, Treasure** treasures, Position* player_pos, char** grid);

char** initialize_server_grid(Position* player_pos, Treasure** tresures);
void process_message(kermit_protocol_header* header, char** grid, Position* player_pos, Treasure** tresures, int sock, char* interface, unsigned char server_mac[6]);
void send_file(char* filename, int sock, char* interface, unsigned char server_mac[6]);

#endif