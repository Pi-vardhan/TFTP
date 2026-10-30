
#ifndef TFTP_UDP
#define TFTP_UDP

#include <sys/socket.h>
#include <netinet/in.h>

int udp_bind_server(int port);
int udp_rebind_server(int port);
int udp_bind_client(void);
int udp_send_packet(int sockfd, const void *packet, int packet_size, struct sockaddr_in *dest_addr);
int udp_receive_packet(int sockfd, void *buffer, int buffer_size, struct sockaddr_in *src_addr); 

int udp_receive_packet_timeout(int sockfd, void *buffer, int buffer_size, struct sockaddr_in *src_addr, int timeout);

#endif
