#include "client.h"

int main(){
    unsigned char server_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    client("lo", server_mac, 8080);
    return 0;
}