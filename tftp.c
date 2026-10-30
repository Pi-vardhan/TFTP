#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "tftp.h"
#include "file.h"

static packetbuffer_t *packet_form_request(opcode_t opcode, const char *filename, const char *mode, int *packet_size)
{
    packetbuffer_t *packet;
    size_t filename_len, mode_len, size;
    unsigned short net_opcode;

    if (!filename || !mode || !packet_size)
        return NULL;

    filename_len = strlen(filename);
    mode_len = strlen(mode);
    size = sizeof(opcode_t) + filename_len + 1 + mode_len + 1;

    if (filename_len == 0 || mode_len == 0 || size > PACKETSIZE)
        return NULL;

    packet = malloc(size);
    if (!packet)
    {
        perror("malloc");
        return NULL;
    }

    net_opcode = htons(opcode);
    memcpy(packet, &net_opcode, sizeof(net_opcode));
    memcpy(packet + sizeof(net_opcode), filename, filename_len + 1);
    memcpy(packet + sizeof(net_opcode) + filename_len + 1, mode, mode_len + 1);

    *packet_size = (int)size;
    return packet;
}

packetbuffer_t *packet_form_rrq(const char *filename, const char *mode, int *packet_size)
{
    return packet_form_request(OPCODE_RRQ, filename, mode, packet_size);
}

packetbuffer_t *packet_form_wrq(const char *filename, const char *mode, int *packet_size)
{
    return packet_form_request(OPCODE_WRQ, filename, mode, packet_size);
}

void packet_extract_opcode(packetbuffer_t *buffer, packet_t *parsed_packet)
{
    opcode_t opcode;

    if (!buffer || !parsed_packet)
        return;

    memcpy(&opcode, buffer, sizeof(opcode));
    parsed_packet->opcode = ntohs(opcode);
}

int packet_parse_rq(packetbuffer_t *buffer, int packet_size, packet_t *packet)
{
    char *filename_end, *mode_end;
    size_t filename_len, mode_len;
    int offset, remaining;

    if (!buffer || !packet || packet_size < 4 || packet_size > PACKETSIZE)
        return -1;

    offset = sizeof(opcode_t);
    remaining = packet_size - offset;

    filename_end = memchr(buffer + offset, '\0', remaining);
    if (!filename_end || filename_end == buffer + offset)
        return -1;

    filename_len = (size_t)(filename_end - (buffer + offset));
    if (filename_len >= sizeof(packet->filename))
        return -1;

    offset += (int)filename_len + 1;
    remaining = packet_size - offset;

    if (remaining <= 1)
        return -1;

    mode_end = memchr(buffer + offset, '\0', remaining);
    if (!mode_end || mode_end == buffer + offset)
        return -1;

    mode_len = (size_t)(mode_end - (buffer + offset));
    if (mode_len >= sizeof(packet->mode) || mode_end != buffer + packet_size - 1)
        return -1;

    memcpy(packet->filename, buffer + sizeof(opcode_t), filename_len);
    packet->filename[filename_len] = '\0';
    memcpy(packet->mode, buffer + offset, mode_len);
    packet->mode[mode_len] = '\0';

    return 0;
}

void packet_parse_data(packetbuffer_t *buffer, int packet_size, packet_t *packet)
{
    unsigned short block;

    memcpy(&block, buffer + sizeof(opcode_t), sizeof(block));
    packet->blocknum = ntohs(block);
    packet->data_length = packet_size - 2 * sizeof(opcode_t);

    if (packet->data_length > 0)
        memcpy(packet->data, buffer + 2 * sizeof(opcode_t), packet->data_length);
}

void packet_parse_ack(packetbuffer_t *buffer, packet_t *packet)
{
    unsigned short block;

    memcpy(&block, buffer + sizeof(opcode_t), sizeof(block));
    packet->blocknum = ntohs(block);
}

