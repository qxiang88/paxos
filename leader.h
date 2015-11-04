#ifndef LEADER_H_
#define LEADER_H_
#include "server.h"
#include "vector"
#include "string"
#include "unordered_set"
#include "map"
#include "utilities.h"
#include "set"
using namespace std;

void *LeaderEntry(void *_S);

class Leader {
public:
    bool ConnectToCommander(const int server_id);
    bool ConnectToScout(const int server_id);
    bool ConnectToReplica(const int server_id);
    void LeaderMode();
    void IncrementBallotNum();
    void GetFdSet(fd_set& recv_from_set, int& fd_max);


    int get_commander_fd(const int server_id);
    int get_scout_fd(const int server_id);
    int get_replica_fd(const int server_id);
    Ballot get_ballot_num();
    bool get_leader_active();

    void set_commander_fd(const int server_id, const int fd);
    void set_scout_fd(const int server_id, const int fd);
    void set_replica_fd(const int server_id, const int fd);
    void set_ballot_num(const Ballot &ballot_num);
    void set_leader_active(const bool b);

    Leader(Server* S);

    Server *S;
    std::map<int, Proposal> proposals_;

private:
    Ballot ballot_num_;
    bool leader_active_;

    std::vector<int> commander_fd_;
    std::vector<int> scout_fd_;
    std::vector<int> replica_fd_;


};

#endif //LEADER_H_