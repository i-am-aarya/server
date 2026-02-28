#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>

int server_create(int port);
int server_accept(int server_fd, struct sockaddr_in *client_addr);

#endif
