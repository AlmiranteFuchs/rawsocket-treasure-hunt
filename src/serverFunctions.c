#include "game.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>

// --- Globals for socket communication ---
int g_server_sock = -1;
char* g_server_interface = NULL;
unsigned char g_client_mac[6];
// --- -------------------------------- ---

extern kermit_protocol_header* global_header_buffer;
extern kermit_protocol_header** global_receive_buffer;
extern kermit_protocol_header* last_header;

unsigned int wait_for_ack_or_nack(kermit_protocol_header *sent_header) {
  while (1) {
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(g_server_sock, &readfds);
    tv.tv_sec = 0;
    tv.tv_usec = 10000000; // 10000ms

    int retval = select(g_server_sock + 1, &readfds, NULL, NULL, &tv);
    if (retval == -1) {
      perror("select()");
      return 0;
    } else if (retval == 0) {
      printf("Timeout waiting for ACK/NACK\n");
      return 0;
    }

    receive_and_buffer_packet(); // Update buffer
    kermit_protocol_header *response = get_first_in_line_receive_buffer();

    if (response == NULL)
      continue; // Try again

    // Check if ACK/NACK and if matches the sequence
    if (strncmp((char*)response->type, ACK, TYPE_SIZE) == 0) {
        if (memcmp(response->sequence, sent_header->sequence, SEQUENCE_SIZE) == 0) {
            destroy_header(response);
            return 1;
        }
    } else if (strncmp((char*)response->type, NAK, TYPE_SIZE) == 0) {
        if (memcmp(response->sequence, sent_header->sequence, SEQUENCE_SIZE) == 0) {
            printf("Nak lol\n");
            exit(1);
            destroy_header(response);
            return 0;
        }
    }

    destroy_header(
        response); // If not the one we want, discard and keep waiting
  }

  return 0; // fallback
}

void receive_and_buffer_packet() {
    unsigned char buffer[4096];
    struct sockaddr_ll sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    receive_package(g_server_sock, buffer, &sender_addr, &addr_len);
    kermit_protocol_header* header = read_bytes_into_header(buffer);
    if (header == NULL) return;

    if (last_header && check_if_same(header, last_header)) {
        destroy_header(header);
        return;
    }

    if (is_header_on_receive_buffer(header)) {
        destroy_header(header);
        return;
    }

    update_receive_buffer(header);
    copy_header_deep(&last_header, header);
}

void listen_server(Treasure** treasures, Position* player_pos, char** grid){
    printf("Server waiting for packets on loopback...\n");
    while (1) {
        receive_and_buffer_packet();

        kermit_protocol_header* temp = get_first_in_line_receive_buffer();
        if (temp != NULL) {
            process_message(temp, grid, player_pos, treasures);
            destroy_header(temp);
        }
    }
    printf("end\n");
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

        // FIXME: Remove after
        if(i == 7){
            treasures[i]->pos->x = 2;
            treasures[i]->pos->y = 2;
        }else{
            treasures[i]->pos->x = rand() % (GRID_SIZE - 2) + 1;
            treasures[i]->pos->y = rand() % (GRID_SIZE - 2) + 1;
        }
        // FIXME: Remove after
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
    char* file_name;

    if (!isPlayerOnTreasure(grid, player_pos))
        return;

    // Removes being found after being found
    grid[player_pos->x][player_pos->y] = FOUND;

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
    send_file(file_path);
}

void send_filename(char* filename, unsigned char* type){
    // copy the size of the filename into size
    unsigned int filename_length = strlen(filename);
    unsigned int data_size = filename_length + 1; // include null terminator!

    unsigned char* size_bin = convert_decimal_to_binary(data_size, SIZE_SIZE);

    unsigned char* filename_data = malloc(data_size);
    memcpy(filename_data, filename, filename_length);
    filename_data[filename_length] = '\0'; // make sure it ends correctly

    kermit_protocol_header* header = create_header(size_bin, type, filename_data);
    free(filename_data);  // safe to free, header made its own copy

    if (header == NULL) {
        fprintf(stderr, "Failed to create header\n");
        free(size_bin);
        return;
    }

    const unsigned char* message = generate_message(header);
    unsigned int message_size = getHeaderSize(header);
    send_package(g_server_sock, g_server_interface, g_client_mac, message, message_size);

    // Clean up
    free(size_bin);
    free((void*) message); // only if generate_message() mallocs
    destroy_header(header);
}

void send_file_size(FILE* file){
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
    send_package(g_server_sock, g_server_interface, g_client_mac, message, message_size);

    destroy_header(header);
}

void send_end_of_file(){
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
    send_package(g_server_sock, g_server_interface, g_client_mac, message, message_size);

    destroy_header(header);
}

void send_file_packages(char* filename, unsigned char* type){
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
    send_filename(f_name, type);
    send_file_size(file);

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
        send_package(g_server_sock, g_server_interface, g_client_mac, message, message_size);

        unsigned int result = wait_for_ack_or_nack( header);

        destroy_header(header);
    }

    send_end_of_file();

    free(filename);
    fclose(file);
}

void send_file(char* filename){
    char* extension = strrchr(filename, '.');
    if (extension) {
        extension++;
    }

    switch (extension[0]){
        case 't':   // text file
            send_file_packages(filename, (unsigned char*) TEXT_ACK_NAME);
            break;
        case 'm':   // video file
            send_file_packages(filename, (unsigned char*) VIDEO_ACK_NAME);
            break;
        case 'j':   // image file
            send_file_packages(filename, (unsigned char*) IMAGE_ACK_NAME);
            break;
        default:
            fprintf(stderr, "Unknown file type: %s\n", filename);
            break;
    }
}

void server(){
    // Initializes game logic
    Treasure** treasures = malloc(sizeof(Treasure*) * 8);
    Position* player_pos = initialize_player();
    
    char** grid = initialize_server_grid(player_pos, treasures);
    last_header = NULL;
    initialize_receive_buffer();

    listen_server(treasures, player_pos, grid);
}

void initialize_connection_context(char* interface, unsigned char client_mac[6], int port) {
    g_server_sock = create_raw_socket();
    bind_raw_socket(g_server_sock, interface, port);

    g_server_interface = interface;
    memcpy(g_client_mac, client_mac, 6);
}