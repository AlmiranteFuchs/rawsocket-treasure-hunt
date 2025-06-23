#include "client.h"
#include "socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// --- Globals for socket communication ---
int g_sock = -1;
char* g_interface = NULL;
unsigned char self_mac[6];
unsigned char g_server_mac[6];
// --- -------------------------------- ---

// --- Globals for Header Management ---
extern kermit_protocol_header* last_header;
extern kermit_protocol_header** global_receive_buffer;
extern kermit_protocol_header* global_header_buffer;
// --- -------------------------------- ---

// --- Globals for File Management ---
FILE* curr = NULL;
unsigned int curr_tam = 0;
char curr_filename[256] = {0};
// --- -------------------------------- ---

// States of the client
typedef enum {
    STATE_PLAYING,
    STATE_DATA_TRANSFER
} ProtocolState;

ProtocolState state = STATE_PLAYING;

// Terminal manipulation
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
void receive_and_buffer_packet() {
    unsigned char buffer[4096];
    struct sockaddr_ll sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    // // Packet Processing
    // Builds packet
    receive_package(g_sock, buffer, &sender_addr, &addr_len);
    kermit_protocol_header* header = read_bytes_into_header(buffer);
    if (header == NULL) return;

    //  Duplicate validation
    if (last_header && check_if_same(header, last_header)) {
      log_msg_v("Received duplicate header, re-sending the last packet...");
      if (global_header_buffer) {
        char* message = generate_message(global_header_buffer);
        unsigned int message_size = getHeaderSize(global_header_buffer);
        send_package(g_sock, g_interface, g_server_mac, (const unsigned char*) message, message_size);
        free((void*) message);
      }
      destroy_header(header);
      return;
    }
    if (is_header_on_receive_buffer(header)) {
      log_msg("Filtered as already in buffer: ");
      destroy_header(header);
      return;
    }

    if (!checksum_if_valid(header)) {
      log_err("Received packet with checksum error");

      log_msg("Sending NACK for seq: #%d",
      convert_binary_to_decimal(header->sequence, SEQUENCE_SIZE));
      send_ack_or_nack(g_sock, g_interface, g_server_mac, header, NAK);

      destroy_header(header);
      return;
    }

    update_receive_buffer(header);
    copy_header_deep(&last_header, header);
}

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
            unsigned int seq = convert_binary_to_decimal(header->sequence, SEQUENCE_SIZE);
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
    FD_SET(g_sock, &readfds);
    tv.tv_sec = 0;
    tv.tv_usec = 1000000; // 1000ms

    int retval = select(g_sock + 1, &readfds, NULL, NULL, &tv);
    if (retval == -1) {
      perror("select()");
      return 0;
    } else if (retval == 0) {
        log_info("Timeout waiting for ACK/NACK");
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
            log_info("Received NACK for seq: #%d", convert_binary_to_decimal(response->sequence, SEQUENCE_SIZE));
            destroy_header(response);
            return 0;
        }
    } else if (strncmp((char*)response->type, ERROR, TYPE_SIZE) == 0) {
        if (memcmp(response->sequence, sent_header->sequence, SEQUENCE_SIZE) == 0) {
            log_info("Received ERROR for seq: #%d", convert_binary_to_decimal(response->sequence, SEQUENCE_SIZE));
            destroy_header(response);
            return 2;
        }
    }

    destroy_header(
        response); // If not the one we want, discard and keep waiting
  }

  return 0; // fallback
}


