/*############################################################################################### 
 *
 *           Author         : Priyavardhan
 *           Date           : 06/10/2026
 *           File           : file.c©
 *           Descriptions   : All file operations
 *           Objective      : To read/write from/to file in both server and client side
 *
###############################################################################################*/

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "tftp.h"

#include "file.h"

int file_open_read(const char *filename)
{
    int fd;
    fd = open(filename, O_RDONLY);

    if (fd < 0)
    {
        perror("open");
        return -1;
    }

    return fd;
}

int file_open_write(const char *filename)
{
    int fd;

    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd < 0)
    {
        perror("open");
        return -1;
    }

    return fd;
}

int file_append_from_buffer(int fd, const char *buffer, int data_length)
{
    int bytes_written;

    bytes_written = write(fd, buffer, data_length);

    if (bytes_written < 0)
    {
        perror("write");
        return -1;
    }

    return bytes_written;
}

int file_buffer_from_pos(int fd, char *buffer, int position)
{
    int bytes_read;

    if (lseek(fd, position, SEEK_SET) < 0)
    {
        perror("lseek");
        return -1;
    }

    bytes_read = read(fd, buffer, BLIMIT);

    if (bytes_read < 0)
    {
        perror("read");
        return -1;
    }

    return bytes_read;
}