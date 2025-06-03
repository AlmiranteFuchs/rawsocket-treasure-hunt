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

    unsigned char buffer[4096];
    struct sockaddr_ll sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    receive_package(sock, buffer, &sender_addr, &addr_len);

    return header;
}

kermit_protocol_header* move_left(char** grid, Position* pos, int sock, char* interface, unsigned char server_mac[6]){
    unsigned char direction[TYPE_SIZE] = LEFT;
    kermit_protocol_header* header = move(sock, interface, server_mac, direction);
    move_player(grid, pos, '2');
    print_grid(grid);
    return header;
}

kermit_protocol_header* move_right(char** grid, Position* pos, int sock, char* interface, unsigned char server_mac[6]){
    unsigned char direction[TYPE_SIZE] = RIGHT;
    kermit_protocol_header* header = move(sock, interface, server_mac, direction);
    move_player(grid, pos, '3');
    print_grid(grid);
    return header;
}

kermit_protocol_header* move_up(char** grid, Position* pos, int sock, char* interface, unsigned char server_mac[6]){
    unsigned char direction[TYPE_SIZE] = UP;
    kermit_protocol_header* header = move(sock, interface, server_mac, direction);
    move_player(grid, pos, '0');
    print_grid(grid);
    return header;
}

kermit_protocol_header* move_down(char** grid, Position* pos, int sock, char* interface, unsigned char server_mac[6]){
    unsigned char direction[TYPE_SIZE] = DOWN;
    kermit_protocol_header* header = move(sock, interface, server_mac, direction);
    move_player(grid, pos, '1');
    print_grid(grid);
    return header;
}

/*
    0 - Text
    1 - Video
    2 - Image
*/
void create_file(kermit_protocol_header* header, unsigned int type){
    // create new file with its title and extension
    char* extension, *output_str;
    unsigned int extension_size;
    switch (type){
        case 0:
            extension = ".txt";
            extension_size = 3;
            break;
        case 1:
            extension = ".mp4";
            extension_size = 3;
            break;
        case 2:
            extension = ".jpeg";
            extension_size = 4;
            break;
    } 
    output_str = malloc(atoi(header->size) + extension_size);
    sprintf(output_str, "%s%s", header->data, extension);

    FILE* file = fopen(output_str, "w+");
    if (!file){
        return;
    }

    free(output_str);
    fclose(file);

}

void process_message(kermit_protocol_header* header){
    unsigned int type = convert_binary_to_decimal(header->type, TYPE_SIZE);

    switch (type) {
        // tamanho
        case 4:
            break;
        // dados
        case 5:
            break;
        // texto + ack + nome
        case 6:
            break;
        // video + ack + nome
        case 7:
            break;
        // imagem + ack + nome
        case 8:
            break;
        // fim de arquivo
        case 9:
            break;
        // erro
        case 15:
            break;
        default:
            printf("Unknown message type: %u\n", type);
            break;
    }

}