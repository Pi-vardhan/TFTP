/*############################################################################################### 
 *
 *           Author         : Priyavardhan
 *           Date           : 06/10/2026 
 *           File           : server.c©
 *           Descriptions   : Server side of tftp protocol
 *           Objective      : Ready to recive/send data to client 
 *
###############################################################################################*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>

#include "file.h"
#include "udp.h"
#include "tftp.h"


void sigchld_handler(int signum)
{
    (void)signum;

    while (waitpid(-1, NULL, WNOHANG) > 0)
    {
    }
}

int is_same_client(const struct sockaddr_in *expected, const struct sockaddr_in *received)
{
    return expected->sin_addr.s_addr == received->sin_addr.s_addr && expected->sin_port == received->sin_port;
}

int main(void)
{
    signal(SIGCHLD, sigchld_handler);

    int server_sockfd;
    int bytes_received;
    char buffer[PACKETSIZE];

    struct sockaddr_in client_addr;
    packet_t packet;

    int next_port = 20001;
    server_sockfd = udp_bind_server(20000);

    if (server_sockfd < 0)
    {
        printf("Failed to start TFTP server\n");
        return 1;
    }
    printf("TFTP server started on port 20000\n");

    while (1)
    {
        int transfer_port;
        int transfer_sockfd;
        pid_t pid;

        bytes_received = udp_receive_packet(server_sockfd, buffer, sizeof(buffer), &client_addr);
        if (bytes_received < 0) continue;

        printf("\n================================\n");
        printf("Received %d bytes\n", bytes_received);

        if (packet_parse(buffer, bytes_received, &packet) < 0)
        {
            printf("Invalid TFTP packet\n");
            continue;
        }

        if (IS_RRQ(packet.opcode))
        {
            printf("RRQ received\n");
            printf("Filename: %s\n", packet.filename);
            printf("Mode: %s\n", packet.mode);
        }
        else if (IS_WRQ(packet.opcode))
        {
            printf("WRQ received\n");
            printf("Filename: %s\n", packet.filename);
            printf("Mode: %s\n", packet.mode);
        }
        else
        {
            printf("Unsupported request opcode: %hu\n", packet.opcode);
            continue;
        }

        transfer_port = next_port++;
        printf("Allocating transfer port: %d\n", transfer_port);
        transfer_sockfd = udp_rebind_server(transfer_port);

        if (transfer_sockfd < 0)
        {
            printf("Failed to create transfer socket\n");
            continue;
        }
        printf("Transfer socket created on port %d\n", transfer_port);

        pid = fork();
        if (pid < 0)
        {
            perror("fork");
            close(transfer_sockfd);
            continue;
        }

        if (pid > 0)
        {
            printf("Parent: created child PID %d\n", pid);
            close(transfer_sockfd);
            continue;
        }
        printf("Child: handling transfer on port %d\n", transfer_port);
        close(server_sockfd);

        // WRQ / PUT
        if (IS_WRQ(packet.opcode))
        {
            int fd;
            int bytes_received;
            char data_buffer[PACKETSIZE];
            bnum_t expected_block = 1;

            fd = file_open_write(packet.filename);
            if (fd < 0)
            {
                printf("Failed to open file for writing: %s\n", packet.filename);
                close(transfer_sockfd);
                exit(1);
            }

            // Send ACK 0
            {
                packetbuffer_t *ack_packet;
                int ack_size;
                ack_packet = packet_form_ack(0, &ack_size);

                if (ack_packet == NULL)
                {
                    close(fd);
                    close(transfer_sockfd);
                    exit(1);
                }

                if (udp_send_packet(transfer_sockfd, ack_packet, ack_size, &client_addr) < 0)
                {
                    free(ack_packet);
                    close(fd);
                    close(transfer_sockfd);
                    exit(1);
                }

                printf("ACK block 0 sent successfully\n");
                free(ack_packet);
            }

            // Receive DATA blocks
            while (1)
            {
                packet_t data_packet;
                {
                    struct sockaddr_in received_addr;
                    bytes_received = udp_receive_packet(transfer_sockfd, data_buffer, sizeof(data_buffer), &received_addr);

                    if (bytes_received < 0)
                    {
                        printf("Failed to receive DATA\n");
                        close(fd);
                        close(transfer_sockfd);
                        exit(1);
                    }

                    if (!is_same_client(&client_addr, &received_addr))
                    {
                        printf("Packet received from unknown client\n");
                        continue;
                    }
                }

                if (packet_parse(data_buffer, bytes_received, &data_packet) < 0)
                {
                    printf("Invalid DATA packet\n");
                    close(fd);
                    close(transfer_sockfd);
                    exit(1);
                }

                if (!IS_DATA(data_packet.opcode))
                {
                    printf("Expected DATA packet\n");
                    close(fd);
                    close(transfer_sockfd);
                    exit(1);
                }

                printf("Received DATA block %hu\n", data_packet.blocknum);
                printf("Data length: %d\n", data_packet.data_length);

                if (data_packet.blocknum < expected_block)
                {
                    printf("Duplicate DATA block %hu received\n", data_packet.blocknum);
                    printf("Re-sending ACK block %hu\n", data_packet.blocknum);
                    {
                        packetbuffer_t *ack_packet;
                        int ack_size;
                        ack_packet = packet_form_ack(data_packet.blocknum, &ack_size);

                        if (ack_packet == NULL)
                        {
                            close(fd);
                            close(transfer_sockfd);
                            exit(1);
                        }

                        if (udp_send_packet(transfer_sockfd, ack_packet, ack_size, &client_addr) < 0)
                        {
                            free(ack_packet);
                            close(fd);
                            close(transfer_sockfd);
                            exit(1);
                        }
                        free(ack_packet);
                    }
                    continue;
                }

                if (data_packet.blocknum > expected_block)
                {
                    printf("Unexpected DATA block\n");
                    printf("Expected: %hu\n", expected_block);
                    printf("Received: %hu\n", data_packet.blocknum);

                    close(fd);
                    close(transfer_sockfd);
                    exit(1);
                }
                if (file_append_from_buffer(fd, data_packet.data, data_packet.data_length) < 0)
                {
                    printf("Failed to write DATA block %hu\n", data_packet.blocknum);
                    close(fd);
                    close(transfer_sockfd);
                    exit(1);
                }
                printf("DATA block %hu written successfully\n", data_packet.blocknum);

                // Send ACK
                {
                    packetbuffer_t *ack_packet;
                    int ack_size;
                    ack_packet = packet_form_ack(data_packet.blocknum, &ack_size);

                    if (ack_packet == NULL)
                    {
                        close(fd);
                        close(transfer_sockfd);
                        exit(1);
                    }

                    if (udp_send_packet(transfer_sockfd, ack_packet, ack_size, &client_addr) < 0)
                    {
                        free(ack_packet);
                        close(fd);
                        close(transfer_sockfd);
                        exit(1);
                    }
                    printf("ACK block %hu sent successfully\n", data_packet.blocknum);
                    free(ack_packet);
                }

                // Final DATA block 
                if (data_packet.data_length < BLIMIT)
                {
                    printf("Upload complete!\n");
                    break;
                }
                expected_block++;
            }
            close(fd);
            close(transfer_sockfd);
            exit(0);
        }

        // RRQ / GET
        if (IS_RRQ(packet.opcode))
        {
            int fd;
            bnum_t blocknum = 1;
            int position = 0;

            char file_buffer[BLIMIT];
            char ack_buffer[PACKETSIZE];

            fd = file_open_read(packet.filename);

            if (fd < 0)
            {
                packetbuffer_t *error_packet;
                int error_size;

                printf("File not found: %s\n",packet.filename);
                error_packet = packet_form_error(ECODE_1, ESTRING_1, &error_size);

                if (error_packet == NULL)
                {
                    close(transfer_sockfd);
                    exit(1);
                }

                if (udp_send_packet(transfer_sockfd, error_packet, error_size, &client_addr) < 0)
                {
                    printf("Failed to send ERROR packet\n");
                    free(error_packet);
                    close(transfer_sockfd);
                    exit(1);
                }

                printf("ERROR packet sent successfully\n");
                free(error_packet);
                close(transfer_sockfd);
                exit(1);
            }
            printf("File opened successfully\n");

            while (1)
            {
                int bytes_read;
                int packet_size;
                int ack_bytes;

                packet_t ack_packet;
                packetbuffer_t *data_packet;

                printf("\n-----------------------------\n");
                printf("Reading block %hu\n", blocknum);
                printf("File position: %d\n", position);

                bytes_read = file_buffer_from_pos(fd, file_buffer, position);

                if (bytes_read < 0)
                {
                    printf("Failed to read file\n");
                    close(fd);
                    close(transfer_sockfd);
                    exit(1);
                }

                printf("Bytes read: %d\n", bytes_read);
                data_packet = packet_form_data(file_buffer, bytes_read, blocknum, &packet_size);

                if (data_packet == NULL)
                {
                    printf("Failed to form DATA packet\n");
                    close(fd);
                    close(transfer_sockfd);
                    exit(1);
                }
                printf("DATA packet size: %d\n", packet_size);

                if (udp_send_packet(transfer_sockfd, data_packet, packet_size, &client_addr) < 0)
                {
                    printf("Failed to send DATA block %hu\n", blocknum);
                    free(data_packet);
                    close(fd);
                    close(transfer_sockfd);
                    exit(1);
                }

                printf("DATA block %hu sent successfully\n", blocknum);
                {
                    int retry_count = 0;
                    while (retry_count < TIMEOUT_LIMIT)
                    {
                        {
                            struct sockaddr_in received_addr;
                            ack_bytes = udp_receive_packet_timeout(transfer_sockfd, ack_buffer, sizeof(ack_buffer), &received_addr, TIMEOUT);

                            if (ack_bytes > 0)
                            {
                                if (!is_same_client(&client_addr, &received_addr))
                                {
                                    printf("ACK received from unknown client\n");
                                    ack_bytes = 0;
                                    continue;
                                }
                                break;
                            }
                        }

                        if (ack_bytes < 0)
                        {
                            printf("Error while waiting for ACK\n");
                            free(data_packet);
                            close(fd);
                            close(transfer_sockfd);
                            exit(1);
                        }
                        retry_count++;

                        printf("Timeout waiting for ACK %hu\n", blocknum);
                        printf("Retransmitting DATA block %hu " "(%d/%d)\n", blocknum, retry_count, TIMEOUT_LIMIT);

                        if (udp_send_packet(transfer_sockfd, data_packet, packet_size, &client_addr) < 0)
                        {
                            printf("Failed to retransmit " "DATA block %hu\n", blocknum);
                            free(data_packet);
                            close(fd);
                            close(transfer_sockfd);
                            exit(1);
                        }
                    }

                    if (ack_bytes == 0)
                    {
                        printf("ACK timeout limit reached " "for block %hu\n", blocknum);
                        free(data_packet);
                        close(fd);
                        close(transfer_sockfd);
                        exit(1);
                    }
                }

                if (packet_parse(ack_buffer, ack_bytes, &ack_packet) < 0)
                {
                    printf("Invalid ACK packet\n");
                    free(data_packet);
                    close(fd);
                    close(transfer_sockfd);
                    exit(1);
                }

                if (!IS_ACK(ack_packet.opcode))
                {
                    printf("Expected ACK packet\n");
                    free(data_packet);
                    close(fd);
                    close(transfer_sockfd);
                    exit(1);
                }
                printf("ACK block: %hu\n", ack_packet.blocknum);

                if (ack_packet.blocknum != blocknum)
                {
                    printf("Unexpected ACK block number\n");
                    printf("Expected: %hu\n", blocknum);
                    printf("Received: %hu\n", ack_packet.blocknum);
                    free(data_packet);
                    close(fd);
                    close(transfer_sockfd);
                    exit(1);
                }

                printf("ACK block %hu verified successfully\n", blocknum);
                free(data_packet);

                if (bytes_read < BLIMIT)
                {
                    printf("\nTransfer complete!\n");
                    break;
                }

                position += bytes_read;
                blocknum++;
            }
            close(fd);
        }
        close(transfer_sockfd);
        exit(0);
    }

    close(server_sockfd);
    return 0;
}