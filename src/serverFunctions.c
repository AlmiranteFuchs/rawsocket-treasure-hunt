#include "game.h"
#include "server.h"
#include "socket.h"
#include <stdio.h>
#include <stdlib.h>

// --- Globals for socket communication ---
int g_server_sock = -1;
char* g_server_interface = NULL;
unsigned char g_self_mac[6];
unsigned char g_client_mac[6];
// --- -------------------------------- ---

extern kermit_protocol_header* global_header_buffer;
extern kermit_protocol_header** global_receive_buffer;
extern kermit_protocol_header* last_header;

int send_package_until_ack(int sock, char* interface, unsigned char* mac, 
    const unsigned char* message, size_t message_size, 
    kermit_protocol_header* header) {

    int result = -1;
    int attempt = 0;
    do {
        if (attempt > 20){
            log_err("Too many attempts to send package, giving up");
            return 0; 
        }

        if (attempt > 0) {
            unsigned int seq = (unsigned int) header->sequence;
            log_info("Resending package #%d, attempt #%d",seq, attempt);
        }
        send_package(sock, interface, mac, message, message_size);
        attempt++;

        result = wait_for_ack_or_nack(header);
    } while (result == 0);

    if(result == 1){
        log_msg("ACK");
    }else if(result == 2){
        log_msg("ACK ERROR");
    }else{
        log_err("Invalid return for movement request");
    }
     
    return result;
}

unsigned int wait_for_ack_or_nack(kermit_protocol_header *sent_header) {
  while (1) {
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(g_server_sock, &readfds);
    tv.tv_sec = 0;
    tv.tv_usec = 1000000; // 1000ms

    int retval = select(g_server_sock + 1, &readfds, NULL, NULL, &tv);
    if (retval == -1) {
      perror("select()");
      return 0;
    } else if (retval == 0) {
        log_info("Timeout waiting for ACK/NACK");
      return 0;
    }

    receive_and_buffer_packet(); // Update buffer
    kermit_protocol_header *response = get_first_in_line_receive_buffer();
    
    if (response == NULL){
        continue; // Try again
    }
    printf("Server received packet: type=%d\n", (int) response->type);

    // Check if ACK/NACK and if matches the sequence
    if (response->type == ACK) {
        if (response->sequence == sent_header->sequence) {
            destroy_header(response);
            return 1;
        }
    } else if (response->type == NAK) {
        if (response->sequence == sent_header->sequence) {
            log_info("Received NACK for seq: #%d", (int) response->sequence);
            destroy_header(response);
            return 0;
        }
    } else if (response->type == ERROR) {
        if (response->sequence == sent_header->sequence) {
            log_info("Received ERROR for seq: #%d", (int) response->sequence);
            destroy_header(response);
            return 2;
        }
    }

    destroy_header(response); 
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
      log_msg("Received duplicate header, re-sending the last packet...");
      if (global_header_buffer) {
        char* message = generate_message(global_header_buffer);
        unsigned int message_size = getHeaderSize(global_header_buffer);

        send_package(g_server_sock, g_server_interface, g_client_mac, (const unsigned char*) message, message_size);
        free((void*) message);
      }
      destroy_header(header);
      return;
    }

    if (is_header_on_receive_buffer(header)) {
        destroy_header(header);
        return;
    }

    if (!checksum_if_valid(header)) {
        log_err("Received packet with checksum error");
  
        log_msg("Sending NACK for seq: #%d", (int) header->sequence);
        send_ack_or_nack(g_server_sock, g_server_interface, g_client_mac, header, NAK);
  
        destroy_header(header);
        return;
      }

    update_receive_buffer(header);
    copy_header_deep(&last_header, header);
}

void listen_server(Treasure** treasures, Position* player_pos, char** grid){
    log_info("\nServer waiting for packets on loopback...\n");
    while (1) {
        receive_and_buffer_packet();

        kermit_protocol_header* temp = get_first_in_line_receive_buffer();
        if (temp != NULL) {
            process_message(temp, grid, player_pos, treasures);
            destroy_header(temp);
        }
    }
}

