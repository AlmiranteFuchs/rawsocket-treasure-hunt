#include "client.h"

void client(char* interface, unsigned char server_mac[6], int port){
    int sock = create_raw_socket();
    bind_raw_socket(sock, interface, port);
    // connect_raw_socket(sock, "lo", server_mac);
    const unsigned char message[] = "Hello, server!"; 

    kermit_protocol_header* header = create_header("0000000", "00000", "0000", (unsigned char*) message);
    const unsigned char* generatedM = generate_message(header);
    send_package(sock, interface, server_mac, generatedM);
}