/*############################################################################################### 
 *
 *           Author         : Priyavardhan
 *           Date           : 06/10/2026 
 *           File           : client.c©
 *           Descriptions   : Client side of tftp protocol
 *           Objective      : To send/recive file to/from server
 *
###############################################################################################*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "udp.h"
#include "tftp.h"

/* Client code for tftp Protocol */

#define DEFAULT_PORT 20000

int is_same_server(struct sockaddr_in *expected, struct sockaddr_in *received)
{
    return expected->sin_addr.s_addr == received->sin_addr.s_addr && expected->sin_port == received->sin_port;
}

static int connect_to_server(const char *ip, int port, int *client_sockfd, int *connected, struct sockaddr_in *server_addr)
{
    struct sockaddr_in new_server_addr;
    int new_sockfd;

    memset(&new_server_addr, 0, sizeof(new_server_addr));
    new_server_addr.sin_family = AF_INET;
    new_server_addr.sin_port = htons((unsigned short)port);

    if (inet_pton(AF_INET, ip, &new_server_addr.sin_addr) != 1)
    {
        printf("Invalid server IP address\n");
        return -1;
    }

    new_sockfd = udp_bind_client();
    if (new_sockfd < 0)
    {
        printf("Failed to create client socket\n");
        return -1;
    }

    if (*client_sockfd >= 0)	
    	close(*client_sockfd);

    *client_sockfd = new_sockfd;
    *server_addr = new_server_addr;
    *connected = 1;

    printf("Connected to %s:%d\n", ip, port);
    return 0;
}

