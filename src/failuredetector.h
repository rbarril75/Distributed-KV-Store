/*
 * Machine Problem #1
 * CS425, Summer 2012
 */

#ifndef FAILUREDETECTOR_H_
#define FAILUREDETECTOR_H_

#include "socket.h"

typedef struct {
	// Set by program args
	int ID;
	int machines;
	int packet_loss;
	
	// Set by newSocket()
	Socket* recv_sock;
	Socket* send_socks[3];
	
	// Set by configure_process()
	char* ips[3];
	char* my_port;
	char* ports[3];
	int ids[3];


	// Set in main()
	long long start_time;
	int msg_count;
    long long fpos_times[3];

} FailureDetector;

int newFailureDetector(FailureDetector *myFD, char ** argv);

void configure_process(FailureDetector *myFD);
long long cur_time_ms();
int get_uptime(FailureDetector *myFD);
int check_failures(FailureDetector *myFD, long long * timeout_time, long long * death_times);
int pulse(FailureDetector *myFD, char* msg, long long * death_times);
int receive(FailureDetector *myFD, long long * death_times, long long * timeout_time, long long finterval, int* flag);
int should_send(int packet_loss);
int is_alive(long long* death_times, int index);
int notify_nodes(FailureDetector *myFD, int id, int event);
int process_notify_message(char * msg, int* flag);

#endif /*FAILUREDETECTOR_H_ */
