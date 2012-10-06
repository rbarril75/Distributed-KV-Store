#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

typedef struct {
	int fd;
	char* port;
	struct addrinfo hints;
	struct addrinfo *servinfo;
	struct addrinfo *ptr;
} Socket;

int newListenSocket(Socket *sock, char* port);
int newSendSocket(Socket *sock, char* ip, char* port);
int newRequestSocket(Socket *sock, char* ip, char* port, int recv);
int newAcceptSocket(int listenfd);

#endif
