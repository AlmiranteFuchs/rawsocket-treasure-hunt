#include "client.h"

int getch() {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

void client(char* interface, unsigned char server_mac[6], int port){
    int sock = create_raw_socket();
    bind_raw_socket(sock, interface, port);

    Position* player_pos = initialize_player();
    char** grid = initialize_grid(player_pos);
    print_grid(grid);

    char input = getch();
    while (input != 'q'){
        // listen to server
        // ...

        input = getch();
        switch (input) {
            case 'w':
                move_up(grid, player_pos, sock, interface, server_mac);
                break;
            case 'a':
                move_left(grid, player_pos, sock, interface, server_mac);
                break;
            case 's':
                move_down(grid, player_pos, sock, interface, server_mac);
                break;
            case 'd':
                move_right(grid, player_pos, sock, interface, server_mac);
                break;
            default:
                break;
        }
    }
}

kermit_protocol_header* move(int sock, char* interface, unsigned char server_mac[6], unsigned char* direction){
    unsigned char size[SIZE_SIZE] = "0000000";
    unsigned char sequence[SEQUENCE_SIZE] = "00000";
    unsigned char type[TYPE_SIZE];
    memcpy(type, direction, TYPE_SIZE);
    unsigned char* data = NULL;

    kermit_protocol_header* header = create_header(size, sequence, type, data);
    if (header == NULL) {
        fprintf(stderr, "Failed to create header\n");
        return NULL;
    }

    const unsigned char* generatedM = generate_message(header);
    unsigned int sizeMessage = getHeaderSize(header);

    send_package(sock, interface, server_mac, generatedM, sizeMessage);

    return header;
}

kermit_protocol_header* move_left(char** grid, Position* pos, int sock, char* interface, unsigned char server_mac[6]){
    unsigned char direction[TYPE_SIZE] = "0013"; // LEFT
    move_player(grid, pos, '2');
    print_grid(grid);
    return move(sock, interface, server_mac, direction);
}

kermit_protocol_header* move_right(char** grid, Position* pos, int sock, char* interface, unsigned char server_mac[6]){
    unsigned char direction[TYPE_SIZE] = "0010"; // RIGHT
    move_player(grid, pos, '3');
    print_grid(grid);
    return move(sock, interface, server_mac, direction);
}

kermit_protocol_header* move_up(char** grid, Position* pos, int sock, char* interface, unsigned char server_mac[6]){
    unsigned char direction[TYPE_SIZE] = "0011"; // UP
    move_player(grid, pos, '0');
    print_grid(grid);
    return move(sock, interface, server_mac, direction);
}

kermit_protocol_header* move_down(char** grid, Position* pos, int sock, char* interface, unsigned char server_mac[6]){
    unsigned char direction[TYPE_SIZE] = "0012"; // DOWN
    move_player(grid, pos, '1');
    print_grid(grid);
    return move(sock, interface, server_mac, direction);
}