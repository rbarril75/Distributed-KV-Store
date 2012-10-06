#include "socket.h"
#include "kvstore.h"

#define UPRANGE 250000
#define SEMICOLONS 6
#define RECVBUFSIZE 8192

using namespace std;
//Constructor for KVStore; takes in an ID which it uses
//to identify itself; 
KVStore::KVStore(int id) {
    //metrics setup
    messages_passed = 0;
    msg_passed_size = 0;
    data_received_size = 0;
    data_sent_size = 0;

    data_stored_size = 0;


    ID = id;
	FILE* cfg;
	cfg = fopen("cfg.txt", "r");
	
	char temp_port[80];
    char temp_udp[80];
	char temp_ip[80];
    char temp_fdet[80];
	int temp_id;
	int count = 0;

	while(fscanf(cfg, "%s %s %s %s %d", temp_ip, temp_port, temp_udp, temp_fdet, &temp_id) != EOF) {
		if(ID == temp_id) {
			my_port = (char *) malloc(strlen(temp_port)+1);
			temp_port[strlen(temp_port)] = '\0';
			strcpy(my_port, temp_port);
            req_port = (char*) malloc(strlen(temp_udp)+1);
			temp_udp[strlen(temp_udp)] = '\0';
			strcpy(req_port, temp_udp);
		} else if(temp_id == ((ID % 4) + 1)){
			ips[0] = (char *) malloc(strlen(temp_ip) + 1);
			temp_ip[strlen(temp_ip)] = '\0';
			strcpy(ips[0], temp_ip);
			ports[0] = (char *) malloc(strlen(temp_port)+1);
			temp_port[strlen(temp_port)] = '\0';
			strcpy(ports[0], temp_port);
			ids[0] = temp_id;
			count++;
            //blah
		} else if((temp_id == (((ID+1) % 4) + 1))){
            ips[1] = (char *) malloc(strlen(temp_ip) + 1);
			temp_ip[strlen(temp_ip)] = '\0';
			strcpy(ips[1], temp_ip);
			ports[1] = (char *) malloc(strlen(temp_port)+1);
			temp_port[strlen(temp_port)] = '\0';
			strcpy(ports[1], temp_port);
			ids[1] = temp_id;
			count++;
		} else if (temp_id != ID) {
            ips[2] = (char *) malloc(strlen(temp_ip) + 1);
			temp_ip[strlen(temp_ip)] = '\0';
			strcpy(ips[2], temp_ip);
			ports[2] = (char *) malloc(strlen(temp_port)+1);
			temp_port[strlen(temp_port)] = '\0';
			strcpy(ports[2], temp_port);
			ids[2] = temp_id;
			count++;
        }
	}
    
    printf("myport: %s\n", my_port);
    int j;
    for(j = 0; j < 3; j++) {
        alive[j] = true;
        printf("%s, %s, %d\n", ips[j], ports[j], ids[j]);
    }
	fclose(cfg);
    Socket * sock = new Socket;
    req_sock = newRequestSocket(sock, NULL, req_port, 1);
    freeaddrinfo(sock->servinfo);
    listen_sock = newListenSocket(sock, my_port);
    freeaddrinfo(sock->servinfo);
    aux_send_sock = -1;
    delete sock;
}

KVStore::~KVStore() {
}

//Attempt to insert key/value
int KVStore::insert(int storeID, int key, char* value) {
    if(inRange(key, ID) != -1) {
        char * toStore = new char[strlen(value) + 1];
        strcpy(toStore, value);
        store[storeID][key] = toStore;
        data_stored_size += (long long)strlen(value + 1);
        data_stored_size += (long long)sizeof(key);
        return 1;
    } else {
        return 0;
    }
}

//Attempt to remove key
int KVStore::remove(int storeID, int key) {
	int result = 0;
    char* value;
    if(inRange(key, ID) != -1) {
        value = store[storeID][key];
        if(value != NULL) {
            data_stored_size -= (long long) strlen(value);
        }
        result = store[storeID].erase(key);
	}
	if (result == 0) {
		printf("No value\n");
	}
	return result;
}

//Attempt to lookup key
char* KVStore::lookup(int storeID, int key) {
    if(inRange(key, ID) != -1) {
        char* value = store[storeID][key];
        if(value == NULL) {
            store[storeID].erase(key);
            return NULL;
        }
        char* toReturn = new char[strlen(value)];
        strcpy(toReturn, value);
        return toReturn;
    } else {
        return NULL;
    }
}