char* obtain_client_mac(char* interface){
    // Will loop and send broadcast messages until the client responds and then stabilish a handshake
    initialize_receive_buffer();

    unsigned char broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    kermit_protocol_header* header = create_header((unsigned char) 6, (unsigned char) MAC, g_self_mac);
    char* message = generate_message(header);
    unsigned int message_size = getHeaderSize(header);

    log_info("Sending broadcast message to obtain client MAC");
    if (send_package_until_ack(g_server_sock, g_server_interface, broadcast_mac,
        (const unsigned char*) message, message_size, header) == 1) {
        log_info("Broadcast message sent successfully");
    }

    while (1) {
        log_info("Waiting for MAC response from client...");

        receive_and_buffer_packet();

        kermit_protocol_header* temp = get_first_in_line_receive_buffer();
        if (temp != NULL) {
            if (temp->type == MAC) {
                memcpy(g_client_mac, temp->data, 6);
                log_info("Received MAC response");
                send_ack_or_nack(g_server_sock, g_server_interface, g_client_mac, temp, ACK);
                destroy_header(temp);

                return (char*)g_client_mac;
            }

            destroy_header(temp);
        }
    }

    return NULL;
}

char** initialize_server_grid(Position* player_pos, Treasure** treasures){
    DIR *dir;
    struct dirent *entry;
    char** grid = initialize_grid(player_pos);

    dir = opendir("./objetos");
    if (!dir)
        log_err("Failed to open files directory");

    // set the 8 treasures locations
    unsigned int i = 0;
    while(i < 8){
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
            log_err("Not enough files in the directory");
            return NULL;
        }
        printf("Found file: %s\n", entry->d_name);
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        treasures[i]->file_name = malloc(strlen(entry->d_name) + 1);

        if (!treasures[i]->file_name){
            log_err("Failed to allocate memory for file name");
            return NULL;
        }
        strcpy(treasures[i]->file_name, entry->d_name);
        i++;
    }
    
    return grid;
}

void process_message(kermit_protocol_header* header, char** grid, Position* player_pos, Treasure** treasures){
    unsigned int type = (int) header->type;
    unsigned char move_type;

    switch (type) {
        case 11:
            log_info("Move Up");
            move_type = '0';
            break;
        case 12: 
            log_info("Move Down");
            move_type = '1';
            break;
        case 13: 
            log_info("Move Left");
            move_type = '2';
            break;
        case 10:
            log_info("Move Right");
            move_type = '3';
            break;
        default:
            log_err("Unknown message type: %u", type);
            break;
    }

    // If this ack ok or error fails the server unsyncs and client is stuck in waiting ;c
    if(move_player(grid, player_pos, move_type)){
        log_msg("Valid player movement, sending ACK OK");
        // send_ack_or_nack(g_server_sock, g_server_interface, g_client_mac, header, OK_ACK);
        send_ack_or_nack(g_server_sock, g_server_interface, g_client_mac, header, ACK);
    }else{
        log_msg("Invalid player movement, sending ERROR");
        send_ack_or_nack(g_server_sock, g_server_interface, g_client_mac, header, ERROR);
        return;
    }

    char* file_name = NULL;

    // Removes being found after being found
    grid[player_pos->x][player_pos->y] = FOUND;

    for (unsigned int i = 0; i < 8; i++){
        if (treasures[i]->pos->x == player_pos->x && treasures[i]->pos->y == player_pos->y){
            file_name = treasures[i]->file_name;
            break;
        }
    }

    if (!file_name) {
        return;
    }

    char* file_path = malloc(strlen("./objetos/") + strlen(file_name) + 1);
    if (file_path == NULL) {
        log_err("Failed to allocate memory for file path");
        return;
    }
    sprintf(file_path, "./objetos/%s", file_name);
    send_file(file_path);
    free(file_path); 
}

int send_filename(char* filename, unsigned char type){
    // copy the size of the filename into size
    unsigned int filename_length = strlen(filename);
    unsigned int data_size = filename_length + 1; // include null terminator!

    unsigned char size_bin = (unsigned char) data_size;

    unsigned char* filename_data = malloc(data_size);
    memcpy(filename_data, filename, filename_length);
    filename_data[filename_length] = '\0'; // make sure it ends correctly

    kermit_protocol_header* header = create_header(size_bin, type, filename_data);
    free(filename_data);  // safe to free, header made its own copy

    if (header == NULL) {
        log_err("Failed to create header");
        return 0;
    }

    const unsigned char* message = generate_message(header);
    unsigned int message_size = getHeaderSize(header);

    log_info("Sending file name to client");
    int send_result = send_package_until_ack(g_server_sock, g_server_interface, g_client_mac, message, message_size, header);

    free((void*) message); // only if generate_message() mallocs
    destroy_header(header);

    return send_result;
}

