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
#include "kvstore.h"


#define HEARTBEAT_INTERVAL 3000 // milliseconds
#define FAILURE_TIMEOUT 3 * HEARTBEAT_INTERVAL

FailureDetector myFD;
KVStore * kvstore;

void handler(int sig);
bool twoClosed(pollfd * ufds);
int receiveJoinId(int fd);
int printKVInfo(KVStore kvstore);

int main(int argc, char ** argv) {
    bool usingAux = false;
    bool setup;
    
    if(atoi(argv[2]) == 1) {
        setup = true;
    } else {
        setup = false;
    }

    //Signal setup
	struct sigaction sa;
	sa.sa_handler = handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);

    
	//seed random numbers
	srand(time(NULL));

//setup KVSTORE
	struct pollfd ufds[6];

    kvstore = new KVStore(atoi(argv[1]));

    sleep(10);
    
    //initialize FD
    newFailureDetector(&myFD, argv);

    myFD.start_time = cur_time_ms();
	long long initialTO = myFD.start_time + FAILURE_TIMEOUT;
	long long timeout_time[3] = {initialTO, initialTO, initialTO};
	long long msg_time = myFD.start_time + HEARTBEAT_INTERVAL;
	
    //int count = 0;
	long long timeout;
    long long death_times[3] = {0, 0, 0}; 
	int ret;
	myFD.msg_count = 0;

//setup send socks
    kvstore->setupSendSocks();

//setup for poll
//fdet
    ufds[0].fd = myFD.recv_sock->fd;
    ufds[0].events = POLLIN;
//requests
	ufds[1].fd = kvstore->req_sock;
    ufds[1].events = POLLIN;
//recv messages
    ufds[2].fd = -1;
    ufds[2].events = POLLIN;
    ufds[3].fd = -1;
    ufds[3].events = POLLIN;
//aux
    ufds[4].fd = -1;
    ufds[4].events = POLLIN;
//listen
    ufds[5].fd = kvstore->listen_sock;
    ufds[5].events = POLLIN;

//initial pulse
	pulse(&myFD, argv[1], death_times);

    int status;
    int retval;
    int flag;
    int setupCount = 0;
    bool synch = true;
    printf("POLLING\n");
    while(1) {
		check_failures(&myFD, timeout_time, death_times);	
		if (((timeout = (msg_time - cur_time_ms())) <= 0) && (timeout != 0)) {
			timeout = 0;
		}
		if ((status = poll(ufds, 6, timeout)) == -1) {
			perror("poll: error");
            printf("MESSAGES PASSED: %d\n", kvstore->messages_passed);
            printf("KEYS STORED: %d\n", (int) (kvstore->store[0].size() + kvstore->store[1].size() + 
				   kvstore->store[2].size() ));
			exit(1);
		} else if (status == 0) {
            //timedout...
		} else {
            //failure detector
			if (ufds[0].revents & POLLIN) {
                printf("HBEAT\n");
				ret = receive(&myFD, death_times, timeout_time, FAILURE_TIMEOUT, &flag);
                if(flag == 1) {
                    kvstore->markFailed(ret);
                } else if(flag == 2) {
                    kvstore->markAlive(ret);        
                } //else... just a heartbeat
            //requests
			}
            if (ufds[1].revents & POLLIN) {
                printf("UFDS: REQUEST\n");
                kvstore->receiveRequest();
            }
            //tcp communication sockets
            if (ufds[2].revents & POLLIN) {
                printf("UFDS: COMM 2\n");
                retval = kvstore->receiveMessage(ufds[2].fd);
                if (retval == -1) {
                    ufds[2].fd = -1;
                }
            }
            if(ufds[3].revents & POLLIN) {
                printf("UFDS: COMM 3\n");
                retval = kvstore->receiveMessage(ufds[3].fd);
                if (retval == -1) {
                    ufds[3].fd = -1;
                }
            }
            //auxiliary
            if(ufds[4].revents & POLLIN) {
                printf("UFDS: COMM AUX\n");
                retval = kvstore->receiveMessage(ufds[4].fd);
                if(retval == -1) {
                    ufds[4].fd = -1;
                }
            }
            //listen socket
            if(ufds[5].revents & POLLIN) {
                printf("UFDS: RECEIVED CONNECTION\n");
                //receiving sockets
                if((setup == true) || (synch == true)) {
                    printf("SETUP\n");
                    if(ufds[2].fd == -1) {
                        kvstore->accept_socks[0] = newAcceptSocket(kvstore->listen_sock);
                        ufds[2].fd = kvstore->accept_socks[0];
                        retval = receiveJoinId(kvstore->accept_socks[0]);
                    } else {
                        kvstore->accept_socks[1] = newAcceptSocket(kvstore->listen_sock);
                        ufds[3].fd = kvstore->accept_socks[1];
                        retval = receiveJoinId(kvstore->accept_socks[1]);
                    }
                    setupCount++;
                    if(setupCount >= 2) {
                        setup = false;
                    }
                    synch = false;
                //set up aux connection
                } else if(usingAux && (ufds[4].fd == -1)) {
                    printf("SET UP AUX\n");
                    kvstore->aux_recv_sock = newAcceptSocket(kvstore->listen_sock);
                    ufds[4].fd = kvstore->aux_recv_sock;
                //get rid of aux connection
                } else if(usingAux && (ufds[4].fd != -1)) {
                    printf("GET RID OF AUX\n");
                    close(kvstore->aux_recv_sock);
                    usingAux = false;
                    ufds[4].fd = -1;
                    if(ufds[2].fd == -1) {
                        kvstore->accept_socks[0] = newAcceptSocket(kvstore->listen_sock);
                        ufds[2].fd = kvstore->accept_socks[0];
                        retval = receiveJoinId(kvstore->accept_socks[0]);
                        printf("Received connection\n");
                    } else {
                        kvstore->accept_socks[1] = newAcceptSocket(kvstore->listen_sock);
                        ufds[3].fd = kvstore->accept_socks[1];
                        retval = receiveJoinId(kvstore->accept_socks[1]);
                        printf("received connection\n");
                    }
                    notify_nodes(&myFD, retval, 1);
                //2 or 3 alive, no aux
                } else {
                    printf("JOIN; NO AUX\n");
                    if(ufds[2].fd == -1) {
                        kvstore->accept_socks[0] = newAcceptSocket(kvstore->listen_sock);
                        ufds[2].fd = kvstore->accept_socks[0];
                        retval = receiveJoinId(kvstore->accept_socks[0]);
                    } else {
                        kvstore->accept_socks[1] = newAcceptSocket(kvstore->listen_sock);
                        ufds[3].fd = kvstore->accept_socks[1];
                        retval = receiveJoinId(kvstore->accept_socks[1]);
                    }
                    printf("CONNECTION FROM %d\n", retval);
                    notify_nodes(&myFD, retval, 1);
                    synch = false;
                }
            }
		}
        if((twoClosed(ufds))) {
            if(synch == false) {
                usingAux = true;
            }
        }
	    //handle pulse & pulse timing	
		if (cur_time_ms() >= msg_time) {
			pulse(&myFD, argv[1], death_times);
			msg_time = cur_time_ms() + HEARTBEAT_INTERVAL;
		}
	}
}