// This wait for ack is only for movement, client doenst wait for more things
// from server, so it doesnt need to be waiting for ack
// As the movement state is difficult to track as we dont have a get player pos
// Or another source of truth and the server can lose the ACK_OK sent
// And we cant wait for ack of an ack_ok of an movement
unsigned int wait_for_ack_ok(kermit_protocol_header *sent_header) {
    while (1) {
      fd_set readfds;
      struct timeval tv;
      FD_ZERO(&readfds);
      FD_SET(g_sock, &readfds);
      tv.tv_sec = 0;
      tv.tv_usec = 1000000; // 1000ms
  
      int retval = select(g_sock + 1, &readfds, NULL, NULL, &tv);
      if (retval == -1) {
        perror("select()");
        return 0;
      } else if (retval == 0) {
          log_info("Timeout waiting for ACK/NACK");
        return 0;
      }
  
      receive_and_buffer_packet(); // Update buffer
      kermit_protocol_header *response = get_first_in_line_receive_buffer();
  
      if (response == NULL)
        continue; // Try again
  
      // Check if ACK/NACK and if matches the sequence
      if (strncmp((char*)response->type, OK_ACK, TYPE_SIZE) == 0) {
          if (memcmp(response->sequence, sent_header->sequence, SEQUENCE_SIZE) == 0) {
              destroy_header(response);
              return 1;
          }
      } else if (strncmp((char*)response->type, ERROR, TYPE_SIZE) == 0) {
          if (memcmp(response->sequence, sent_header->sequence, SEQUENCE_SIZE) == 0) {
              log_info("Received ERROR for move request, invalid position");
              destroy_header(response);
              return 2;
          }
      }
  
      destroy_header(
          response); // If not the one we want, discard and keep waiting
    }
  
    return 0; // fallback
  }

int send_move_package_until_ack_ok(int sock, char* interface, unsigned char* mac, 
    const unsigned char* message, size_t message_size, 
    kermit_protocol_header* header) {

    int result = -1;
    int attempt = 0;
    do {
        if (attempt > 0) {
            unsigned int seq = convert_binary_to_decimal(header->sequence, SEQUENCE_SIZE);
            log_info("Resending package #%d, attempt #%d",seq, attempt);
        }
        send_package(sock, interface, mac, message, message_size);
        attempt++;

        result = wait_for_ack_ok(header);
    } while (result == 0);
    
    if(result == 1){
        log_msg("Movement success");
    }else if(result == 2){
        log_msg("Invalid movement");
    }else{
        log_err("Invalid return for movement request");
    }

    return result;
}

void listen_to_server(int sock, char* interface, unsigned char server_mac[6], char** grid, Position* player_pos) {
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
        
        receive_and_buffer_packet();
        
        kermit_protocol_header* temp;

        while ((temp = get_first_in_line_receive_buffer()) != NULL) {
            process_message(temp);
            destroy_header(temp);
        }
    }
}

int move(unsigned char* direction){
    unsigned char size[SIZE_SIZE] = "0000000";
    unsigned char type[TYPE_SIZE];
    memcpy(type, direction, TYPE_SIZE);
    unsigned char* data = NULL;

    log_msg_v("Sending move command\n");

    kermit_protocol_header* header = create_header(size, type, data);
    if (header == NULL) {
        log_err( "Failed to create header\n");
        return 0;
    }

    const unsigned char* generatedM = generate_message(header);
    unsigned int sizeMessage = getHeaderSize(header);

    log_info("Sending move request");
    // int result = send_move_package_until_ack_ok(g_sock, g_interface, g_server_mac, generatedM, sizeMessage, header);
    int result = send_package_until_ack(g_sock, g_interface, g_server_mac, generatedM, sizeMessage, header);

    free((void*) generatedM);
    free(header);

    return result;
}

void move_left(char** grid, Position* pos){
    unsigned char direction[TYPE_SIZE] = LEFT;
    int result = move(direction);
    if(result == 1)
        move_player(grid, pos, '2');
    print_grid(grid);
}

void move_right(char** grid, Position* pos){
    unsigned char direction[TYPE_SIZE] = RIGHT;
    int result = move(direction);
    if(result == 1)
        move_player(grid, pos, '3');
    print_grid(grid);
}

void move_up(char** grid, Position* pos){
    unsigned char direction[TYPE_SIZE] = UP;
    int result = move(direction);
    if(result == 1)
        move_player(grid, pos, '0');
    print_grid(grid);
}

void move_down(char** grid, Position* pos){
    unsigned char direction[TYPE_SIZE] = DOWN;
    int result = move(direction);
    if(result == 1)
        move_player(grid, pos, '1');
    print_grid(grid);
}

int read_to_file(kermit_protocol_header* header){
    if (curr == NULL) {
        log_err( "No file is currently open to write data to.\n");
        return 0;
    }

    unsigned int size = convert_binary_to_decimal(header->size, SIZE_SIZE);
    fwrite(header->data, sizeof(unsigned char), size, curr);
    fflush(curr);
    log_msg_v("Wrote %u bytes to file.\n", size);
    return 1;
}