//Receive a message from another machine in the system,
//Handle the message
int KVStore::receiveMessage(int fd){
    int numbytes;
    int len;
    int total = 0;

    if((numbytes = recv(fd, &len, sizeof len, 0)) == -1) {
        perror("receive");
        return 0;
    }
    //return -1 to indicate closed connection
    if(numbytes == 0) {
        close(fd);
        return -1;
    }
    char buf[RECVBUFSIZE];
    string msg;
    msg.reserve(len + 1);
    while(total < len) {
        if((numbytes = recv(fd, buf, bytesToReceive(len, total), 0))== -1) {
            perror("recv");
            exit(1);
        }
        msg.append(buf, numbytes);
        total += numbytes;
    }
    printf("%d: MESSAGE RECV: %d BYTES; LEN WAS %d\n", ID, total, len);
    if(msg[0] == 'd') {
        data_received_size += (long long)len;
        return handleData(msg);
    } else {
        char message[msg.length() + 1];
        strcpy(message, msg.c_str());
        return handleMessage(message, 0);
    }
}

int KVStore::bytesToReceive(int len, int total) {
    return min(len - total, RECVBUFSIZE - 1);
}

//Receive a request from the client outside the system
//Handle the message received
int KVStore::receiveRequest() {
    int numbytes;
    char buf[512];
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    addr_len = sizeof their_addr;
    if((numbytes = recvfrom(req_sock, buf, 511, 0, (sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recfrom: error receiving from client");
        exit(1);
    } 
    char msg[numbytes+1];
    strncpy(msg, buf, numbytes);
    msg[numbytes] = '\0';
    buf[512] = '\0';
    printf("%d: REQUEST RECEIVED; %d BYTES\n", ID, numbytes);
    return handleMessage(msg, 1);
}

//Handles a given message, 
//Flagged on whether or not it is an internal request
int KVStore::handleMessage(char * msg, int request) {
    Message * message = new Message;
	printf ("HANDLING: MESSAGE IS %s\n", msg);
    processMessage(msg, message, request);
    int key = atoi(message->key);
    //Handle it if we should
	int storeID = inRange(key, ID);
    char* value = NULL;
    int success = 0;
    printf("INRANGE(%d): STOREID %d\n", key, storeID);
    if((storeID != -1)) {
        if(strcmp(message->oper, "insert")==0) {
            insert(storeID, key, message->value);
            success = 1;
        } else if(strcmp(message->oper, "remove")==0) {
            remove(storeID, key);
            success = 1;
        } else if(strcmp(message->oper, "lookup")==0) {
            value = lookup(storeID, key);
            if(value == NULL) {
                returnResult(message, 0, NULL);
            } else {
                printf("LOOKED UP VALUE %s\n", value);
                returnResult(message, 1, value);
            }
			freeMessage(message);
			return 1;
        } else {
            //invalid operation
            returnResult(message, success, value);
            freeMessage(message);
            return 1;
        }
	}
    //passing logic
	if (ids[0] == atoi(message->orig_id)) {
		returnResult(message, success, value);		
	}
	else if ((alive[0]) && (ids[0] != atoi(message->orig_id)) && (inRange(key, ids[0]) != -1)) {
		passMessage(message, send_socks[0]);
		printf("PASSING: ID %d passing to %d\n", ID, ID+1);
	}
	else if ((alive[1]) && (ids[1] != atoi(message->orig_id)) && (inRange(key, ids[1]) != -1)) {
		passMessage(message, send_socks[1]);
		printf("PASSING: ID %d passing to %d\n", ID, ID+2);
	}
    //pass to aux
	else if ((ids[2] != atoi(message->orig_id)) && (aux_send_sock != -1) && (storeID != 0)) {
		passMessage(message, aux_send_sock);
		printf("PASSING: ID %d passing to %d\n", ID, ID+3);
	}
    //can't pass
	else {
        returnResult(message, success, value);
    }
    freeMessage(message);
    return 1;
}

//Pass the message to another machine in the system,
//where fd is the socket file descriptor for the target machine
int KVStore::passMessage(Message* message, int fd) {
    messages_passed++; 
    char* msg = buildMessage(message); 
    int len = (int)strlen(msg);
    printf("%d: PASSING: %s, WITH %d BYTES\n", ID, msg, len);
    if(send(fd, &len, sizeof len, 0) == -1) {
        perror("send");
        return 0;
    }
    if(sendAll(fd, msg, &len) == -1) {
        perror("sendall");
        return 0;
    }
    msg_passed_size += (long long) len;
    delete[] msg;
    return 1;
}

//AS SEEN ON BEEJ'S GUIDE: beej.us/guide/bgnet/output/html/multipage/index.html
//Call send until all data has been sent
int KVStore::sendAll(int fd, char * msg, int * len) {
    int total = 0;
    int remaining = *len;
    int n;

    while(total < (int) *len) {
        n = send(fd, msg + total, remaining, 0);
        if (n==-1) {break;}
        total += n;
        remaining -= n;
    }
    
    *len = total;
    return n==-1 ? -1 : 0;
}

//Return the result of the operations to the client;
//0 for failure, 1 for success;
//value "NULL" if not lookup
int KVStore::returnResult(Message * message, int success, char* value) {
    int numbytes;
    char* returnString = buildReturn(success, value);
    if(success == 1) {
        if(value != NULL) {
            printf("OPERATION: %s succeeded; Value %s retrieved\n", message->oper, value);
        } else {
            printf("OPERATION: %s succeeded; no value retrieved\n", message->oper);
        }
    } else {
        printf("OPERATION: %s failed\n", message->oper);
    }
    //build socket, send message
    Socket* sock = new Socket;
	int stfd = newRequestSocket(sock, message->ret_ip, message->ret_port, 0);
    if((numbytes = sendto(stfd, returnString, strlen(returnString), 0,
                            sock->ptr->ai_addr, sock->ptr->ai_addrlen)) == -1) {
        perror("sendto: error sending message to requester");
    }
    close(stfd);
    freeaddrinfo(sock->servinfo);
    delete[] returnString;
    delete sock;
    return success;
}

//Build the return string to send to the client
char* KVStore::buildReturn(int success, char* value) {
	char * toReturn;
	if (value == NULL)
		toReturn = new char[34];
	else
		toReturn = new char[strlen(value) + 34];
    char buf[33];
    snprintf(buf, sizeof(buf), "%d", success);
    strcpy(toReturn, buf);
	if (value != NULL) {
        strcat(toReturn, ":");
		strcat(toReturn, value);
    } else {
        strcat(toReturn, ":");
        strcat(toReturn, "null");
    }
    return toReturn;
}

//process a message into a message structure
int KVStore::processMessage(char * msg, Message* message, int request) {
    char* tokens[7];
    int i;
    char * first = strtok(msg, ":");
    tokens[0] = first;
    for(i = 1; i < 7; i++) {
        tokens[i] = strtok(NULL, ":");
    }
    message->ret_ip = new char[strlen(tokens[0])+1];
    strcpy(message->ret_ip, tokens[0]);
    message->ret_port = new char[strlen(tokens[1])+1];
    strcpy(message->ret_port, tokens[1]);
    
    char buf[33];
    snprintf(buf, sizeof(buf), "%d", ID);

    //if it's an outside request
    if(request == 1) {
        message->orig_id = new char[strlen(buf)+1];
        strcpy(message->orig_id, buf);
        message->send_id = new char[strlen(buf)+1];
        strcpy(message->send_id, buf);
    } else {
        message->orig_id = new char[strlen(tokens[2])+1];
        strcpy(message->orig_id, tokens[2]);
        message->send_id = new char[strlen(tokens[3])+1];
        strcpy(message->send_id, tokens[3]);
    }

    message->oper = new char[strlen(tokens[4])+1];
    strcpy(message->oper, tokens[4]);
    message->key = new char[strlen(tokens[5])+1];
    strcpy(message->key, tokens[5]);
    message->value = new char[strlen(tokens[6])+1];
    strcpy(message->value, tokens[6]);
    return 1;
}

//Build a messages string from a Message structure
char* KVStore::buildMessage(Message* message) {
    char buf[33];
    snprintf(buf, sizeof(buf), "%d", ID);
    char newbuf[strlen(buf)+1];
    strcpy(newbuf, buf);
    int length = strlen(message->ret_ip) +
                    strlen(message->ret_port) + 
                    strlen(message->orig_id) + 
                    strlen(newbuf) + 
                    strlen(message->oper) + 
                    strlen(message->key) + 
                    strlen(message->value) +
                    SEMICOLONS + 1;
    char* msg = new char[length];
    strcpy(msg, message->ret_ip);
    strcat(msg, ":");
    strcat(msg, message->ret_port);
    strcat(msg, ":");
    strcat(msg, message->orig_id);
    strcat(msg, ":");
    //key
    strcat(msg, newbuf);
    strcat(msg, ":");
    strcat(msg, message->oper);
    strcat(msg, ":");
    strcat(msg, message->key);
    strcat(msg, ":");
    strcat(msg, message->value);
    return msg;
}

//Determine if a key is in the range of the machine with ID id
int KVStore::inRange(int key, int id) {
    if((key < 0*UPRANGE) || (key > 4*UPRANGE)) {
        return -1;
    }
    if((key <= id * UPRANGE) && (key >= (id * UPRANGE) - (UPRANGE - 1))) {
        return 0;
    }
	if((key <= ((id % 4) + 1) * UPRANGE) && (key >= (((id % 4) + 1) * UPRANGE) - (UPRANGE - 1))) {
        return 1;
    }
	if((key <= (((id+1)%4)+1) * UPRANGE) && (key >= ((((id+1)%4)+1) * UPRANGE) - (UPRANGE - 1))) {
        return 2;
    }
    return -1;
}

//Determine if a message has cycled
bool KVStore::hasCycled(Message * message) {
    int orig_id = atoi(message->orig_id);
    if((orig_id == ids[0]) ) {
        return true;
    } else {
        return false;
    }
}

//Set up the sockets used for send()
int KVStore::setupSendSocks() {
	alive[0] = true;
	alive[1] = true;
    Socket * sock = new Socket;
    send_socks[0] = newSendSocket(sock, ips[0], ports[0]);
    freeaddrinfo(sock->servinfo);
    send_socks[1] = newSendSocket(sock, ips[1], ports[1]);
    freeaddrinfo(sock->servinfo);
    int i;
    int id;
    for(i = 0; i < 2; i++) {
        printf("send_socks[%d]: %d\n", i, send_socks[i]);
        if(send_socks[i] == -1) {
            alive[i] = false;
        } else {
            id = ID;
            if(send(send_socks[i], &id, sizeof id, 0) == -1) {
                perror("send: setup");
            }
        }
    }
    delete sock;
    return 1;
}

//Free a message struct
int KVStore::freeMessage(Message * message) {
    delete[] message->ret_ip;
    delete[] message->ret_port;
    delete[] message->orig_id;
    delete[] message->send_id;
    delete[] message->oper;
    delete[] message->key;
    delete[] message->value;
    delete message;
    return 1;
}

//Mark a node as failed if necessary
int KVStore::markFailed(int nodeId) {
    int i;
    for(i = 0; i < 3; i++) {
        if(ids[i] == nodeId) {
            if(alive[i]) {
                alive[i] = false;
            }
        }
    }
    if(areTwoFailures()) {
        printf("ARE TWO FAILURES\n");
        Socket * sock = new Socket;
        aux_send_sock = newSendSocket(sock, ips[2], ports[2]);
        freeaddrinfo(sock->servinfo);
        delete sock;
    }
	return 0;
}

int KVStore::markAlive(int nodeId) {
    //get rid of auxiliary
    if(areTwoFailures()) {
        close(aux_send_sock);
        aux_send_sock = -1;
    }
    int i;
    int index;
    int id;
    for(i = 0; i < 3; i++) {
        if(ids[i] == nodeId) {
            index = i;
            if(!alive[i]) {
                alive[i] = true;
                if(i != 2) {
                    printf("CONNECTING TO JOINING NODE\n");
                    Socket * sock = new Socket;
                    send_socks[index] = newSendSocket(sock, ips[index], ports[index]);
                    id = ID;
                    if(send(send_socks[i], &id, sizeof id, 0) == -1) {
                        perror("send: setup");
                    }
                    freeaddrinfo(sock->servinfo);
                    delete sock;
                }
                //send data
                //if node is ids[1]: send your data; if ids[0] is dead, also send ids[1]'s data
                if(nodeId == ids[1]) {
                    sendData(0, ids[1], ids[1]);
                    if(alive[0] == false) {
                        sendData(2, ids[1], ids[1]);
                    }
                //else if node is ids[0]: send data of ids[0] and ids[1]
                } else if (nodeId == ids[0]) {
                    sendData(1, ids[0], ids[0]);
                    sendData(2, ids[0], ids[0]);
                } else if (nodeId == ids[2]) {
                    if((alive[0]) && (!alive[1])) {
                        //send own data to ids[0] for ids[2]
                        sendData(0, ids[0], ids[2]);
                    } else if((!alive[0]) && (alive[1])) {
                        //send data of ids[0] to ids[1] for ids[2]
                        sendData(1, ids[1], ids[2]);
                    }
                }
            }
        }
    }
    return 0;
}

//check if the two we're talking to have failed
bool KVStore::areTwoFailures() {
    return (!alive[0] && !alive[1]);
}

//need passing and packing methods for data; unpacking methods; storing of unpacked data methods
//
//pass or unpack & store
int KVStore::handleData(string& data) {
    printf("HANDLING DATA\n");
    int destId;
    if(data[4] >= '0' && data[4] <= '9') {
        destId = data[4] - '0';
    }
    if(ID == destId) {
        unpackData(data);
    } else {
       passData(data, destId);
    }
    return 1;
}

//take the data, and move it to the intended destination
int KVStore::passData(string& data, int destId) {
    printf("PASSING DATA TO %d\n", destId);
    int len = data.length() + 1;
    data_sent_size += (long long) len;

    int fd = getFdFromId(destId);
    char * datastr = new char[data.length() + 1];
    strcpy(datastr, data.c_str());

    if(send(fd, &len, sizeof len, 0) == -1) {
        perror("send");
        return 0;
    }
    if(sendAll(fd, datastr, &len) == -1) {
        perror("sendall");
        return 0;
    }   
    delete[] datastr;
    return 1;
}

//send data in map at storeID to target
//called from markAlive()... and, badly, from mp3..
int KVStore::sendData(int storeId, int targetId, int destId) {
    string data;
    packData(storeId, destId, data);

    int len = data.length() + 1;
    data_sent_size += (long long)len;

    printf("SENDING Data to %d; dest %d; %d bytes\n", targetId, destId, len);
    int fd = getFdFromId(targetId); 

    if(send(fd, &len, sizeof len, 0) == -1) {
        perror("send");
        return 0;
    }
    char * datastr = new char[len];
    strcpy(datastr, data.c_str());
    if(sendAll(fd, datastr, &len) == -1) {
        perror("sendall");
        return 0;
    }   
    delete[] datastr;
    return 1;
}

//pack the data from a map into a string
//front has format d:idofdata:idofdest;k:v;...
int KVStore::packData(int storeId, int destId, string& data) {
    printf("Packing data in MAP %d\n", storeId);
    map<int, char*>::iterator it;
    int dataId = getIdFromStoreId(storeId);

    stringstream stream; 
    string dataIdStr;
    stream << dataId;
    dataIdStr = stream.str();
    stream.str("");

    string destIdStr;
    stream << destId;
    destIdStr = stream.str();
    stream.str("");

    string header = "d:" + dataIdStr + ":" + destIdStr + ";";
    data.append(header);

    string first;
    for(it = store[storeId].begin(); it != store[storeId].end(); it++ ) {
        stream << (*it).first;
        first = stream.str();
        stream.str("");
        data.append(first);
        data.append(":");
        data.append((*it).second);
        data.append(";");
    }
    return 1;
}

//unpack the data into the appropriate map
int KVStore::unpackData(string& data) {
    printf("Unpacking data\n");
    int dataId;
    if(data[2] >= '0' && data[2] <= '9') {
        dataId = data[2] - '0';
    }
    int storeId = getStoreIdFromDataId(dataId);
    int intKey;
    istringstream tokenizer(data);
    string head;
    string token;

    string key;
    string value;

    getline(tokenizer, head, ';');

    //get k:v pairs; insert(storeId, key, value)
    while(getline(tokenizer, token, ';')) {
        istringstream kvhandler(token);
        getline(kvhandler, key, ':');
        getline(kvhandler, value, ':');
        char kbuf[key.length() + 1];
        strcpy(kbuf, key.c_str());
        intKey = atoi(kbuf);
        char vbuf[value.length() + 1];
        strcpy(vbuf, value.c_str());
        insert(storeId, intKey, vbuf); 
    }

    return 1;

}

//get send socket fd from the target id
int KVStore::getFdFromId(int targetId) {
    if(targetId == ids[0]) {
        return send_socks[0];
    }
    if(targetId == ids[1]) {
        return send_socks[1];
    }
    return -1;
}

//get the actual id from the storage id
int KVStore::getIdFromStoreId(int storeId) {
    if(storeId == 0) {
        return ID;
    } else if(storeId == 1) {
        return ids[0];
    } else if(storeId == 2) {
        return ids[1];
    } else {
        return -1;
    }
}

//get the storage id from the data id
int KVStore::getStoreIdFromDataId(int dataId) {
    if(dataId == ID) {
        return 0;
    } else if (dataId == ids[0]) {
        return 1;
    } else if (dataId == ids[1]) {
        return 2;
    } else {
        //error!
        return -1;
    }
}
