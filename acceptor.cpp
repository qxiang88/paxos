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
#
extern void* AcceptConnectionsAcceptor(void* _S);

int Server::get_acceptor_fd(const int server_id) {
    return acceptor_fd_[server_id];
}

int Server::get_acceptor_port(const int server_id) {
    return acceptor_port_[server_id];
}

void Server::set_acceptor_fd(const int server_id, const int fd) {
    // if (fd == -1 || acceptor_fd_[server_id] == -1) {
        acceptor_fd_[server_id] = fd;
    // }
}

/**
 * adds the fd for communication with a commander to commander_fd_set_
 * @param fd fd to be added
 */
void Server::AddToCommanderFDSet(const int fd) {
    commander_fd_set_.insert(fd);
}

/**
 * removes the fd for communication with a commander from commander_fd_set_
 * @param fd fd to be removed
 */
void Server::RemoveFromCommanderFDSet(const int fd) {
    close(fd);
    commander_fd_set_.erase(fd);
}

/**
 * sends back the acceptor side's fd for commander-acceptor connection
 * back to the commander
 * @param fd acceptor side's fd for commander-acceptor connection
 */
void Server::SendBackOwnFD(const int fd) {
    string msg = to_string(fd);
    if (send(fd, msg.c_str(), msg.size(), 0) == -1) {
        D(cout << "SA" << get_pid() << ": ERROR: Cannot send fd to commander" << endl;)
        //TODO: delete this fd from the commander fd set?
        RemoveFromCommanderFDSet(fd);
    } else {
        D(cout << "SA" << get_pid() << ": fd sent to commander" << endl;)
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

    pthread_t accept_connections_thread;
    CreateThread(AcceptConnectionsAcceptor, (void*)S, accept_connections_thread);

    // sleep for some time to make sure accept threads of scouts are running
    usleep(kGeneralSleep);
    // if (S->ConnectToCommanderA(S->get_primary_id())) {
    //     D(cout << "SA" << S->get_pid() << ": Connected to commander of S"
    //       << S->get_primary_id() << endl;)
    // } else {
    //     D(cout << "SA" << S->get_pid() << ": ERROR in connecting to commander of S"
    //       << S->get_primary_id() << endl;)
    //     return NULL;
    // }

    if (S->ConnectToScoutA(S->get_primary_id())) {
        D(cout << "SA" << S->get_pid() << ": Connected to scout of S"
          << S->get_primary_id() << endl;)
    } else {
        D(cout << "SA" << S->get_pid() << ": ERROR in connecting to scout of S"
          << S->get_primary_id() << endl;)
        return NULL;
    }

    S->Acceptor();
}
void Server::SendP1b(const Ballot& b, const unordered_set<Triple> &st)
{
    string msg = kP1b + kInternalDelim + to_string(get_pid());
    msg += kInternalDelim + ballotToString(b) + kInternalDelim;
    msg += tripleSetToString(st) + kMessageDelim;
    Unicast(kP1b, msg);
}

void Server::SendP2b(const Ballot& b, int return_fd)
{
    string msg = kP2b + kInternalDelim + to_string(get_pid());
    msg += kInternalDelim + ballotToString(b) + kMessageDelim;
    Unicast(kP2b, msg, return_fd);
}

void Server::Acceptor()
{
    // accepted_.clear();
    ballot_num_.id = INT_MIN;
    ballot_num_.seq_num = INT_MIN;

    char buf[kMaxDataSize];
    int num_bytes;

    fd_set recv_from;
    vector<int> fds;
    
    // fd_temp = get_commander_fd(get_primary_id());

    while (true) {  // always listen to messages from the acceptors

        int fd_max, fd_temp;
        GetCommanderFdSet(recv_from, fds, fd_max);
        fd_temp = get_scout_fd(get_primary_id());
        if(fd_temp!=-1)
        {
            fds.push_back(fd_temp);
            FD_SET(fd_temp, &recv_from);
            fd_max = max(fd_max, fd_temp);
        }

        if(fds.empty()) {
            usleep(kSelectSleep);
            continue;
        }

        int rv = select(fd_max + 1, &recv_from, NULL, NULL, NULL);

        if (rv == -1) { //error in select
            D(cout << "SA" << get_pid() << ": ERROR in select() for Acceptor" << endl;)
        } else if (rv == 0) {
            D(cout << "SA" << get_pid() << ": Unexpected select timeout in Acceptor" << endl;)
            continue;
        } else {
            for (int i = 0; i < fds.size(); i++) {
                if (FD_ISSET(fds[i], &recv_from)) { // we got one!!
                    char buf[kMaxDataSize];
                    if ((num_bytes = recv(fds[i], buf, kMaxDataSize - 1, 0)) == -1) {
                        D(cout << "SA" << get_pid() << ": ERROR in receiving from scout or commander"<< endl;)
                        if(i!=fds.size()-1)
                            RemoveFromCommanderFDSet(fds[i]);
                    } else if (num_bytes == 0) {     //connection closed
                        D(cout << "SA" << get_pid() << ": Connection closed by scout or commander."<< endl;)
                        if(i!=fds.size()-1)
                            RemoveFromCommanderFDSet(fds[i]);
                    } else {
                        buf[num_bytes] = '\0';
                        std::vector<string> message = split(string(buf), kMessageDelim[0]);
                        for (const auto &msg : message) {
                            std::vector<string> token = split(string(msg), kInternalDelim[0]);
                            if (token[0] == kP1a)
                            {
                                D(cout << "SA" << get_pid() << ": Received P1a message" <<  endl;)

                                Ballot recvd_ballot = stringToBallot(token[2]);
                                if (recvd_ballot > ballot_num_)
                                    ballot_num_ = recvd_ballot;
                                SendP1b(ballot_num_, accepted_);
                            }
                            else if (token[0] == kP2a)
                            {
                                D(cout << "SA" << get_pid() << ": Received P2a message" <<  endl;)

                                int return_fd = stoi(token[1]);
                                Triple recvd_triple = stringToTriple(token[2]);
                                if (recvd_triple.b >= ballot_num_)
                                {
                                    ballot_num_ = recvd_triple.b;
                                    accepted_.insert(recvd_triple);
                                }
                                SendP2b(ballot_num_, return_fd);
                            }
                            else {    //other messages
                                D(cout << "SA" << get_pid() << ": Unexpected message in Acceptor " << msg << endl;)
                            }
                        }
                    }
                }
            }
        }
    }
}

//have to create for each server
void* AcceptorThread(void* _rcv_thread_arg) {
    AcceptorThreadArgument *rcv_thread_arg = (AcceptorThreadArgument *)_rcv_thread_arg;
    Server *S = rcv_thread_arg->S;
    S->Acceptor();
    return NULL;
}