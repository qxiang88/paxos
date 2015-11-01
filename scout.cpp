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

extern void* AcceptConnectionsScout(void* _S);
extern std::vector<string> split(const std::string &s, char delim);
extern void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread);

int Server::get_scout_fd(const int server_id) {
    return scout_fd_[server_id];
}

int Server::get_scout_listen_port(const int server_id) {
    return scout_listen_port_[server_id];
}

void Server::set_scout_fd(const int server_id, const int fd) {
    if (fd == -1 || scout_fd_[server_id] == -1) {
        scout_fd_[server_id] = fd;
    }
}

/**
 * creates accept thread for scout
 */
void Server::ScoutAcceptThread() {
    pthread_t accept_connections_thread;
    CreateThread(AcceptConnectionsScout, (void*)this, accept_connections_thread);
}