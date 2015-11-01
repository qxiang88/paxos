#include "server.h"
#include "constants.h"
#include "utilities.h"
#include "iostream"
#include "vector"
#include "string"
#include "fstream"
#include "sstream"
#include "unistd.h"
#include "signal.h"
#include "errno.h"
#include "sys/socket.h"
#include "limits.h"
using namespace std;

typedef pair<int, Proposal> SPtuple;

#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

int Server::get_acceptor_fd(const int server_id) {
    return acceptor_fd_[server_id];
}

int Server::get_acceptor_port(const int server_id) {
    return acceptor_port_[server_id];
}

void Server::set_acceptor_fd(const int server_id, const int fd) {
    if (fd == -1 || acceptor_fd_[server_id] == -1) {
        acceptor_fd_[server_id] = fd;
    }
}

/**
 * checks if given port corresponds to a acceptor's port
 * @param  port port number to be checked
 * @return      id of server whose acceptor port matches param port
 * @return      -1 if param port is not the acceptor port of any server
 */
int Server::IsAcceptorPort(const int port) {
    if (acceptor_port_map_.find(port) != acceptor_port_map_.end()) {
        return acceptor_port_map_[port];
    } else {
        return -1;
    }
}

/**
 * thread entry function for acceptor
 * @param  _S pointer to server class object
 * @return    NULL
 */
void *AcceptorEntry(void *_S) {
    Server *S = (Server*) _S;

    // does not need accept threads since it does not listen to connections from anyone

    // sleep for some time to make sure accept threads of commanders and scouts are running
    usleep(kGeneralSleep);
    if (S->ConnectToCommanderA(S->get_primary_id())) {
        D(cout << "SA" << S->get_pid() << ": Connected to commander of S"
          << S->get_primary_id() << endl;)
    } else {
        D(cout << "SA" << S->get_pid() << ": ERROR in connecting to commander of S"
          << S->get_primary_id() << endl;)
        return NULL;
    }

    if (S->ConnectToScoutA(S->get_primary_id())) {
        D(cout << "SA" << S->get_pid() << ": Connected to scout of S"
          << S->get_primary_id() << endl;)
    } else {
        D(cout << "SA" << S->get_pid() << ": ERROR in connecting to scout of S"
          << S->get_primary_id() << endl;)
        return NULL;
    }
}

void Server::Acceptor()
{

}

//actually leader thread for each replica required. but here only one replica sends leader anything
void* AcceptorThread(void* _rcv_thread_arg) {
    AcceptorThreadArgument *rcv_thread_arg = (AcceptorThreadArgument *)_rcv_thread_arg;
    Server *S = rcv_thread_arg->S;
    S->Acceptor();
    return NULL;
}