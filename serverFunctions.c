#include "server.h"

extern kermit_protocol_header* global_header_buffer;
extern kermit_protocol_header** global_receive_buffer;
extern kermit_protocol_header* last_header;
extern unsigned int expected_sequence;

void listen_server(int sock, Treasure** treasures, Position* player_pos, char** grid){
    unsigned char buffer[4096];
    struct sockaddr_ll sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    kermit_protocol_header* header = NULL;

    printf("Server waiting for packets on loopback...\n");
    while (1) {
        receive_package(sock, buffer, &sender_addr, &addr_len); 
        header = read_bytes_into_header(buffer);
        if (header == NULL)
            continue;
            
        if (last_header){
            if (check_if_same(header, last_header)) {
                printf("Received duplicate header, ignoring...\n");
                destroy_header(header);
                continue;
            }
        }

        if (is_header_on_receive_buffer(header)) {
            destroy_header(header);
            continue;
        }

        update_receive_buffer(header);
        copy_header_deep(&last_header, header);

        kermit_protocol_header* temp = get_first_in_line_receive_buffer();
        if (temp != NULL){
            unsigned char sender_mac[6];
            memcpy(sender_mac, sender_addr.sll_addr, sizeof(sender_mac));
            process_message(temp, grid, player_pos, treasures, sock, "veth2", sender_mac);
            destroy_header(temp);
        }

        printf("expected sequence: %u\n", expected_sequence);
    }
}

void server(char* interface, int port){
    int sock = create_raw_socket();

    bind_raw_socket(sock, interface, port);
    
    Treasure** treasures = malloc(sizeof(Treasure*) * 8);
    Position* player_pos = initialize_player();
    char** grid = initialize_server_grid(player_pos, treasures);
    last_header = NULL;
    initialize_receive_buffer();

    listen_server(sock, treasures, player_pos, grid);
}

char** initialize_server_grid(Position* player_pos, Treasure** treasures){
    DIR *dir;
    struct dirent *entry;
    char** grid = initialize_grid(player_pos);

    dir = opendir("./objetos");
    if (!dir)
        fprintf(stderr, "Failed to open files directory\n");

    // set the 8 treasures locations
    for (int i = 0; i < 8; i++){
        treasures[i] = (Treasure*) malloc(sizeof(Treasure));
        treasures[i]->pos = (Position*) malloc(sizeof(Position));
        treasures[i]->pos->x = rand() % (GRID_SIZE - 2) + 1;
        treasures[i]->pos->y = rand() % (GRID_SIZE - 2) + 1;
        grid[treasures[i]->pos->x][treasures[i]->pos->y] = EVENT;

        entry = readdir(dir);
        if (!entry){
            fprintf(stderr, "Not enough files in the directory\n");
            return NULL;
        }
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        treasures[i]->file_name = (char*) malloc(sizeof(entry));
        strcpy(treasures[i]->file_name, entry->d_name);
    }
    
    return grid;
}

unsigned int isPlayerOnTreasure(char** grid, Position* player_pos){
    printf("Grid[%d][%d]: %c\n", player_pos->x, player_pos->y, grid[player_pos->x][player_pos->y]);
    return (grid[player_pos->x][player_pos->y] == EVENT);
}

void process_message(kermit_protocol_header* header, char** grid, Position* player_pos, Treasure** treasures, int sock, char* interface, unsigned char server_mac[6]){
    unsigned int type = convert_binary_to_decimal(header->type, TYPE_SIZE);
    unsigned char move_type;

    switch (type) {
        case 11:
            printf("Move Up\n");
            move_type = '0';
            break;
        case 12: 
            printf("Move Down\n");
            move_type = '1';
            break;
        case 13: 
            printf("Move Left\n");
            move_type = '2';
            break;
        case 10:
            printf("Move Right\n");
            move_type = '3';
            break;
        default:
            printf("Unknown message type: %u\n", type);
            break;
    }

    move_player(grid, player_pos, move_type);
    char* file_name;

    if (!isPlayerOnTreasure(grid, player_pos))
        return;

    for (unsigned int i = 0; i < 8; i++){
        if (treasures[i]->pos->x == player_pos->x && treasures[i]->pos->y == player_pos->y){
            file_name = treasures[i]->file_name;
            break;
        }
    }

    char* file_path = malloc(strlen("./objetos/") + strlen(file_name) + 1);
    if (file_path == NULL) {
        fprintf(stderr, "Failed to allocate memory for file path\n");
        return;
    }
    sprintf(file_path, "./objetos/%s", file_name);
    send_file(file_path, sock, interface, server_mac);
}