int main(int argc, char *argv[])
{
    char command[100];
    int client_sockfd = -1;
    int connected = 0;
    struct sockaddr_in server_addr;
    char transfer_mode[20] = MODE_OCTET;

    if (argc > 2)
    {
        fprintf(stderr, "Usage: %s [server_ip]\n", argv[0]);
        return 1;
    }

    printf("TFTP Client\n");
    printf("Type 'help' for available commands.\n");

    if (argc == 2 && connect_to_server(argv[1], DEFAULT_PORT, &client_sockfd, &connected, &server_addr) < 0)	return 1;

    while (1)
    {
        printf("tftp> ");
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL)		break;

        command[strcspn(command, "\n")] = '\0';

        if (strcmp(command, "bye") == 0 || strcmp(command, "quit") == 0)
        {
            printf("Exiting TFTP client...\n");
            break;
        }

        if (strcmp(command, "help") == 0)
        {
            printf("\nAvailable commands:\n");
			printf("  help                   - Display available commands\n");
			printf("  connect <server_ip>    - Connect to a TFTP server\n");
			printf("  get <filename>         - Download a file from the server\n");
			printf("  put <filename>         - Upload a local file to the server\n");
			printf("  mode <octet|netascii>  - Set the file transfer mode\n");
			printf("  bye                    - Disconnect and exit the client\n");
			printf("  quit                   - Exit the client\n\n");

            continue;
        }

        if (strcmp(command, "connect") == 0)
		{
		    printf("Usage: connect <server_ip>\n");
		    continue;
		}
        if (strncmp(command, "connect ", 8) == 0)
        {
            char ip[INET_ADDRSTRLEN];
            char extra;
            int parsed = sscanf(command + 8, "%15s %c", ip, &extra);

            if (parsed != 1)
            {
                printf("Usage: connect <server_ip>\n");
                continue;
            }

            connect_to_server(ip, DEFAULT_PORT, &client_sockfd, &connected, &server_addr);
            continue;
        }

        if (strcmp(command, "get") == 0)
		{
		    printf("Usage: get <filename>\n");
		    continue;
		}
        if (strncmp(command, "get ", 4) == 0)
		{
		    char filename[100];

		    int packet_size;
		    int bytes_received;

		    int fd = -1;
		    int position = 0;
			int download_success = 0;
			int temp_created = 0;
            char temp_filename[120];

		    bnum_t expected_block = 1;
		    packetbuffer_t *rrq_packet;
		    packet_t received_packet;

		    char buffer[PACKETSIZE];

		    struct sockaddr_in response_addr;
		    struct sockaddr_in transfer_addr;
			int transfer_addr_set = 0;

		    if (!connected)
		    {
		        printf("Not connected to a server\n");
		        continue;
		    }

		    if (sscanf(command + 4, "%99s",filename) != 1)
		    {
		        printf("Usage: get <filename>\n");
		        continue;
		    }

			//Download to a temporary file. The requested filename is replaced only after the transfer completes.		
			if (snprintf(temp_filename, sizeof(temp_filename), "%s.tftp-part", filename) >= (int)sizeof(temp_filename))
			{
				printf("Filename is too long\n");
				continue;
			}

		    //Form RRQ packet
		    rrq_packet = packet_form_rrq(filename, transfer_mode, &packet_size);

		    if (rrq_packet == NULL)
		    {
		        if (fd >= 0) { close(fd); fd = -1; }
		        continue;
		    }

		    printf("Sending RRQ for %s...\n", filename);

		    if (udp_send_packet(client_sockfd, rrq_packet, packet_size, &server_addr) < 0)
		    {
		        printf("Failed to send RRQ\n");
		        free(rrq_packet);
		        if (fd >= 0) { close(fd); fd = -1; }	continue;
		    }

		    free(rrq_packet);
		    printf("RRQ sent successfully\n");

		    //Receive DATA blocks
		    while (1)
		    {
		        bytes_received =
		            udp_receive_packet_timeout(client_sockfd, buffer, sizeof(buffer), &response_addr, TIMEOUT);

		        if (bytes_received > 0 && !transfer_addr_set)
				{
				    transfer_addr = response_addr;
				    transfer_addr_set = 1;
				}

		        if (bytes_received < 0)
		        {
		            printf("Error while waiting for DATA\n");
		            if (fd >= 0) { close(fd); fd = -1; }
		            break;
		        }

		        if (bytes_received == 0)
		        {
		            printf("Timeout waiting for DATA block %hu\n", expected_block);
		            // Server performs retransmission when
		            // ACK is not received, so keep waiting.
		            continue;
		        }

		        if (packet_parse(buffer, bytes_received, &received_packet) < 0)
		        {
		            printf("Invalid response packet\n");
		            if (fd >= 0) { close(fd); fd = -1; }	
		            break;
		        }

		        //ERROR packet
		        if (IS_ERROR(received_packet.opcode))
		        {
		            printf("\n================================\n");
		            printf("ERROR %hu: %s\n", received_packet.ecode, received_packet.estring);
		            printf("================================\n");

		            if (fd >= 0) { close(fd); fd = -1; }	break;
		        }

		        //We expect DATA
		        if (!IS_DATA(received_packet.opcode))
		        {
		            printf("Expected DATA packet\n");
		            if (fd >= 0) { close(fd); fd = -1; }	break;
		        }

		        printf("\n-----------------------------\n");
		        printf("Received DATA block %hu\n", received_packet.blocknum);
				printf("Data length: %d\n", received_packet.data_length);

		        //Duplicate DATA block
		        if (received_packet.blocknum < expected_block)
		        {
		            packetbuffer_t *ack_packet;
		            int ack_size;

		            printf("Duplicate DATA block %hu received\n", received_packet.blocknum);
		            ack_packet = packet_form_ack(received_packet.blocknum, &ack_size);

		            if (ack_packet == NULL)
		            {
		                if (fd >= 0) { close(fd); fd = -1; }	
		            	break;
		            }

		            printf("Re-sending ACK block %hu\n", received_packet.blocknum);
		            udp_send_packet(client_sockfd, ack_packet, ack_size, &transfer_addr);
		            free(ack_packet);
		            continue;
		        }

		        //Unexpected future block
		        if (received_packet.blocknum > expected_block)
		        {
		            printf("Unexpected DATA block\n");
		            printf("Expected: %hu\n", expected_block);
		            printf("Received: %hu\n", received_packet.blocknum);

		            if (fd >= 0) { close(fd); fd = -1; }
		            break;
		        }

		         //Create the temporary destination only after a valid DATA
		         //packet arrives. An RRQ ERROR therefore creates no local file.
				if (fd < 0)
                    {
                        fd = open(temp_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                        if (fd < 0)
                        {
                            perror("open temporary download file");
                            break;
                        }
                        temp_created = 1;
                    }	

		        //Write DATA to file
		        if (write(fd, received_packet.data, received_packet.data_length) < received_packet.data_length)
		        {
		            perror("write");
		            if (fd >= 0) { close(fd); fd = -1; }	break;
		        }

		        printf("DATA block %hu written successfully\n", received_packet.blocknum);

		        // Send ACK
		        {
		            packetbuffer_t *ack_packet;
		            int ack_size;
		            ack_packet = packet_form_ack(received_packet.blocknum, &ack_size);

		            if (ack_packet == NULL)
		            {
		                if (fd >= 0) { close(fd); fd = -1; }	
		            	break;
		        	}

		            if (udp_send_packet(client_sockfd, ack_packet, ack_size, &transfer_addr) < 0)
		            {
		                printf("Failed to send ACK block %hu\n", received_packet.blocknum);
		                free(ack_packet);
		                if (fd >= 0) { close(fd); fd = -1; }	
		                break;
		            }

		            printf("ACK block %hu sent successfully\n", received_packet.blocknum);
		            free(ack_packet);
		        }

		        //Final DATA packet
		        if (received_packet.data_length < BLIMIT)
		        {
                    download_success = 1;
		            printf("\n================================\n");
		            printf("Download complete!\n");
		            printf("================================\n");

		            if (fd >= 0) { close(fd); fd = -1; }	
		            break;
		        }

		        position += received_packet.data_length;
		        printf("File position: %d\n", position);
		        expected_block++;
		    }

                    if (fd >= 0)
                    {
                        close(fd);
                        fd = -1;
                    }

                    if (download_success && temp_created)
                    {
                        if (rename(temp_filename, filename) == 0)
                        {
                            printf("Downloaded file saved as %s\n", filename);
                        }
                        else
                        {
                            perror("rename downloaded file");
                            unlink(temp_filename);
                        }
                    }
                    else if (temp_created)
                    {
                        unlink(temp_filename);
                        printf("Download failed; incomplete file removed.\n");
                    }
		    continue;
		}

		if (strcmp(command, "put") == 0)
		{
		    printf("Usage: put <filename>\n");
		    continue;
		}
		if (strncmp(command, "put ", 4) == 0)
		{
		    char filename[100];

		    int packet_size;
		    int bytes_received;

		    int fd;
		    int position = 0;

		    bnum_t blocknum = 1;

		    char file_buffer[BLIMIT];
		    char buffer[PACKETSIZE];

		    packetbuffer_t *wrq_packet;
		    packet_t received_packet;

		    struct sockaddr_in response_addr;
		    struct sockaddr_in transfer_addr;

		    if (!connected)
		    {
		        printf("Not connected to a server\n");
		        continue;
		    }

		    if (sscanf(command + 4, "%99s", filename) != 1)
		    {
		        printf("Usage: put <filename>\n");
		        continue;
		    }

		    //Open local file
		    fd = open(filename, O_RDONLY);

		    if (fd < 0)
		    {
		        perror("open");
		        continue;
		    }

		    // Form WRQ packet
		    wrq_packet = packet_form_wrq(filename, transfer_mode, &packet_size);

		    if (wrq_packet == NULL)
		    {
		        close(fd);
		        continue;
		    }

		    printf("Sending WRQ for %s...\n", filename);

		    if (udp_send_packet(client_sockfd, wrq_packet, packet_size, &server_addr) < 0)
		    {
		        printf("Failed to send WRQ\n");
		        free(wrq_packet);
		        close(fd);
		        continue;
		    }
		    free(wrq_packet);
		    printf("WRQ sent successfully\n");

		    //Wait for ACK 0
		    bytes_received = udp_receive_packet_timeout(client_sockfd, buffer, sizeof(buffer),&response_addr, TIMEOUT);

		    if (bytes_received <= 0)
		    {
		        printf("Failed to receive ACK 0\n");
		        close(fd);
		        continue;
		    }

		    if (packet_parse(buffer, bytes_received, &received_packet) < 0)
		    {
		        printf("Invalid response packet\n");
		        close(fd);
		        continue;
		    }

		    //ERROR packet
		    if (IS_ERROR(received_packet.opcode))
		    {
		        printf("\n================================\n");
		        printf("ERROR %hu: %s\n", received_packet.ecode, received_packet.estring);
		        printf("================================\n");

		        close(fd);
		        continue;
		    }

		    //Verify ACK 0
		    if (!IS_ACK(received_packet.opcode) || received_packet.blocknum != 0)
			{
			    printf("Expected ACK block 0\n");
			    close(fd);
			    continue;
			}

			transfer_addr = response_addr;
			printf("ACK block 0 received\n");

		    //Send DATA blocks
		    while (1)
		    {
		        int bytes_read;
		        int data_size;

		        packetbuffer_t *data_packet;

		        //Read next block
		        bytes_read = pread(fd, file_buffer, BLIMIT, position);
		        if (bytes_read < 0)
		        {
		            perror("read");
		            close(fd);
		            break;
		        }

		        printf("\n-----------------------------\n");
		        printf("Sending block %hu\n", blocknum);
		        printf("File position: %d\n", position);
		        printf("Bytes read: %d\n", bytes_read);

		        //Form DATA packet
		        data_packet = packet_form_data(file_buffer, bytes_read, blocknum, &data_size);

		        if (data_packet == NULL)
		        {
		            printf("Failed to form DATA packet\n");

		            close(fd);
		            break;
		        }
				printf("DATA packet size: %d\n", data_size);

	        //Send DATA and wait for ACK
	        {
	            int retry_count = 0;
	            int ack_received = 0;

	            while (retry_count < TIMEOUT_LIMIT)
	            {
	                // Send DATA
	                if (udp_send_packet(client_sockfd, data_packet, data_size, &transfer_addr) < 0)
	                {
	                    printf("Failed to send DATA block %hu\n", blocknum);
	                    free(data_packet);
	                    close(fd);

	                    ack_received = -1;
	                    break;
	                }

	                if (retry_count == 0)
	                    printf("DATA block %hu sent successfully\n", blocknum);
	                else
	                    printf("Retransmitted DATA block %hu " "(%d/%d)\n", blocknum, retry_count, TIMEOUT_LIMIT);

	                // Wait for ACK
	                bytes_received = udp_receive_packet_timeout(client_sockfd,buffer, sizeof(buffer), &response_addr, TIMEOUT);

	                if (bytes_received < 0)
	                {
	                    printf("Error while waiting for ACK\n");
	                    ack_received = -1;
	                    break;
	                }

	                if (bytes_received == 0)
	                {
	                    retry_count++;
						printf("Timeout waiting for ACK %hu\n",blocknum);
	                    continue;
	                }

	                // Parse response
	                if (packet_parse(buffer, bytes_received, &received_packet) < 0)
	                {
	                    printf("Invalid response packet\n");
	                    ack_received = -1;
	                    break;
	                }

	                // Server returned ERROR
	                if (IS_ERROR(received_packet.opcode))
	                {
	                    printf("\n================================\n");
	                    printf("ERROR %hu: %s\n", received_packet.ecode, received_packet.estring);
	                    printf("================================\n");

	                    ack_received = -1;
	                    break;
	                }

	                // Verify ACK
	                if (!IS_ACK(received_packet.opcode))
	                {
	                    printf("Expected ACK packet\n");
	                    ack_received = -1;
	                    break;
	                }

	                if (received_packet.blocknum != blocknum)
	                {
	                    printf("Unexpected ACK block\n");
	                    printf("Expected: %hu\n", blocknum);
	                    printf("Received: %hu\n", received_packet.blocknum);
	                    ack_received = -1;
	                    break;
	                }

	                printf("ACK block: %hu\n", received_packet.blocknum);
					printf("ACK block %hu verified successfully\n",blocknum);
	                ack_received = 1;
	                break;
	            }

	            if (ack_received != 1)
	            {
	                if (ack_received == 0)
	                    printf("ACK timeout limit reached " "for block %hu\n", blocknum);

	                free(data_packet);
	                close(fd);
	                break;
	            }
	        }
        	free(data_packet);

	        // Final DATA packet
	        if (bytes_read < BLIMIT)
	        {
	            printf("\n================================\n");
	            printf("Upload complete!\n");
	            printf("================================\n");

	            close(fd);
	            break;
	        }

	        position += bytes_read;
	        blocknum++;
	    }
	    continue;
	}
		
		if (strcmp(command, "mode") == 0)
		{
		    printf("Usage: mode <netascii | octet>\n");
		    continue;
		}	
        if (strncmp(command, "mode ", 5) == 0)
        {
            char mode[20];

            if (sscanf(command + 5, "%19s", mode) != 1)
            {
                printf("Usage: mode <octet|netascii>\n");
                continue;
            }

            if (strcmp(mode, MODE_OCTET) == 0)
            {
                strcpy(transfer_mode, MODE_OCTET);
                printf("Transfer mode set to octet\n");
            }
            else if (strcmp(mode, MODE_NETASCII) == 0)
            {
                strcpy(transfer_mode, MODE_NETASCII);
                printf("Transfer mode set to netascii\n");
            }
            else
            {
                printf("Invalid mode. Use 'octet' or 'netascii'.\n");
            }
            continue;
        }

        if (strlen(command) == 0)	continue;
        printf("Unknown command: %s\n", command);
    }

    if (client_sockfd >= 0)
        close(client_sockfd);

    return 0;
}