/*
 * Machine Problem #1
 * CS425 Summer 2012
 */

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
#include <time.h>
#include <sys/time.h>

#include "failuredetector.h"
#include "socket.h"

#define MAXBUFLEN 100
#define MACHINES 2

//AS APPEARS ON BEEJ'S GUIDE:
//http://beej.us/guide/bgnet/output/html/multipage/index.html
//Used in obtaining address from which message was received
void * get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//Sets up some parts of the failuredetector structure
int newFailureDetector(FailureDetector *myFD, char ** argv) {
	myFD->ID = atoi(argv[1]);
	
	configure_process(myFD);
	
	myFD->recv_sock = new Socket; 
	myFD->send_socks[0] = new Socket;
	myFD->send_socks[1] = new Socket;
	myFD->send_socks[2] = new Socket;
	
    
	newRequestSocket(myFD->recv_sock, NULL, myFD->my_port, 1);
	newRequestSocket(myFD->send_socks[0], myFD->ips[0], myFD->ports[0], 0);
	newRequestSocket(myFD->send_socks[1], myFD->ips[1], myFD->ports[1], 0);
	newRequestSocket(myFD->send_socks[2], myFD->ips[2], myFD->ports[2], 0);
	
	return 0;
}

//Uses the configuration file to prepare the ports, ips, and ids
void configure_process(FailureDetector *myFD) {
	FILE* cfg;
	cfg = fopen("cfg.txt", "r");
	
	char temp_port[80];
	char temp_ip[80];
    char temp_req[80];
    char temp_fdet[80];
	int temp_id;
	int count = 0;
	int i;
    
    //adjusted for 2
    for(i = 0; i < MACHINES + 1; i++) {
        myFD->fpos_times[i] = 0;
    }

	while(fscanf(cfg, "%s %s %s %s %d", temp_ip, temp_port, temp_req, temp_fdet, &temp_id) != EOF) {
        if(myFD->ID == temp_id) {
			myFD->my_port = (char *) malloc(strlen(temp_fdet)+1);
			temp_fdet[strlen(temp_fdet)] = '\0';
			strcpy(myFD->my_port, temp_fdet);
		} else if(temp_id == ((myFD->ID % 4) + 1)){
			myFD->ips[0] = (char *) malloc(strlen(temp_ip) + 1);
			temp_ip[strlen(temp_ip)] = '\0';
			strcpy(myFD->ips[0], temp_ip);
			myFD->ports[0] = (char *) malloc(strlen(temp_fdet)+1);
			temp_fdet[strlen(temp_fdet)] = '\0';
			strcpy(myFD->ports[0], temp_fdet);
			myFD->ids[0] = temp_id;
			count++;
            //blah
		} else if((temp_id == (((myFD->ID+1) % 4) + 1))){
            myFD->ips[1] = (char *) malloc(strlen(temp_ip) + 1);
			temp_ip[strlen(temp_ip)] = '\0';
			strcpy(myFD->ips[1], temp_ip);
			myFD->ports[1] = (char *) malloc(strlen(temp_fdet)+1);
			temp_fdet[strlen(temp_fdet)] = '\0';
			strcpy(myFD->ports[1], temp_fdet);
			myFD->ids[1] = temp_id;
			count++;
		} else if (temp_id != myFD->ID) {
            myFD->ips[2] = (char *) malloc(strlen(temp_ip) + 1);
			temp_ip[strlen(temp_ip)] = '\0';
			strcpy(myFD->ips[2], temp_ip);
			myFD->ports[2] = (char *) malloc(strlen(temp_fdet)+1);
			temp_fdet[strlen(temp_fdet)] = '\0';
			strcpy(myFD->ports[2], temp_fdet);
			myFD->ids[2] = temp_id;
			count++;
        } else {}
	}
    printf("MYPORT: %s\n", myFD->my_port);
    for(i = 0; i < 3; i++) {
        printf("%d: PORT: %s, ID: %d\n", myFD->ID, myFD->ports[i], myFD->ids[i]);
    }
	fclose(cfg);
	return;
}

//Obtains the current time in milliseconds using gettimeofday()
long long cur_time_ms() {
	struct timeval to_set;
	gettimeofday(&to_set, NULL);
	return (long long) (((to_set.tv_sec)*1000) +
						(to_set.tv_usec)/1000);
}

