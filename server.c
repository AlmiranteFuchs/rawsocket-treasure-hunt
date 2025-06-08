#include "server.h"

int main(){
    // set seed
    srand(time(NULL));

    server("veth2", 8080);
    return 0;
}