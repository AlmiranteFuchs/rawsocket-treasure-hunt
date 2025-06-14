#include "client.h"

extern kermit_protocol_header* last_header;
extern kermit_protocol_header** global_receive_buffer;

FILE* curr = NULL;
unsigned int curr_tam = 0;
unsigned int expected_sequence_local = 0;

int getch_timeout(int usec_timeout) {
    struct termios oldt, newt;
    int ch = -1;
    struct timeval tv;
    fd_set readfds;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    tv.tv_sec = 0;
    tv.tv_usec = usec_timeout;

    int retval = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
    if (retval > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
        ch = getchar();
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

void listen_to_server(int sock, char* interface, unsigned char server_mac[6], char** grid, Position* player_pos) {
    unsigned char buffer[4096];
    struct sockaddr_ll sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    kermit_protocol_header* header = NULL;

    while (1) {
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms

        int retval = select(sock + 1, &readfds, NULL, NULL, &tv);
        if (retval == -1) {
            perror("select()");
            break;
        } else if (retval == 0) {
            // timeout, nada mais para ler
            break;
        }
        
        // kermit_protocol_header* temp = get_first_in_line_receive_buffer();
        // if (temp != NULL) {
        //     process_message(temp);
        //     destroy_header(temp);
        // }
        
        receive_package(sock, buffer, &sender_addr, &addr_len); 
        header = read_bytes_into_header(buffer);
        
        if (header == NULL) {
            fprintf(stderr, "Failed to read header\n");
            continue;
        }
        
        if (last_header && check_if_same(header, last_header)) {
            printf("Received duplicate header, ignoring...\n");
            destroy_header(header);
            continue;
        }
        
        if (is_header_on_receive_buffer(header)) {
            printf("Filtered as already in buffer: ");
            destroy_header(header);
            continue;
        }
        
        update_receive_buffer(header);
        copy_header_deep(&last_header, header);
        kermit_protocol_header* temp;

        while ((temp = get_first_in_line_receive_buffer()) != NULL) {
            print_header(temp);
            process_message(temp);
            destroy_header(temp);
        }
    }
}


void client(char* interface, unsigned char server_mac[6], int port){
    int sock = create_raw_socket();
    bind_raw_socket(sock, interface, port);

    Position* player_pos = initialize_player();
    char** grid = initialize_grid(player_pos);
    initialize_receive_buffer();
    print_grid(grid);

    int input;
    while (1){
        input = getch_timeout(100000); // 100ms

        listen_to_server(sock, interface, server_mac, grid, player_pos);

        if (input == 'q') break;
        if (input == -1) continue; // timeout, volta para o loop

        if (!curr){
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
}

kermit_protocol_header* move(int sock, char* interface, unsigned char server_mac[6], unsigned char* direction){
    unsigned char size[SIZE_SIZE] = "0000000";
    // unsigned char sequence[SEQUENCE_SIZE] = "00000";
    unsigned char type[TYPE_SIZE];
    memcpy(type, direction, TYPE_SIZE);
    unsigned char* data = NULL;

    printf("Sending move command\n");

    kermit_protocol_header* header = create_header(size, type, data);
    if (header == NULL) {
        fprintf(stderr, "Failed to create header\n");
        return NULL;
    }

    const unsigned char* generatedM = generate_message(header);
    unsigned int sizeMessage = getHeaderSize(header);

    send_package(sock, interface, server_mac, generatedM, sizeMessage);

    // unsigned char buffer[4096];
    // struct sockaddr_ll sender_addr;
    // socklen_t addr_len = sizeof(sender_addr);
    // receive_package(sock, buffer, &sender_addr, &addr_len);

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

void read_to_file(kermit_protocol_header* header){
    if (curr == NULL) {
        fprintf(stderr, "No file is currently open to write data to.\n");
        return;
    }

    unsigned int size = convert_binary_to_decimal(header->size, SIZE_SIZE);
    fwrite(header->data, sizeof(unsigned char), size, curr);
    fflush(curr);
    printf("Wrote %u bytes to file.\n", size);
}

/*
    0 - Text
    1 - Video
    2 - Image
*/
void create_file(kermit_protocol_header* header){
    // create new file with its title and extension
    printf("Creating file: %s\n", header->data);

    FILE* file = fopen((const char*) header->data, "w+");
    if (!file){
        return;
    }

    curr = file;
}

void process_message(kermit_protocol_header* header){
    unsigned int type = convert_binary_to_decimal(header->type, TYPE_SIZE);

    switch (type) {
        // tamanho
        case 4:
            int size = convert_binary_to_decimal(header->size, SIZE_SIZE);
            printf("Received size: %u\n", size);
            curr_tam = size;
            break;
        // dados
        case 5:
            printf("Received data of size: %u\n", convert_binary_to_decimal(header->size, SIZE_SIZE));
            read_to_file(header);
            break;
        // texto + ack + nome
        case 6:
            create_file(header);
            break;
        // video + ack + nome
        case 7:
            break;
        // imagem + ack + nome
        case 8:
            break;
        // fim de arquivo
        case 9:
            printf("End of file received.\n");
            fclose(curr);
            curr = NULL;
            curr_tam = 0;
            expected_sequence_local = 0;
            break;
        // erro
        case 15:
            break;
        default:
            printf("Unknown message type: %u\n", type);
            break;
    }

}