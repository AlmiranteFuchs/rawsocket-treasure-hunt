#include "src/server.h"

int main(){
    // set seed
    srand(time(NULL));

    unsigned char client_mac[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01};

    initialize_connection_context("veth2", client_mac, 8080);
    server();
    return 0;
}