/*
    0 - Text
    1 - Video
    2 - Image
*/
int create_file(kermit_protocol_header* header) {
    // create new file with its title and extension
    log_info("Creating file: %s", header->data);

    FILE* file = fopen((const char*) header->data, "w+");
    if (!file) {
        return 0;
    }

    curr = file;

    // Save filename globally
    strncpy(curr_filename, (const char*) header->data, sizeof(curr_filename) - 1);
    curr_filename[sizeof(curr_filename) - 1] = '\0';  // ensure null-termination

    return 1;
}

unsigned long get_free_disk_space(const char* path, unsigned long* blockSize) {
    struct statvfs stat;
    if (statvfs(path, &stat) != 0) {
        perror("statvfs failed");
        return 0;
    }
    *blockSize = stat.f_bsize;
    return stat.f_bsize * stat.f_bavail;
}

void process_message(kermit_protocol_header* header) {
    unsigned int type = convert_binary_to_decimal(header->type, TYPE_SIZE);

    switch (state) {
        // --- Default game logic ---
        case STATE_PLAYING:
            switch (type) {
                case 6: {
                    // Texto + ack + nome
                    log_info("# Starting to receive Text data");
                    if(create_file(header)){
                        log_msg("Sending ACK\n");
                        send_ack_or_nack(g_sock, g_interface, g_server_mac, header, ACK);
                        state = STATE_DATA_TRANSFER;
                    }else{
                        log_err("Could not create file");
                        send_ack_or_nack(g_sock, g_interface, g_server_mac, header, ERROR);
                    }
                    
                    break;
                }
                case 7: {
                    // Vídeo + ack + nome (TODO)
                    log_info("# Starting to receive Video data");
                    // create_file(header);
                    // state = STATE_DATA_TRANSFER;
                    break;
                }
                case 8: {
                    // Imagem + ack + nome (TODO)
                    log_info("# Starting to receive image data");
                    // create_file(header);
                    // state = STATE_DATA_TRANSFER;
                    break;
                }
                default: {
                    log_err("# Unknown message type (PLAYING): %u\n", type);
                    break;
                }
            }
            break;

        // --- File receiving logic ---
        case STATE_DATA_TRANSFER:
            switch (type) {
                case 4: {
                    // Tamanho do arquivo
                    int data_len = convert_binary_to_decimal(header->size, SIZE_SIZE);
                    if (data_len <= 0 || data_len > 127) {
                        log_err("Invalid data length in size field: %d", data_len);
                        return;
                    }

                    unsigned long long file_size = 0;
                    for (int i = 0; i < data_len; i++) {
                        file_size = (file_size << 8) | header->data[i];
                    }

                    log_info("File size received: %llu\n", file_size);
                    unsigned long blockSize = 0;
                    unsigned long free_space = get_free_disk_space(".", &blockSize);
                    unsigned long real_file_size = ceil((double)file_size / blockSize) * blockSize;
                    if (free_space < real_file_size) {
                        log_err("Not enough disk space to receive file. Required: %lu, Available: %lu", real_file_size, free_space);
                        send_error(g_sock, g_interface, (char*) g_server_mac, NO_SPACE);
                        return;
                    }

                    curr_tam = file_size;
                    send_ack_or_nack(g_sock, g_interface, g_server_mac, header, ACK);

                    break;
                }
                case 5: {
                    // Dados do arquivo
                    log_info("File data received for seq: #%u", convert_binary_to_decimal(header->sequence, SEQUENCE_SIZE));
                    if(read_to_file(header)){
                        send_ack_or_nack(g_sock, g_interface, g_server_mac, header, ACK);
                    }else{
                        send_ack_or_nack(g_sock, g_interface, g_server_mac, header, ERROR);
                    }
                    break;
                }
                case 9: {
                    // Fim do arquivo
                    log_info("End of file received.\n");
                    if (curr != NULL) {
                        fclose(curr);
                        curr = NULL;
                    }
                    curr_tam = 0;
                    state = STATE_PLAYING;
                    send_ack_or_nack(g_sock, g_interface, g_server_mac, header, ACK);

                    // Open file
                    printf("Opening file: %s\n", curr_filename);
                    if (strlen(curr_filename) > 0) {
                        char cmd[512];
                        snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" &", curr_filename);
                        system(cmd);
                        // exit(1);
                    }
                    break;
                }
                case 15: {
                    // Erro
                    log_err("Received error message.\n");
                    break;
                }
                default: {
                    log_err("Unknown message type (DATA_TRANSFER): %u\n", type);
                    break;
                }
            }
            break;

        // --- Catch unknown state ---
        default:
            log_err("Unknown state: %d\n", state);
            break;
    }
}

