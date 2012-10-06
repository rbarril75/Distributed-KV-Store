#include "socket.h"

#define CONNECTIONS 2

int newListenSocket(Socket *sock, char* port) {
	int status;
    int err;	
	sock->port = port;
	memset(&(sock->hints), 0, sizeof (sock->hints));
	sock->hints.ai_family = AF_UNSPEC;
	sock->hints.ai_socktype = SOCK_STREAM;
    sock->hints.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, sock->port, &(sock->hints), &(sock->servinfo))) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		return 1;
	}
	for(sock->ptr = sock->servinfo; sock->ptr != NULL; sock->ptr->ai_next) {
		if ((sock->fd = socket (sock->ptr->ai_family, sock->ptr->ai_socktype, 
								sock->ptr->ai_protocol)) == -1) {
			perror("socket: error");
			continue;
		}	
		// Bind if receiving socket
		if (bind(sock->fd, sock->ptr->ai_addr, sock->ptr->ai_addrlen) == -1) {
            err = errno;
			close(sock->fd);
			perror("bind: error");
            if(err == EADDRINUSE) {
                exit(1);
            }

			continue;
		}
		break;
	}
	
	if (sock->ptr == NULL) {
		fprintf(stderr, "failed to get socket\n");
		return 2;
	}
	
    if (listen(sock->fd, CONNECTIONS) == -1) {
        perror("listen");
        return 1;
    }

	return sock->fd;
}

int newSendSocket(Socket *sock, char* ip, char* port) {
    int status;
	int err;

	sock->port = port;
	memset(&(sock->hints), 0, sizeof (sock->hints));
	sock->hints.ai_family = AF_UNSPEC;
	sock->hints.ai_socktype = SOCK_STREAM;
	
	sock->hints.ai_flags = AI_NUMERICHOST;
    if ((status = getaddrinfo(ip, sock->port, &(sock->hints), &(sock->servinfo))) != 0) {
	    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
	    return -1;
    }
	for(sock->ptr = sock->servinfo; sock->ptr != NULL; sock->ptr->ai_next) {
		if ((sock->fd = socket (sock->ptr->ai_family, sock->ptr->ai_socktype, 
								sock->ptr->ai_protocol)) == -1) {
			perror("socket: error");
			continue;
		}	
        //connect if sending socket
        if(connect(sock->fd, sock->ptr->ai_addr, sock->ptr->ai_addrlen) == -1) {
            close(sock->fd);
            err = errno;
            perror("client:connect");
            if(err == ECONNREFUSED) {
                return -1;
            }
            continue;
        }

		break;
	}
	
	if (sock->ptr == NULL) {
		fprintf(stderr, "connect: failed to get socket\n");
		return -1;
	}
    return sock->fd;
}

int newRequestSocket(Socket *sock, char* ip, char* port, int recv) {
    int status;
	sock->port = port;
	memset(&(sock->hints), 0, sizeof (sock->hints));
	sock->hints.ai_family = AF_UNSPEC;
	sock->hints.ai_socktype = SOCK_DGRAM;
	
    if(recv) {
        sock->hints.ai_flags = AI_PASSIVE;
        if ((status = getaddrinfo(NULL, sock->port, &(sock->hints), &(sock->servinfo))) != 0) {
		    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		    return 1;
	    }
    } else {
	    sock->hints.ai_flags = AI_NUMERICHOST;
	    if ((status = getaddrinfo(ip, sock->port, &(sock->hints), &(sock->servinfo))) != 0) {
		    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		    return 1;
	    }
    }
	for(sock->ptr = sock->servinfo; sock->ptr != NULL; sock->ptr->ai_next) {
		if ((sock->fd = socket (sock->ptr->ai_family, sock->ptr->ai_socktype, 
								sock->ptr->ai_protocol)) == -1) {
			perror("socket: error");
			continue;
		}	
		// Bind if receiving socket
		if (recv) {
			if (bind(sock->fd, sock->ptr->ai_addr, sock->ptr->ai_addrlen) == -1) {
				close(sock->fd);
				perror("bind: error");
				continue;
			}
		}
		break;
	}

	if (sock->ptr == NULL) {
		fprintf(stderr, "failed to get socket\n");
		return 2;
	}
	
	return sock->fd;

}

int newAcceptSocket(int listenfd) {
    printf("accepting new connection\n");
    sockaddr_storage their_addr;
    socklen_t sin_size = sizeof their_addr;
    return accept(listenfd, (sockaddr *)&their_addr, &sin_size);
}






