#ifndef KVSTORE_H_
#define KVSTORE_H_

using namespace std;

#include <map>
#include <string>
#include <sstream>
#include <algorithm>
#include <sys/socket.h>
#include "socket.h"

//Struct for holding a parsed message string
typedef struct {
    char* ret_ip,* ret_port, * orig_id, * send_id, * oper, * key, * value;
} Message;

class KVStore {
public:
//member vars
    int ID;
    char* my_port;
    char* req_port;
    int ids[3];
    char* ips[3];
    char* ports[3];
    //alive tracks life of those in front of you in ring
    bool alive[3];

    int req_sock;
    int send_socks[2];
    int accept_socks[2];

    int aux_send_sock;
    int aux_recv_sock;

    int listen_sock;

//data storage
    map<int, char*> store[3];
	
//metrics info
    int messages_passed;
    long long msg_passed_size;
    long long data_received_size;
    long long data_sent_size;

    int data_stored_size;

//functions
    KVStore(int id);
    ~KVStore();
    
    int insert(int storeID, int key, char* value);
    int remove(int storeID, int key);
    char* lookup(int storeID, int key);

    int passMessage(Message* message, int fd); 
    int receiveMessage(int fd);
    int bytesToReceive(int len, int total);

    int receiveRequest();
    int returnResult(Message * message, int success, char* value);
    int processMessage(char* msg, Message* message, int request);
    int inRange(int key, int id);
    int handleMessage(char* msg, int request);
    bool hasCycled(Message * message);
    int freeMessage(Message * message);
    char* buildMessage(Message* message);
    char* buildReturn(int success, char* value);
    int setupSendSocks();
    int sendAll(int fd, char* msg, int * len);
    int markFailed(int nodeId);
    int markAlive(int nodeId);
    bool areTwoFailures();

    int handleData(string& data);
    int passData(string& data, int destId);
    int sendData(int storeId, int targetId, int destId);
    int packData(int storeId, int destId, string& data);
    int unpackData(string& data);
    int getFdFromId(int targetId);
    int getIdFromStoreId(int storeId);
    int getStoreIdFromDataId(int dataId);
};

#endif

