#include "server.h"

void listen_server(int sock){
    unsigned char buffer[4096];
    struct sockaddr_ll sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    printf("Server waiting for packets on loopback...\n");
    while (1) {
       receive_package(sock, buffer, &sender_addr, &addr_len); 
    }
}

void server(char* interface, int port){
    int sock = create_raw_socket();

    bind_raw_socket(sock, interface, port);
    listen_server(sock);
}