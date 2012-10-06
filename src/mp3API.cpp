/*
 *  mp2API.cpp
 *  
 *
 *  Created by Ryan Barril on 7/9/12.
 *  Copyright 2012 __MyCompanyName__. All rights reserved.
 *
 */

using namespace std;

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
#include <sys/poll.h>
#include <ctime>
#include <vector>
#include <sys/time.h>

#include "socket.h"

#define MYPORT "49994"
#define MYIP "127.0.0.1"
#define SEMICOLONS 6
#define OPER_TIMEOUT

using namespace std;

bool isValidOper(char* oper);
int getRandomIndex(vector<int> possTargs);
char* buildMessage(char* oper, char* key, char* value);
int receiveResult(int recv_fd); 
int sendRequest(char* oper, char* key, char* value, int targInd);
void genRandKey(char* buf);
void genRandVal(char * buf, int len);
int executeOperation(char* oper, char* key, char* val, int recv_fd, struct pollfd *ufds,
                            vector<int>& possTargs);
long long cur_time_ms();

char * ips[4];
char * ports[4];
int ids[4];

int main (int argc, char ** argv) {
	
    //variables
    int count = 0;
    int temp_id;
    char temp_port[80];
    char temp_udp[80];
    char temp_fdet[80];
    char temp_ip[80];

    //latency information
    long long send_time;
    long long recv_time;
    int overall_latency = 0;

    //load information from cfg file
    FILE* cfg;
    cfg = fopen("cfg.txt", "r");
    while(fscanf(cfg, "%s %s %s %s %d", temp_ip, temp_port, temp_udp, temp_fdet, &temp_id) != EOF) {
        ips[count] = new char[(strlen(temp_ip) + 1)];
		temp_ip[strlen(temp_ip)] = '\0';
		strcpy(ips[count], temp_ip);
		ports[count] = new char[(strlen(temp_udp)+1)];
		temp_port[strlen(temp_udp)] = '\0';
		strcpy(ports[count], temp_udp);
		ids[count] = temp_id;
		count++;
    }
    fclose(cfg);

	Socket * sock = (Socket*)malloc(sizeof(Socket));
    //our socket for receiving
	int recv_fd = newRequestSocket(sock, NULL, (char*) MYPORT, 1);

    //get operation mode; 1 or 1000
    int mode = atoi(argv[2]);
    if(mode <= 0) {
        printf("Usage: ./mp2 <oper> <mode> [<key>] [<val>]\n");
        printf("<mode> must be int > 0\n");
        exit(1);
    }

    if(!isValidOper(argv[1])) {
        printf("Usage: ./mp2 <oper> <mode> [<key>] [<val>]\n");
        printf("<oper> must be one of: lookup, insert, remove\n");
        exit(1);
    }

    //seed random numbers
    srand(time(NULL));
	
	struct pollfd ufds[1];
	ufds[0].fd = recv_fd;
    ufds[0].events = POLLIN;
	int targarr[] = {0, 1, 2, 3};
    vector<int> possTargs(targarr, targarr + sizeof(targarr) / sizeof(int));

    if(mode == 1) {
        if(argc != 5) {
            printf("invalid number of arguments\n");
            exit(1);
        }
        send_time = cur_time_ms();
		executeOperation(argv[1], argv[3], argv[4], recv_fd, ufds, possTargs);
        recv_time = cur_time_ms();
        overall_latency += (int) (recv_time - send_time);
    } else {
        int i;

        for(i = 0; i < mode; i++) {
            //get a random key from 1 to 1000000
            char keybuf[33];
            genRandKey(keybuf);
            char key[strlen(keybuf)+1];
            //key = char[strlen(keybuf)+1];
            strcpy(key, keybuf);
            //get a random value
            int len = (rand() % 10) + 1;
            char val[len+1];
            //val = char[len+1];
            genRandVal(val, len);
            //then as before
            send_time = cur_time_ms();
			executeOperation(argv[1], key, val, recv_fd, ufds, possTargs);
            recv_time = cur_time_ms();
            overall_latency += (int) (recv_time - send_time);
        }
    }
    printf("Finished Sending\n");
    printf("Overall Latency for %s operations: %d milliseconds\n", argv[2], overall_latency);

    //clean up
    close(recv_fd);
    freeaddrinfo(sock->servinfo);
    free(sock);
	exit(1);
}

