#include "server.h"
#include "constants.h"
#include "iostream"
#include "vector"
#include "string"
#include "fstream"
#include "sstream"
#include "unistd.h"
#include "signal.h"
#include "errno.h"
#include "sys/socket.h"
using namespace std;

typedef pair<int, Proposal> SPtuple;

#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

extern std::vector<string> split(const std::string &s, char delim);
extern void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread);

int Server::get_leader_fd(const int server_id) {
    return leader_fd_[server_id];
}

int Server::get_leader_port(const int server_id) {
    return leader_port_[server_id];
}

void Server::set_leader_fd(const int server_id, const int fd) {
    if (fd == -1 || leader_fd_[server_id] == -1) {
        leader_fd_[server_id] = fd;
    }
}

/**
 * checks if given port corresponds to a leader's port
 * @param  port port number to be checked
 * @return      id of server whose leader port matches param port
 * @return      -1 if param port is not the leader port of any server
 */
int Server::IsLeaderPort(const int port) {
    if (leader_port_map_.find(port) != leader_port_map_.end()) {
        return leader_port_map_[port];
    } else {
        return -1;
    }
}

/**
 * thread entry function for leader
 * @param  _S pointer to server class object
 * @return    NULL
 */
void *LeaderEntry(void *_S) {
    Server *S = (Server*) _S;

    // does not need accept threads since it does not listen to connections from anyone

    // sleep for some time to make sure accept threads of commanders,scouts,replica are running
    usleep(kGeneralSleep);
    if (S->ConnectToCommanderL(S->get_primary_id())) {
        D(cout << "SL" << S->get_pid() << ": Connected to commander of S"
          << S->get_primary_id() << endl;)
    } else {
        D(cout << "SL" << S->get_pid() << ": ERROR in connecting to commander of S"
          << S->get_primary_id() << endl;)
        return NULL;
    }

    if (S->ConnectToScoutL(S->get_primary_id())) {
        D(cout << "SL" << S->get_pid() << ": Connected to scout of S"
          << S->get_primary_id() << endl;)
    } else {
        D(cout << "SL" << S->get_pid() << ": ERROR in connecting to scout of S"
          << S->get_primary_id() << endl;)
        return NULL;
    }

    if (S->ConnectToReplica(S->get_primary_id())) { // same as S->get_pid()
        D(cout << "SL" << S->get_pid() << ": Connected to replica of S"
          << S->get_primary_id() << endl;)
    } else {
        D(cout << "SL" << S->get_pid() << ": ERROR in connecting to replica of S"
          << S->get_primary_id() << endl;)
        return NULL;
    }

}