//Pulse messages to the other machines/processes
int pulse(FailureDetector *myFD, char* msg, long long * death_times) {
    printf("PULSING\n");
	int numbytes;
	int i;
    //modified for two targets
	for(i = 0; i < MACHINES; i++) {
            if((numbytes = sendto(myFD->send_socks[i]->fd, msg, strlen(msg), 0, 
								  myFD->send_socks[i]->ptr->ai_addr, myFD->send_socks[i]->ptr->ai_addrlen)) == -1) {
			perror("sendto: error");
			exit(1);
		    }
		    myFD->msg_count++;
	}
	return 0;
}

//Receive messages
//flag 1 for failure, 2 for join, 0 for neither
int receive(FailureDetector *myFD, long long * death_times, long long * timeout_time, long long finterval, int * flag) { 
    int toReturn = 0;
	struct sockaddr_storage their_addr;
	char buf[MAXBUFLEN];
	socklen_t addr_len;
	int numbytes;
	addr_len = sizeof their_addr;
	if ((numbytes = recvfrom(myFD->recv_sock->fd, buf, MAXBUFLEN-1 , 0, 
							 (struct sockaddr *)&their_addr, &addr_len)) == -1) {
		perror("recvfrom: error");
		exit(1);
	}
    buf[numbytes] = '\0';
    //if it's a heartbeat
    if(numbytes == 1) {
	    int machine = atoi(buf);
	    int index, i;
        *flag = 0;
	    for(i = 0; i < MACHINES + 1; i++) {
		    if(machine == myFD->ids[i]) {
			    index = i;
			    //if was marked dead
			    if(is_alive(death_times, i) == 0) {
                    myFD->fpos_times[i] += cur_time_ms() - death_times[i];
				    death_times[i] = 0;
                    printf("PROCESS %d ALIVE AGAIN\n", myFD->ids[i]);
                    *flag = 2;
                    toReturn = atoi(buf); 
			    }
		    }
	    }
        //adjust timeout time
        timeout_time[index] = cur_time_ms() + finterval;
	    buf[numbytes] = '\0';
	    return toReturn;
    } else {
        //deal with the failure/join message; set flag
        int ret_id = process_notify_message(buf, flag);
        return ret_id;
    }
}

//Check to see if any machine/process has failed
//let others know via notify
int check_failures(FailureDetector *myFD, long long * timeout_time, long long* death_times) {
    int ret_id = 0;
	int i;
	for(i = 1; i < MACHINES + 1; i++) {
        long long cur_time = cur_time_ms();
		if (cur_time >= timeout_time[i] && (is_alive(death_times, i) == 1)) {
			printf("FAILURE DETECTED ON PORT:  %s, WITH IP %s\n", myFD->ports[i], myFD->ips[i]);
			death_times[i] = cur_time;
            notify_nodes(myFD, myFD->ids[i], 0);
		}
	}
	return ret_id;
}

//notify nodes in the system of an event
int notify_nodes(FailureDetector *myFD, int id, int event) {
    int numbytes;
	int i;
    char buf[33];
    snprintf(buf, sizeof(buf), "%d", id);
    char msg[3 + strlen(buf)];
    if(event == 0) {
        strcpy(msg, "f:");
    } else {
        strcpy(msg, "j:");
    }
    strcat(msg, buf);
    printf("SENDING NOTIFY: %s\n", msg);
	for(i = 0; i < 3; i++) {
        if((numbytes = sendto(myFD->send_socks[i]->fd, msg, strlen(msg), 0, 
							  myFD->send_socks[i]->ptr->ai_addr, myFD->send_socks[i]->ptr->ai_addrlen)) == -1) {
			perror("sendto: error");
			exit(1);
		}
		myFD->msg_count++;
	}
	return 0;

}

int process_notify_message(char * msg, int * flag) {
    char * tokens[2];
    char * first = strtok(msg, ":");
    tokens[0] = first;
    tokens[1] = strtok(NULL, ":");
    if(strcmp("f", tokens[0]) == 0) {
        *flag = 1;
    } else {
        *flag = 2;
    }
    return atoi(tokens[1]);
}

//Get total uptime for the program
int get_uptime(FailureDetector *myFD) {
	return cur_time_ms() - myFD->start_time;
}

//Determine if a machine/process is dead
int is_alive(long long* death_times, int index) {
    if(death_times[index] == 0) {
        return 1;
    } else {
        return 0;
    }
}