int executeOperation(char* oper, char* key, char* val, int recv_fd, struct pollfd *ufds,
                                vector<int>& possTargs) {
	int targInd;
	int status;
	int res = 0;
	while (res == 0) {
		targInd = getRandomIndex(possTargs);
		printf("TARGET: %d\n", targInd);
		sendRequest(oper, key, val, targInd);
		if ((status = poll(ufds, 1, 2000)) == -1) {
			perror("poll: error");
		} else if (status == 0) {
			printf("OP TIMED OUT. REQUESTING DIFF PROCESS.\n");
			swap(possTargs[targInd], possTargs.back());
			possTargs.pop_back();
		} else {
			if (ufds[0].revents & POLLIN) {
				res = receiveResult(recv_fd);
			}
		}
	}
	return 1;
}

bool isValidOper(char* oper) {
    if((strcmp("lookup", oper)==0) ||
        (strcmp("insert", oper)==0) ||
        (strcmp("remove", oper) ==0)) {
        return true;
    }
    return false;
}

int getRandomIndex(vector<int> possTargs) {
    return possTargs[rand() % possTargs.size()];
}

char* buildMessage(char* oper, char* key, char* value) {
    //hardcoded; will need to be changed
    int length = strlen(MYIP) +
                    strlen(MYPORT) + 
                    strlen("0") + 
                    strlen("0") + 
                    strlen(oper) + 
                    strlen(key) + 
                    strlen(value) +
                    SEMICOLONS + 1;
    
    char* msg = new char[length];
    strcpy(msg, (char*)MYIP);
    strcat(msg, ":");
    strcat(msg, (char*)MYPORT);
    strcat(msg, ":");
    strcat(msg, "0");
    strcat(msg, ":");
    strcat(msg, "0");
    strcat(msg, ":");
    strcat(msg, oper);
    strcat(msg, ":");
    strcat(msg, key);
    strcat(msg, ":");
    strcat(msg, value);
    return msg;
}

int sendRequest(char* oper, char* key, char* value, int targInd) {
    Socket * sock = (Socket*)malloc(sizeof(Socket));
	int req_fd = newRequestSocket(sock, ips[targInd], ports[targInd], 0);
    char* msg = buildMessage(oper, key, value);    

    int numbytes;
    printf("sending message %s, %d bytes\n", msg, (int)strlen(msg));
	if((numbytes = sendto(req_fd, msg, strlen(msg), 0,
				  sock->ptr->ai_addr, sock->ptr->ai_addrlen)) == -1) {
        perror("sendto: error sending message to process");
        exit(1);
    }
    printf("SENT %d BYTES\n", numbytes);
    delete[] msg; 
    freeaddrinfo(sock->servinfo);
    free(sock);
    close(req_fd);
    return 1;
}


int receiveResult(int recv_fd) {
	struct sockaddr_storage their_addr;
	char buf[100];
	socklen_t addr_len;
	int nbytes;
	addr_len = sizeof their_addr;
	if ((nbytes = recvfrom(recv_fd, buf, 99, 0, 
					 (struct sockaddr *)&their_addr, &addr_len)) == -1) {
	    perror("recvfrom: error alert");
		exit(1);
	}
	buf[nbytes] = '\0';
	printf("RESULT IS: %s\n", buf);
    return 1;
}

//take a random int from 1 to 1000000 and copy it into buf
void genRandKey(char * buf) {
    snprintf(buf, sizeof(buf), "%d", (rand() % 1000000) + 1);
}

//take a buffer buf of length len, and copy a random string into it
//as seen on stackoverflow.com; presented by Ates Goral
void genRandVal(char * buf, int len) {
    static const char alphanum[] = 
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";
    int i;
    for(i = 0; i < len; i++) {
        buf[i] = alphanum[rand() % (sizeof(alphanum)-1)];
    }

    buf[len] = '\0';
}

long long cur_time_ms() {
    struct timeval to_set;
    gettimeofday(&to_set, NULL);
    return (long long) (((to_set.tv_sec)*1000) +
                        (to_set.tv_usec)/1000);
}
