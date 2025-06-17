#include "src/client.h"

int main(){
    // Temp local machine socket config
    unsigned char server_mac[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x02};

    // Calls client startup
    client("veth1", server_mac, 8080);
    return 0;
}