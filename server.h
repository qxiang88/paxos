#ifndef SERVER_H_
#define SERVER_H_
#include "vector"
#include "string"
#include "unordered_set"
#include "map"
#include "utilities.h"
using namespace std;

void* ReceiveMessagesFromClient(void* _rcv_thread_arg);
void* Commander(void* _rcv_thread_arg);
void* Scout(void* _rcv_thread_arg);

class Server {
public:
    void Initialize(const int pid,
                    const int num_servers,
                    const int num_clients);
    int IsServerPaxosPort(const int port);
    int IsClientChatPort(const int port);
    bool ReadPortsFile();
    void CreateReceiveThreadsForClients();
    bool ConnectToServer(const int server_id);
    void ConnectToOtherServers();
    
    void SendP1a(const Ballot& b);
    void SendP2a(const Triple& t);
    void SendDecision(const Triple& t);
    void SendAdopted(const Ballot& recvd_ballot, unordered_set<Triple> pvalues);
    void SendPreEmpted(const Ballot& b);

    void SendToServers(const string& type, const string& msg);
    void SendToLeader(const string&);
    int GetMaxAcceptorFd();
    fd_set GetAcceptorFdSet();


    void IncrementSlotNum();
    void IncrementBallotNum();
    void Propose(const Proposal &p);

    int get_pid();
    int get_self_paxos_fd();
    
    int get_self_replica_fd();
    int get_self_acceptor_fd();
    int get_self_leader_fd();
    int get_num_servers();
    int get_replica_fd(const int server_id);
    int get_acceptor_fd(const int server_id);
    
    int get_server_paxos_fd(const int server_id);
    int get_client_chat_fd(const int client_id);
    int get_master_port();
    int get_server_listen_port(const int server_id);
    int get_client_listen_port(const int client_id);
    int get_server_paxos_port(const int server_id);
    int get_leader_id();
    int get_slot_num();
    Ballot get_ballot_num();

    void set_self_paxos_fd(const int fd);
    void set_server_paxos_fd(const int server_id, const int fd);
    void set_client_chat_fd(const int client_id, const int fd);
    void set_pid(const int pid);
    void set_master_fd(const int fd);
    void set_leader_id(const int leader_id);
    void set_slot_num(const int slot_num);
    void set_ballot_num(const Ballot &ballot_num);

private:
    int pid_;   // server's ID
    int num_servers_;
    int num_clients_;
    int leader_id_;
    int slot_num_;
    
    Ballot ballot_num_;
    bool leader_active_;

    std::map<int, Proposal> proposals_;  // map of s,Proposal
    std::map<int, Proposal> decisions_;

    int master_fd_;
    int self_paxos_fd_;
    std::vector<int> server_paxos_fd_;
    std::vector<int> client_chat_fd_;

    int master_port_;   // port used by master for communication
    std::vector<int> server_listen_port_;
    std::vector<int> client_listen_port_;
    std::vector<int> server_paxos_port_;
    std::vector<int> client_chat_port_;
    std::map<int, int> server_paxos_port_map_;
    std::map<int, int> client_chat_port_map_;

};

struct ReceiveThreadArgument {
    Server *S;
    int client_id;
};

struct CommanderThreadArgument {
    Server *S;
    Triple *toSend;
};

struct ScoutThreadArgument {
    Server *S;
    Ballot *ball;
};

struct LeaderThreadArgument {
    Server *S;
};

struct AcceptorThreadArgument {
    Server *S;
};


#endif //SERVER_H_