void send_filename(char* filename, unsigned char* type, int sock, char* interface, unsigned char server_mac[6]){
    // copy the size of the filename into size
    unsigned int filename_length = strlen(filename);
    unsigned char* size_bin = convert_decimal_to_binary(filename_length, SIZE_SIZE);

    kermit_protocol_header* header = create_header(size_bin, type, (unsigned char*) filename);
    if (header == NULL) {
        fprintf(stderr, "Failed to create header\n");
        free(size_bin);
        return;
    }

    const unsigned char* message = generate_message(header);
    unsigned int message_size = getHeaderSize(header);
    send_package(sock, interface, server_mac, message, message_size);
}

void send_file_size(FILE* file, int sock, char* interface, unsigned char server_mac[6]){
    if (file == NULL) {
        fprintf(stderr, "File pointer is NULL\n");
        return;
    }

    fseek(file, 0, SEEK_END);
    unsigned long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char size_bin[SIZE_SIZE];
    memcpy(size_bin, convert_decimal_to_binary(file_size, SIZE_SIZE), SIZE_SIZE);

    kermit_protocol_header* header = create_header(size_bin, (unsigned char*) SIZE, NULL);
    if (header == NULL) {
        fprintf(stderr, "Failed to create header for file size\n");
        return;
    }

    const unsigned char* message = generate_message(header);
    unsigned int message_size = getHeaderSize(header);
    send_package(sock, interface, server_mac, message, message_size);

    destroy_header(header);
}

void send_end_of_file(int sock, char* interface, unsigned char server_mac[6]){
    unsigned char end_type[TYPE_SIZE];
    unsigned char size[SIZE_SIZE]; // Size is 0 for end of file
    memcpy(end_type, END, TYPE_SIZE);
    memcpy(size, "0000000", SIZE_SIZE);
    kermit_protocol_header* header = create_header(size, end_type, NULL);
    if (header == NULL) {
        fprintf(stderr, "Failed to create end of file header\n");
        return;
    }

    const unsigned char* message = generate_message(header);
    unsigned int message_size = getHeaderSize(header);
    send_package(sock, interface, server_mac, message, message_size);

    destroy_header(header);
}

void send_file_packages(char* filename, unsigned char* type, int sock, char* interface, unsigned char server_mac[6]){
    if (filename == NULL || type == NULL) {
        fprintf(stderr, "Filename or type is NULL\n");
        return;
    }

    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        return;
    }

    char* f_name = strrchr(filename, '/');
    if (f_name) f_name++;
    send_filename(f_name, type, sock, interface, server_mac);
    send_file_size(file, sock, interface, server_mac);

    unsigned char data[MAX_DATA_SIZE];
    size_t bytes_read;

    for (int sequence = 0; (bytes_read = fread(data, 1, MAX_DATA_SIZE, file)) > 0; sequence++) {
        unsigned char size_bin[SIZE_SIZE];

        unsigned int size_decimal = (unsigned int) bytes_read;
        memcpy(size_bin, convert_decimal_to_binary(size_decimal, SIZE_SIZE), SIZE_SIZE);

        kermit_protocol_header* header = create_header(size_bin, (unsigned char*) DATA, data);
        if (header == NULL) {
            fprintf(stderr, "Failed to create header for data\n");
            fclose(file);
            return;
        }

        const unsigned char* message = generate_message(header);
        unsigned int message_size = getHeaderSize(header);
        send_package(sock, interface, server_mac, message, message_size);

        destroy_header(header);
    }

    send_end_of_file(sock, interface, server_mac);

    free(filename);
    fclose(file);
}

void send_file(char* filename, int sock, char* interface, unsigned char server_mac[6]){
    char* extension = strrchr(filename, '.');
    if (extension) {
        extension++;
    }

    switch (extension[0]){
        case 't':   // text file
            send_file_packages(filename, (unsigned char*) TEXT_ACK_NAME, sock, interface, server_mac);
            break;
        case 'm':   // video file
            send_file_packages(filename, (unsigned char*) VIDEO_ACK_NAME, sock, interface, server_mac);
            break;
        case 'j':   // image file
            send_file_packages(filename, (unsigned char*) IMAGE_ACK_NAME, sock, interface, server_mac);
            break;
        default:
            fprintf(stderr, "Unknown file type: %s\n", filename);
            break;
    }
}