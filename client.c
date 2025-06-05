#include "client.h"

int main(){
    unsigned char server_mac[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x02};

    client("veth1", server_mac, 8080);
    return 0;
}