#ifndef SCOUT_H_
#define SCOUT_H_

#include "server.h"
#include "utilities.h"
#include "vector"
#include "string"
#include "unordered_set"
#include "map"
#include "set"
using namespace std;

class Server;

void* AcceptConnectionsScout(void* _SC);
void* ScoutMode(void* _rcv_thread_arg);

class Scout {
public:
    int SendToServers(const string& type, const string& msg);
    void GetAcceptorFdSet(fd_set&, vector<int>&, int&);
    int SendP1a(const Ballot &b);
    void SendAdopted(const Ballot& recvd_ballot, unordered_set<Triple> pvalues);
    void SendPreEmpted(const Ballot& b);
    void Unicast(const string &type, const string& msg);
    void CloseAndUnSetAcceptor(int id);
    int GetServerIdFromFd(int fd);
    int CountAcceptorsAlive();

    int get_leader_fd(const int server_id);
    // int get_replica_fd(const int server_id);
    int get_acceptor_fd(const int server_id);

    void set_leader_fd(const int server_id, const int fd);
    // void set_replica_fd(const int server_id, const int fd);
    void set_acceptor_fd(const int server_id, const int fd);

    Scout(Server *_S);

    Server *S;
private:
    std::vector<int> leader_fd_;
    // std::vector<int> replica_fd_;
    std::vector<int> acceptor_fd_;
};

#endif //SCOUT_H_