//catch SIGINT
void handler (int sig) {
    //setup for metrics
    int uptime = (int) (get_uptime(&myFD)/1000);

	printf("\nTERMINATING EXECUTION\n");
    printf("FAILURE DETECTOR:\n");
	printf("Sent: %d Messages; %d bytes\n", myFD.msg_count, myFD.msg_count);
	printf("Total uptime: %d seconds\n", uptime);
	printf("Bytes per second: %f\n", (float) ((float)myFD.msg_count / (float)(uptime)));
    
    printf("\nSTORE:\n");
    printf("Number of keys stored: %d\n", (int)(kvstore->store[0].size()
                                            + kvstore->store[1].size()
                                            + kvstore->store[2].size()));
    printf("Size of data stored (keys and values): %d bytes\n", kvstore->data_stored_size);
    printf("Number of messages passed: %d\n", kvstore->messages_passed);
    printf("Total size of messages passed: %lld bytes\n", kvstore->msg_passed_size);
    double msg_bandwidth = ((double)kvstore->msg_passed_size / (double)uptime);
    printf("Bytes/s for message passing: %f\n", msg_bandwidth);
    printf("Total size of data transmitted for replicas: %lld bytes\n", kvstore->data_sent_size);
    printf("Total size of data received for replicas: %lld bytes\n", kvstore->data_received_size);
    double data_send_bwidth = ((double)kvstore->data_sent_size / (double)uptime);
    printf("Bandwidth for transmitted data: %f bytes/s\n", data_send_bwidth);
    double data_recv_bwidth = ((double)kvstore->data_received_size / (double)uptime);
    printf("Bandwidth for received data: %f bytes/s\n", data_recv_bwidth);

	exit(1);
}

//check if our normal connections are unused
bool twoClosed(pollfd * ufds) {
    return ((ufds[2].fd == -1) && (ufds[3].fd == -1));
}

//receive the ID of a joiner
int receiveJoinId(int fd) {
    int numbytes;
    int id;

    if((numbytes = recv(fd, &id, sizeof id, 0))== -1) {
       perror ("receive");
       return 0;
    }

    return id;
}

int printKVInfo(KVStore kvstore) {
    int i;
    printf("listen sock: %d\n", kvstore.listen_sock);
    for(i = 0; i < 2; i++) {
        printf("send_socks[%d] = %d\n", i, kvstore.send_socks[i]);
        printf("send_socks[%d] = %d\n", i, kvstore.accept_socks[i]);
        printf("alive[%d] = %s\n", i, (kvstore.alive[i] ? "true" : "false"));
    }
    return 0;
}
