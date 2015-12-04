#ifndef ACCEPTOR_H_
#define ACCEPTOR_H_

#include "server.h"
#include "utilities.h"
#include "vector"
#include "string"
#include "unordered_set"
#include "map"
#include "set"
using namespace std;

void* AcceptConnectionsAcceptor(void* _A);
void *AcceptorEntry(void *_S);

class Acceptor {
public:
    bool ConnectToScout(const int server_id);
    void GetCommanderFdSet(fd_set&, int&, std::vector<int> &cfds_vec);
    void AddToCommanderFDSet(const int fd);
    void RemoveFromCommanderFDSet(const int fd);
    void SendBackOwnFD(const int fd);
    void AcceptorMode(const int primary_id);
    void SendP1b(const Ballot& b, const unordered_set<Triple> &st,
                 const int primary_id);
    void SendP2b(const Ballot& b, int return_fd, const int primary_id);
    void Unicast(const string &type, const string& msg,
                 const int primary_id, int r_fd = -1);

    int get_scout_fd(const int server_id);
    set<int> get_commander_fd_set();
    Ballot get_best_ballot_num();

    void set_scout_fd(const int server_id, const int fd);
    void set_best_ballot_num(const Ballot &b);

    Acceptor(Server *_S);

    Server *S;
    ~Acceptor();
private:
    Ballot best_ballot_num_;
    std::unordered_set<Triple> accepted_;

    std::vector<int> scout_fd_;
    std::set<int> commander_fd_set_;

};

#endif //ACCEPTOR_H_