void client(){
    // Initializes game logic
    Position* player_pos = initialize_player();
    char** grid = initialize_grid(player_pos);
    
    // Initializes receive buffer
    // initialize_receive_buffer();

    int input;
    while (1){
        input = getch_timeout(100000); // 100ms

        // If server has anything to say, if there's not, we go back to the game ( timeout )
        listen_to_server(g_sock, g_interface, g_server_mac, grid, player_pos);

        // Runs Game logic
        print_grid(grid);

        if (input == 'q') break;
        if (input == -1) continue; // timeout de input, volta para o loop

        if (!curr){
        switch (input) {
            case 'w':
                move_up(grid, player_pos);
                break;
            case 'a':
                move_left(grid, player_pos);
                break;
            case 's':
                move_down(grid, player_pos);
                break;
            case 'd':
                move_right(grid, player_pos);
                break;
            default:
                break;
        }
        }
    }
}

// kermit_protocol_header* send_ack_or_nack_until_response(const char* type, kermit_protocol_header* received){
//     send_ack_or_nack(g_sock, g_interface, g_server_mac, received, type);

//     while (1) {
//         receive_and_buffer_packet();

//         kermit_protocol_header* temp = get_first_in_line_receive_buffer();
//         if (temp == NULL) {
//             return NULL;
//         }

//         int tempSequence = convert_binary_to_decimal(temp->sequence, SEQUENCE_SIZE);
//         int receivedSequence = convert_binary_to_decimal(received->sequence, SEQUENCE_SIZE);
//         printf("Comparing sequences: temp=%d, received=%d\n", tempSequence, receivedSequence);

//         if ((strncmp((char*)temp->sequence, (char*)received->sequence, SEQUENCE_SIZE) == 0)) {
//             send_ack_or_nack(g_sock, g_interface, g_server_mac, received, type);
//             destroy_header(temp);
//             continue;
//         }

//         return temp;
//     }
// }

void send_mac_address(){
    /* Send client MAC as response */
    kermit_protocol_header* header = create_header((unsigned char*)"0001010", (unsigned char*)MAC, self_mac);
    char* message = (char*)generate_message(header);
    unsigned int message_size = getHeaderSize(header);

    if (send_package_until_ack(g_sock, g_interface, g_server_mac, (const unsigned char*)message, message_size, header) != 0) {
        log_info("Client MAC sent successfully");
    } else {
        log_err("Failed to send client MAC");
    }

    free(message);
    destroy_header(header);
}

void wait_process_broadcast_message(){
    initialize_receive_buffer();

    while (1) {
        log_info("Waiting for MAC response from server...");

        receive_and_buffer_packet();

        kermit_protocol_header* temp = get_first_in_line_receive_buffer();
        if (temp != NULL) {
            if (strncmp((char*)temp->type, MAC, TYPE_SIZE) == 0) {
                memcpy(g_server_mac, temp->data, 6);
                log_info("Received MAC response");

                send_ack_or_nack(g_sock, g_interface, g_server_mac, temp, ACK);
                send_mac_address();

                destroy_header(temp);
                break; 
            }

            destroy_header(temp);
        }
    }
}

void initialize_connection_context(char* interface, int port) {
    g_sock = create_raw_socket();
    bind_raw_socket(g_sock, interface, port);

    get_mac_address(g_sock, interface, self_mac);
    wait_process_broadcast_message(); 

    // g_interface = interface;
    // memcpy(g_server_mac, server_mac, 6);
}