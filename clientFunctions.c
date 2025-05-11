#include "client.h"

void client(char* interface, unsigned char server_mac[6], int port){
    int sock = create_raw_socket();
    bind_raw_socket(sock, interface, port);
    // connect_raw_socket(sock, "lo", server_mac);
    const unsigned char message[] = "Hello, server!"; 
    send_package(sock, interface, server_mac, message);
}