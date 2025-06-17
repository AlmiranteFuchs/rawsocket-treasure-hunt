#include "receiveBuffer.h"

// Global received packet buffer
kermit_protocol_header **global_receive_buffer = NULL;
// The next expected packet sequence number
extern unsigned int expected_sequence;

unsigned int initialize_receive_buffer() {
  global_receive_buffer =
      malloc(sizeof(kermit_protocol_header *) * (2 << SEQUENCE_SIZE));
  if (global_receive_buffer == NULL) {
    fprintf(stderr, "Failed to allocate memory for receive buffer\n");
    return 0;
  }

  return 1;
}

unsigned int is_header_on_receive_buffer(kermit_protocol_header *header) {
  if (global_receive_buffer == NULL || global_receive_buffer[0] == NULL) {
    return 0; // Buffer is empty
  }

  unsigned int seq = convert_binary_to_decimal(header->sequence, SEQUENCE_SIZE);
  unsigned int type = convert_binary_to_decimal(header->type, TYPE_SIZE);

  for (unsigned int i = 0; global_receive_buffer[i] != NULL; i++) {
    unsigned int temp_seq = convert_binary_to_decimal(
        global_receive_buffer[i]->sequence, SEQUENCE_SIZE);
    unsigned int temp_type =
        convert_binary_to_decimal(global_receive_buffer[i]->type, TYPE_SIZE);

    if (temp_seq == seq && temp_type == type) {
      return 1; // Header found in the buffer
    }
  }

  return 0;
}

void insert_in_i_receive_buffer(kermit_protocol_header *header,
                                unsigned int index) {
  // Shift the headers to the right to make space for the new header
  int cpy = index;
  while (global_receive_buffer[cpy] != NULL && cpy < RECEIVE_BUFFER_SIZE - 1) {
    global_receive_buffer[cpy + 1] = global_receive_buffer[cpy];
    cpy++;
  }

  // Insert the new header at the specified index
  global_receive_buffer[index] = header;
}

unsigned int update_receive_buffer(kermit_protocol_header *header) {
  if (global_receive_buffer == NULL) {
    fprintf(stderr, "Receive buffer is not initialized\n");
    return 0;
  }

  if (is_header_on_receive_buffer(header)) {
    return 1; // Cabeçalho já existe
  }

  // Converte sequência e tipo para decimal para comparação
  unsigned int seq = convert_binary_to_decimal(header->sequence, SEQUENCE_SIZE);

  // Encontra a posição correta para inserir o cabeçalho
  unsigned int i = 0;
  while (global_receive_buffer[i] != NULL && i < RECEIVE_BUFFER_SIZE) {
    unsigned int temp_seq = convert_binary_to_decimal(
        global_receive_buffer[i]->sequence, SEQUENCE_SIZE);

    // Verifica se o cabeçalho atual é maior que o cabeçalho no buffer
    if (!checkIfNumberIsBigger(seq, temp_seq)) {
      insert_in_i_receive_buffer(header, i);
      return 1;
    }

    i++;
  }

  if (i >= RECEIVE_BUFFER_SIZE) {
    fprintf(stderr, "Receive buffer is full, cannot insert new header\n");
    return 0; // Buffer cheio
  }

  // Se nenhuma posição foi encontrada, adiciona o cabeçalho no final do buffer
  global_receive_buffer[i] = header;

  return 1;
}

kermit_protocol_header *get_first_in_line_receive_buffer() {
  if (global_receive_buffer == NULL || global_receive_buffer[0] == NULL) {
    return NULL; // Buffer vazio
  }

  kermit_protocol_header *first_header = global_receive_buffer[0];
//   print_header(first_header);

  // Shift para a esquerda
  unsigned int i = 0;
  while (global_receive_buffer[i + 1] != NULL && i < RECEIVE_BUFFER_SIZE - 1) {
    global_receive_buffer[i] = global_receive_buffer[i + 1];
    i++;
  }
  global_receive_buffer[i] = NULL;

  return first_header;
}

void print_receive_buffer() {
  if (global_receive_buffer == NULL) {
    printf("Receive buffer is not initialized.\n");
    return;
  }

  printf("Receive Buffer:\n");
  for (unsigned int i = 0; global_receive_buffer[i] != NULL; i++) {
    kermit_protocol_header *header = global_receive_buffer[i];
    printf("Header %u:\n", i);
    // printf("  Start: %.*s\n", START_SIZE, header->start);
    // printf("  Size: %.*s\n", SIZE_SIZE, header->size);
    printf("  Sequence: %.*s\n", SEQUENCE_SIZE, header->sequence);
    printf("  Type: %.*s\n", TYPE_SIZE, header->type);
    // printf("  Checksum: %.*s\n", CHECKSUM_SIZE, header->checksum);
    // if (header->data) {
    //     printf("  Data: %s\n", header->data);
    // } else {
    //     printf("  Data: NULL\n");
    // }
  }
}