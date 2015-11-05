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
void* ReplicaEntry(void *_S);
void* ReceiveMessagesFromReplicas(void* _R );

class Replica {
public:
    bool ConnectToCommander(const int server_id);
    bool ConnectToScout(const int server_id);
    bool ConnectToReplica(const int server_id);
    void Propose(const Proposal &p);
    void SendProposal(const int& s, const Proposal& p);
    void Perform(const int& slot, const Proposal& p);
    void SendResponseToAllClients(const int& s, const Proposal& p);
    void IncrementSlotNum();
    void ReplicaMode();
    void Unicast(const string &type, const string& msg);
    void ProposeBuffered();
    void CheckReceivedAllDecisions(map<int, Proposal>& allDecisions);
    void CreateFdSet(fd_set& fromset, vector<int> &fds, int& fd_max);
    void RecoverDecisions();
    void GetReplicaFdSet(fd_set& server_fd_set, int& fd_max);
    int GetReplicaIdWithFd(int fd);
    void SendDecisionsRequest();
    void SendDecisionsResponse(int);
    void MergeDecisions(map<int, Proposal>);
    void DecisionsRecoveryMode();
    int get_slot_num();
    int get_commander_fd(const int server_id);
    int get_scout_fd(const int server_id);
    int get_leader_fd(const int server_id);
    int get_client_chat_fd(const int client_id);
    int get_replica_fd(const int server_id);
    vector<int> get_replica_fd_set();
    map<int, Proposal> get_decisions();

    void set_slot_num(const int slot_num);
    void set_commander_fd(const int server_id, const int fd);
    void set_scout_fd(const int server_id, const int fd);
    void set_leader_fd(const int server_id, const int fd);
    void set_client_chat_fd(const int client_id, const int fd);
    void set_replica_fd(const int client_id, const int fd);
    void set_recovery(bool val);
    void set_decisions(map<int, Proposal>& d);

    Replica(Server *_S);

    Server *S;
    std::map<int, Proposal> proposals_;
    std::map<int, Proposal> decisions_;

private:
    int slot_num_;

    std::vector<int> commander_fd_;
    std::vector<int> scout_fd_;
    std::vector<int> leader_fd_;
    std::vector<int> client_chat_fd_;
    std::vector<int> replica_fd_;
    bool recovery_;
    vector<Proposal> buffered_proposals_;
};

struct ReceiveThreadArgument {
    Replica *R;
    int client_id;
};

#endif //REPLICA_H_
