#ifndef SERVER_H_
#define SERVER_H_
#include "vector"
#include "string"
#include "map"
using namespace std;

void* ReceiveMessagesFromClient(void* _rcv_thread_arg);

struct Command {
    string client_id;
    string chat_id;
    string msg;

    Command() { }
    Command(const string &client_id,
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
    Command p;
};

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
    void IncrementSlotNum();
    void IncrementBallotNum();
    void Propose(const Command &p);

    int get_pid();
    int get_server_paxos_fd(const int server_id);
    int get_client_chat_fd(const int client_id);
    int get_master_port();
    int get_server_listen_port(const int server_id);
    int get_client_listen_port(const int client_id);
    int get_server_paxos_port(const int server_id);
    int get_leader_id();
    int get_slot_num();
    Ballot get_ballot_num();

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

    std::map<int, Command> proposals_;  // map of s,Command
    std::map<int, Command> decisions_;

    int master_fd_;
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

#endif //SERVER_H_