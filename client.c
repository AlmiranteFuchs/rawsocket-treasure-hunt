#include "src/client.h"

int main(){
    // Temp local machine socket config
    // unsigned char server_mac[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x02};

    // Calls client startup

    // initialize_connection_context("enp3s0", 8080);
    initialize_connection_context("veth1", 8080);
    client();
    return 0;
}