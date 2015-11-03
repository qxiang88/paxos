#ifndef REPLICA_H_
#define REPLICA_H_

#include "server.h"
#include "utilities.h"
#include "vector"
#include "string"
#include "unordered_set"
#include "map"
#include "set"
using namespace std;

void* AcceptConnectionsReplica(void* _R);
void* ReceiveMessagesFromClient(void* _rcv_thread_arg);
void* ReplicaEntry(void *_S);

class Replica {
public:
    bool ConnectToCommander(const int server_id);
    bool ConnectToScout(const int server_id);
    void CreateReceiveThreadsForClients();
    void Propose(const Proposal &p);
    void SendProposal(const int& s, const Proposal& p);
    void Perform(const int& slot, const Proposal& p);
    void SendResponseToClient(const int& s, const Proposal& p);
    void IncrementSlotNum();
    void ReplicaMode();
    void Unicast(const string &type, const string& msg);

    int get_slot_num();
    int get_commander_fd(const int server_id);
    int get_scout_fd(const int server_id);
    int get_leader_fd(const int server_id);
    int get_client_chat_fd(const int client_id);

    void set_slot_num(const int slot_num);
    void set_commander_fd(const int server_id, const int fd);
    void set_scout_fd(const int server_id, const int fd);
    void set_leader_fd(const int server_id, const int fd);
    void set_client_chat_fd(const int client_id, const int fd);

    Replica(Server *_S);

    Server *S;

private:
    int slot_num_;

    std::vector<int> commander_fd_;
    std::vector<int> scout_fd_;
    std::vector<int> leader_fd_;
    std::vector<int> client_chat_fd_;

};

struct ReceiveThreadArgument {
    Replica *R;
    int client_id;
};

#endif //REPLICA_H_
