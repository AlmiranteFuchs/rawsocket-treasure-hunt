#ifndef RECEIVE_BUFFER_H
#define RECEIVE_BUFFER_H

#include "socket.h"
#define RECEIVE_BUFFER_SIZE (2 << SEQUENCE_SIZE)
// #define RECEIVE_BUFFER_SIZE 16

unsigned int initialize_receive_buffer();
unsigned int is_header_on_receive_buffer(kermit_protocol_header* header);
unsigned int update_receive_buffer(kermit_protocol_header* header);
kermit_protocol_header* get_first_in_line_receive_buffer();
void print_receive_buffer();

#endif