int send_file_size(FILE* file) {
    if (file == NULL) {
        log_err("File pointer is NULL");
        return 0;
    }

    fseek(file, 0, SEEK_END);
    unsigned long long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Determine minimum number of bytes to encode file_size
    int data_len = 0;
    unsigned long long temp = file_size;
    do {
        data_len++;
        temp >>= 8;
    } while (temp > 0);

    if (data_len > 127) {
        log_err("File size too large to encode in protocol (max 127 bytes)");
        return 0;
    }

    unsigned char data[127];
    for (int i = 0; i < data_len; i++) {
        data[data_len - 1 - i] = (file_size >> (8 * i)) & 0xFF; // big-endian
    }

    // Convert data_len to 7-bit ASCII binary string for size field
    unsigned char bin_size = (unsigned char) data_len;

    kermit_protocol_header* header = create_header(bin_size, (unsigned char) SIZE, data);
    if (header == NULL) {
        log_err("Failed to create header for file size");
        return 0;
    }

    const unsigned char* message = generate_message(header);
    unsigned int message_size = getHeaderSize(header);

    log_info("Sending file size (%llu bytes) to client", file_size);
    int send_result = send_package_until_ack(g_server_sock, g_server_interface, g_client_mac, message, message_size, header);

    destroy_header(header);
    free((void*) message); 
    return send_result;
}

void send_end_of_file(){
    unsigned char end_type = END;
    unsigned char size = (unsigned char) 0; 

    kermit_protocol_header* header = create_header(size, end_type, NULL);
    if (header == NULL) {
        log_err("Failed to create end of file header");
        return;
    }

    const unsigned char* message = generate_message(header);
    unsigned int message_size = getHeaderSize(header);

    log_msg("Sending end of file to client");
    send_package_until_ack(g_server_sock, g_server_interface, g_client_mac, message, message_size, header);

    destroy_header(header);
    free((void*) message); // only if generate_message() mallocs
}

void send_file_packages(char* filename, unsigned char type){
    log_info("Starting to send data over to client\n");
    if (filename == NULL) {
        log_err("Filename or type is NULL");
        return;
    }

    if (access(filename, F_OK) != 0 || access(filename, R_OK) != 0) {
        log_err("File does not exist or is not readable: %s", filename);
        send_error(g_server_sock, g_server_interface, (char*) g_client_mac, PERMISSION_DENIED);
        return;
    }

    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        log_err("Failed to open file: %s", filename);
        return;
    }

    char* f_name = strrchr(filename, '/');
    if (f_name) f_name++;

    if(send_filename(f_name, type) == 2){
        log_err("Failed to send file name");
        return;
    }

    if(send_file_size(file) == 2){
        log_err("Failed to send file size");
        return;
    }

    unsigned char data[MAX_DATA_SIZE];
    size_t bytes_read;

    log_msg("Sending file data to client:");

    for (int sequence = 0; (bytes_read = fread(data, 1, MAX_DATA_SIZE, file)) > 0; sequence++) {
        unsigned char size_bin;

        unsigned int size_decimal = (unsigned int) bytes_read;
        size_bin = (unsigned char) size_decimal;

        kermit_protocol_header* header = create_header(size_bin, (unsigned char) DATA, data);
        if (header == NULL) {
            log_err("Failed to create header for data");
            fclose(file);
            return;
        }

        const unsigned char* message = generate_message(header);
        unsigned int message_size = getHeaderSize(header);

        log_info("Sending File data for seq: #%d", (int) header->sequence);
        send_package_until_ack(g_server_sock, g_server_interface, g_client_mac, message, message_size, header);

        destroy_header(header);
        free((void*) message);
    }

    send_end_of_file();

    fclose(file);
}

void send_file(char* filename){
    char* extension = strrchr(filename, '.');
    if (extension) {
        extension++;
    }

    switch (extension[0]){
        case 't':   // text file
            send_file_packages(filename, (unsigned char) TEXT_ACK_NAME);
            break;
        case 'm':   // video file
            send_file_packages(filename, (unsigned char) VIDEO_ACK_NAME);
            break;
        case 'j':   // image file
            send_file_packages(filename, (unsigned char) IMAGE_ACK_NAME);
            break;
        default:
            log_err("Unknown file type: %s", filename);
            break;
    }
}

void server(){
    // Initializes game logic
    Treasure** treasures = malloc(sizeof(Treasure*) * 8);
    Position* player_pos = initialize_player();
    
    char** grid = initialize_server_grid(player_pos, treasures);
    last_header = NULL;
    // initialize_receive_buffer();

    listen_server(treasures, player_pos, grid);
}

void initialize_connection_context(char* interface, int port) {
    g_server_interface = interface;

    g_server_sock = create_raw_socket();
    bind_raw_socket(g_server_sock, interface, port);
    
    get_mac_address(g_server_sock, interface, g_self_mac);
    obtain_client_mac(g_server_interface);
    


    // g_server_interface = interface;
    // memcpy(g_client_mac, client_mac, 6);
}