#include "server.h"

extern kermit_protocol_header* global_header_buffer;
extern kermit_protocol_header** global_receive_buffer;

void print_header(kermit_protocol_header* header){
    printf("Start: %.*s\n", START_SIZE, header->start);
    printf("Size: %.*s\n", SIZE_SIZE, header->size);
    printf("Sequence: %.*s\n", SEQUENCE_SIZE, header->sequence);
    printf("Type: %.*s\n", TYPE_SIZE, header->type);
    printf("Checksum: %.*s\n", CHECKSUM_SIZE, header->checksum);
    if (header->data != NULL) {
        unsigned int data_size = convert_binary_to_decimal(header->size, SIZE_SIZE);
        printf("Data: ");
        for (unsigned int i = 0; i < data_size; i++) {
            printf("%02x ", header->data[i]);
        }
        printf("\n");
    } else {
        printf("Data: NULL\n");
    }
}

// unsigned int checkIfRepeated(kermit_protocol_header* header){
//     unsigned int seq = convert_binary_to_decimal(header->sequence, SEQUENCE_SIZE);
//     if (global_receive_buffer != NULL) {
        
//         unsigned int global_seq = convert_binary_to_decimal(global_receive_buffer->sequence, SEQUENCE_SIZE);
//         unsigned int global_type = convert_binary_to_decimal(global_receive_buffer->type, TYPE_SIZE);
//         unsigned int header_type = convert_binary_to_decimal(header->type, TYPE_SIZE);
        
//         if (global_type == header_type && global_seq == seq) {
//             // repeated packet
//             return 1;
//         }
//         printf("header type: %.*s\n, header sequence: %.*s\n", TYPE_SIZE, header->type, SEQUENCE_SIZE, header->sequence);
//         printf("global type: %.*s\n, global sequence: %.*s\n", TYPE_SIZE, global_receive_buffer->type, SEQUENCE_SIZE, global_receive_buffer->sequence);
//     }
    
//     return 0;
// }

void listen_server(int sock){
    unsigned char buffer[4096];
    struct sockaddr_ll sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    kermit_protocol_header* header = NULL;

    Treasure** treasures = malloc(sizeof(Treasure*) * 8);

    Position* player_pos = initialize_player();
    char** grid = initialize_server_grid(player_pos, treasures);

    initialize_receive_buffer();

    printf("Server waiting for packets on loopback...\n");
    while (1) {
        receive_package(sock, buffer, &sender_addr, &addr_len); 
        header = read_bytes_into_header(buffer);
        if (header == NULL)
            continue;

        print_receive_buffer(); 
        
        kermit_protocol_header* temp = get_first_in_line_receive_buffer();
        if (temp != NULL){
            process_message(temp, grid, player_pos, treasures);
            destroy_header(temp);
        }
        // if (checkIfRepeated(header)){
            //     destroy_header(header);
            //     continue;
            // }
        update_receive_buffer(header);
        print_receive_buffer();
    }
}

void server(char* interface, int port){
    int sock = create_raw_socket();

    bind_raw_socket(sock, interface, port);
    listen_server(sock);
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
            entry = readdir(dir);

        treasures[i]->file_name = (char*) malloc(sizeof(entry));
        strcpy(treasures[i]->file_name, entry->d_name);
    }
    
    return grid;
}

unsigned int isPlayerOnTreasure(char** grid, Position* player_pos){
    return (grid[player_pos->x][player_pos->y] == EVENT);
}

void process_message(kermit_protocol_header* header, char** grid, Position* player_pos, Treasure** treasures){
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

    unsigned int idx = 0;
    char* file_name;

    if (!isPlayerOnTreasure(grid, player_pos))
        return;

    for (unsigned int i = 0; i < 8; i++){
        if (treasures[i]->pos->x == player_pos->x && treasures[i]->pos->y == player_pos->y){
            file_name = treasures[i]->file_name;
            break;
        }
    }

    printf("filename: %s\n", file_name);
}