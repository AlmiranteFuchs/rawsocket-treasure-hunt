#ifndef SERVER_H
#define SERVER_H
#include "socket.h"

void server(char* interface, int port);
void listen_server(int sock);

#endif