int packet_parse_error(packetbuffer_t *buffer, int packet_size, packet_t *packet)
{
    unsigned short error_code;
    char *message, *end;
    size_t length;

    if (!buffer || !packet || packet_size < 5 || packet_size > PACKETSIZE)
        return -1;

    memcpy(&error_code, buffer + sizeof(opcode_t), sizeof(error_code));
    packet->ecode = ntohs(error_code);

    message = buffer + 2 * sizeof(opcode_t);
    length = (size_t)(packet_size - 2 * sizeof(opcode_t));
    end = memchr(message, '\0', length);

    if (!end || end != buffer + packet_size - 1)
        return -1;

    length = (size_t)(end - message);
    if (length >= sizeof(packet->estring))
        return -1;

    memcpy(packet->estring, message, length);
    packet->estring[length] = '\0';
    packet->estring_length = (int)length;

    return 0;
}

int packet_parse(packetbuffer_t *buffer, int packet_size, packet_t *packet)
{
    if (!buffer || !packet || packet_size < (int)sizeof(opcode_t) || packet_size > PACKETSIZE)
        return -1;

    packet_extract_opcode(buffer, packet);

    switch (packet->opcode)
    {
        case OPCODE_RRQ:
        case OPCODE_WRQ:
            return packet_parse_rq(buffer, packet_size, packet);

        case OPCODE_DATA:
            if (packet_size < 4 || packet_size > PACKETSIZE)
                return -1;
            packet_parse_data(buffer, packet_size, packet);
            return 0;

        case OPCODE_ACK:
            if (packet_size != 4)
                return -1;
            packet_parse_ack(buffer, packet);
            return 0;

        case OPCODE_ERROR:
            return packet_parse_error(buffer, packet_size, packet);

        default:
            return -1;
    }
}

packetbuffer_t *packet_form_data(const char *data, int data_length, bnum_t blocknum, int *packet_size)
{
    packetbuffer_t *packet;
    unsigned short opcode, block;

    if (!packet_size || data_length < 0 || data_length > BLIMIT || (data_length > 0 && !data))
        return NULL;

    packet = malloc(4 + data_length);
    if (!packet)
    {
        perror("malloc");
        return NULL;
    }

    opcode = htons(OPCODE_DATA);
    block = htons(blocknum);
    memcpy(packet, &opcode, sizeof(opcode));
    memcpy(packet + sizeof(opcode), &block, sizeof(block));

    if (data_length > 0)
        memcpy(packet + 4, data, data_length);

    *packet_size = 4 + data_length;
    return packet;
}

packetbuffer_t *packet_form_ack(bnum_t blocknum, int *packet_size)
{
    packetbuffer_t *packet;
    unsigned short opcode, block;

    if (!packet_size)
        return NULL;

    packet = malloc(4);
    if (!packet)
    {
        perror("malloc");
        return NULL;
    }

    opcode = htons(OPCODE_ACK);
    block = htons(blocknum);
    memcpy(packet, &opcode, sizeof(opcode));
    memcpy(packet + sizeof(opcode), &block, sizeof(block));

    *packet_size = 4;
    return packet;
}

packetbuffer_t *packet_form_error(ecode_t ecode, const char *estring, int *packet_size)
{
    packetbuffer_t *packet;
    unsigned short opcode, error_code;
    size_t message_len, size;

    if (!estring || !packet_size)
        return NULL;

    message_len = strlen(estring);
    size = 2 * sizeof(opcode_t) + message_len + 1;

    if (size > PACKETSIZE)
        return NULL;

    packet = malloc(size);
    if (!packet)
    {
        perror("malloc");
        return NULL;
    }

    opcode = htons(OPCODE_ERROR);
    error_code = htons(ecode);
    memcpy(packet, &opcode, sizeof(opcode));
    memcpy(packet + sizeof(opcode), &error_code, sizeof(error_code));
    memcpy(packet + 2 * sizeof(opcode_t), estring, message_len + 1);

    *packet_size = (int)size;
    return packet;
}