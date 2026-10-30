#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "tftp.h"

static int udp_bind(int port)
{
    int sockfd;
    struct sockaddr_in addr = {0};

    if (port < 1 || port > 65535)
    {
        fprintf(stderr, "Invalid UDP port: %d\n", port);
        return -1;
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int udp_bind_server(int port)
{
    return udp_bind(port);
}

int udp_rebind_server(int port)
{
    return udp_bind(port);
}

int udp_bind_client(void)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0)
        perror("socket");

    return sockfd;
}

int udp_send_packet(int sockfd, const void *packet, int packet_size, struct sockaddr_in *dest_addr)
{
    int bytes_sent;

    if (sockfd < 0 || !packet || packet_size < 0 || !dest_addr)
        return -1;

    bytes_sent = sendto(sockfd, packet, packet_size, 0,
                        (struct sockaddr *)dest_addr, sizeof(*dest_addr));

    if (bytes_sent < 0)
        perror("sendto");

    return bytes_sent;
}

int udp_receive_packet(int sockfd, void *buffer, int buffer_size, struct sockaddr_in *src_addr)
{
    socklen_t addr_len;
    int bytes_received;

    if (sockfd < 0 || !buffer || buffer_size <= 0 || !src_addr)
        return -1;

    addr_len = sizeof(*src_addr);
    bytes_received = recvfrom(sockfd, buffer, buffer_size, 0,
                              (struct sockaddr *)src_addr, &addr_len);

    if (bytes_received < 0)
        perror("recvfrom");

    return bytes_received;
}

int udp_receive_packet_timeout(int sockfd, void *buffer, int buffer_size, struct sockaddr_in *src_addr, int timeout)
{
    fd_set readfds;
    struct timeval tv;
    int ret;

    if (sockfd < 0 || !buffer || buffer_size <= 0 ||
        !src_addr || timeout < 0)
        return -1;

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    ret = select(sockfd + 1, &readfds, NULL, NULL, &tv);

    if (ret < 0)
    {
        perror("select");
        return -1;
    }

    if (ret == 0)
        return 0;

    return udp_receive_packet(sockfd, buffer, buffer_size, src_addr);
}