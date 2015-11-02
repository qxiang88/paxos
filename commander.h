#ifndef COMMANDER_H_
#define COMMANDER_H_

#include "server.h"
#include "utilities.h"
#include "vector"
#include "string"
#include "unordered_set"
#include "map"
#include "set"
using namespace std;

void* AcceptConnectionsCommander(void* _C);
void* CommanderMode(void* _rcv_thread_arg);

class Server;

class Commander {
public:
    bool ConnectToAcceptor(const int server_id);
    void SendP2a(const Triple & t, vector<int> acceptor_peer_fd);
    void SendDecision(const Triple & t);
    void SendPreEmpted(const Ballot& b);
    void SendToServers(const string& type, const string& msg);
    void ConnectToAllAcceptors(std::vector<int> &acceptor_peer_fd);
    void GetAcceptorFdSet(fd_set& acceptor_set, int& fd_max);

    int get_leader_fd(const int server_id);
    int get_replica_fd(const int server_id);
    int get_acceptor_fd(const int server_id);

    void set_leader_fd(const int server_id, const int fd);
    void set_replica_fd(const int server_id, const int fd);
    void set_acceptor_fd(const int server_id, const int fd);

    Commander(Server *_S);
    Commander(Server *_S, const int num_servers);

    Server *S;

private:
    static std::vector<int> leader_fd_;
    static std::vector<int> replica_fd_;
    std::vector<int> acceptor_fd_;
};

#endif //COMMANDER_H_
