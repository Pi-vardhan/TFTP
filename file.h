
#ifndef TFTP_FILE
#define TFTP_FILE
#include "tftp.h"
/* Function design for file operation functions */
int file_open_read(const char *filename);
int file_buffer_from_pos(int fd, char *buffer, int position);
int file_open_write(const char *filename);
int file_append_from_buffer(int fd, const char *buffer, int data_length);
int file_close();

#endif
