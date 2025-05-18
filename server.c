#include "server.h"

int main(){
    // set seed
    srand(time(NULL));

    server("lo", 8080);
    return 0;
}