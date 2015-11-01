#ifndef SERVER_H_
#define SERVER_H_
#include "vector"
#include "string"
#include "map"
using namespace std;

void* ReceiveMessagesFromClient(void* _rcv_thread_arg);

struct Proposal {
    string client_id;
    string chat_id;
    string msg;

    Proposal() { }
    Proposal(const string &client_id,
             const string &chat_id,
             const string &msg)
        : client_id(client_id),
          chat_id(chat_id),
          msg(msg) { }
};

struct Ballot {
    int id;
    int seq_num;

    Ballot() { }
    Ballot(int i, int s): id(i), seq_num(s) {}

    bool operator>(const Ballot &b2) const;
    bool operator<(const Ballot &b2) const;
    bool operator==(const Ballot &b2) const;
    bool operator>=(const Ballot &b2) const;
    bool operator<=(const Ballot &b2) const;

};

struct Triple {
    Ballot b;
    int s;
    Proposal p;
};

class Server {
public:
    void Initialize(const int pid,
                    const int num_servers,
                    const int num_clients);
    int IsReplicaPort(const int port);
    int IsAcceptorPort(const int port);
    int IsLeaderPort(const int port);
    int IsClientChatPort(const int port);
    bool ReadPortsFile();
    void CreateReceiveThreadsForClients();
    bool ConnectToServer(const int server_id);
    void IncrementSlotNum();
    void IncrementBallotNum();
    void CommanderAcceptThread();
    void ScoutAcceptThread();
    void Propose(const Proposal &p);
    bool ConnectToCommanderL(const int server_id);
    bool ConnectToCommanderR(const int server_id);
    bool ConnectToCommanderA(const int server_id);
    bool ConnectToScoutL(const int server_id);
    bool ConnectToScoutR(const int server_id);
    bool ConnectToScoutA(const int server_id);
    bool ConnectToReplica(const int server_id);


    int get_pid();
    int get_commander_fd(const int server_id);
    int get_scout_fd(const int server_id);
    int get_replica_fd(const int server_id);
    int get_acceptor_fd(const int server_id);
    int get_leader_fd(const int server_id);
    int get_client_chat_fd(const int client_id);
    int get_master_port();
    int get_server_listen_port(const int server_id);
    int get_client_listen_port(const int client_id);
    int get_commander_listen_port(const int client_id);
    int get_scout_listen_port(const int client_id);
    int get_replica_listen_port(const int client_id);
    int get_client_chat_port(const int client_id);
    int get_acceptor_port(const int client_id);
    int get_replica_port(const int client_id);
    int get_leader_port(const int client_id);
    int get_primary_id();
    int get_slot_num();
    Ballot get_ballot_num();

    void set_commander_fd(const int server_id, const int fd);
    void set_scout_fd(const int server_id, const int fd);
    void set_replica_fd(const int server_id, const int fd);
    void set_acceptor_fd(const int server_id, const int fd);
    void set_leader_fd(const int server_id, const int fd);
    void set_client_chat_fd(const int client_id, const int fd);
    void set_pid(const int pid);
    void set_master_fd(const int fd);
    void set_primary_id(const int primary_id);
    void set_slot_num(const int slot_num);
    void set_ballot_num(const Ballot &ballot_num);

private:
    int pid_;   // server's ID
    int num_servers_;
    int num_clients_;
    int primary_id_;
    int slot_num_;
    Ballot ballot_num_;

    std::map<int, Proposal> proposals_;  // map of s,Proposal
    std::map<int, Proposal> decisions_;

    int master_fd_;
    std::vector<int> commander_fd_;
    std::vector<int> scout_fd_;
    std::vector<int> replica_fd_;
    std::vector<int> acceptor_fd_;
    std::vector<int> leader_fd_;
    std::vector<int> client_chat_fd_;

    int master_port_;   // port used by master for communication
    std::vector<int> server_listen_port_;
    std::vector<int> client_listen_port_;
    std::vector<int> commander_listen_port_;
    std::vector<int> scout_listen_port_;
    std::vector<int> replica_listen_port_;

    std::vector<int> client_chat_port_;
    std::vector<int> acceptor_port_;
    std::vector<int> replica_port_;
    std::vector<int> leader_port_;

    std::map<int, int> client_chat_port_map_;
    std::map<int, int> acceptor_port_map_;
    std::map<int, int> replica_port_map_;
    std::map<int, int> leader_port_map_;

};

struct ReceiveThreadArgument {
    Server *S;
    int client_id;
};

#endif //SERVER_H_