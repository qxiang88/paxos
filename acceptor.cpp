#include "acceptor.h"
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

// #define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

extern void* AcceptConnectionsAcceptor(void* _S);

Acceptor::Acceptor(Server* _S) {
    S = _S;
    set_best_ballot_num(Ballot(INT_MIN, INT_MIN));

    scout_fd_.resize(S->get_num_servers(), -1);
}


int Acceptor::get_scout_fd(const int server_id) {
    return scout_fd_[server_id];
}

set<int> Acceptor::get_commander_fd_set() {
    return commander_fd_set_;
}

Ballot Acceptor::get_best_ballot_num() {
    return best_ballot_num_;
}

void Acceptor::set_scout_fd(const int server_id, const int fd) {
    scout_fd_[server_id] = fd;
}

void Acceptor::set_best_ballot_num(const Ballot &b) {
    best_ballot_num_.id = b.id;
    best_ballot_num_.seq_num = b.seq_num;
}

/**
 * adds the fd for communication with a commander to commander_fd_set_
 * @param fd fd to be added
 */
 void Acceptor::AddToCommanderFDSet(const int fd) {
    commander_fd_set_.insert(fd);
}

/**
 * removes the fd for communication with a commander from commander_fd_set_
 * @param fd fd to be removed
 */
 void Acceptor::RemoveFromCommanderFDSet(const int fd) {
    close(fd);
    commander_fd_set_.erase(fd);
}

/**
 * constructs an fd_set from the commander_fd_set and computes max_fd
 * @param cfds_set [out] constructed fd_set from the commander_fd_set
 * @param cfds_vec [out] vector correponding to the constructed fd_set
 * @param fd_max   [out] maximum value of fd in the constructed fd_set
 */
 void Acceptor::GetCommanderFdSet(fd_set& cfds_set, vector<int>& cfds_vec, int& fd_max)
 {
    int fd_temp;
    fd_max = INT_MIN;
    set<int> local_set = get_commander_fd_set();
    FD_ZERO(&cfds_set);
    cfds_vec.clear();
    for (auto it = local_set.begin(); it != local_set.end(); it++)
    {
        fd_temp = *it;
        if (fd_temp != -1)
        {
            FD_SET(fd_temp, &cfds_set);
            fd_max = max(fd_max, fd_temp);
            cfds_vec.push_back(fd_temp);
        }
    }
}

/**
 * sends back the acceptor side's fd for commander-acceptor connection
 * back to the commander
 * @param fd acceptor side's fd for commander-acceptor connection
 */
 void Acceptor::SendBackOwnFD(const int fd) {
    string msg = to_string(fd);
    if (send(fd, msg.c_str(), msg.size(), 0) == -1) {
        D(cout << "SA" << S->get_pid() << ": ERROR: Cannot send fd to commander" << endl;)
        //TODO: delete this fd from the commander fd set?
        RemoveFromCommanderFDSet(fd);
    } else {
        D(cout << "SA" << S->get_pid() << ": fd sent to commander" << endl;)
    }
}

void Acceptor::Unicast(const string &type, const string& msg, int r_fd)
{
    int serv_fd;
    if (r_fd == -1)
        serv_fd = get_scout_fd(S->get_primary_id());
    else
        serv_fd = r_fd;

    if (send(serv_fd, msg.c_str(), msg.size(), 0) == -1) {
        D(cout << "SA" << S->get_pid() << ": ERROR in sending " << type << endl;)
        if (serv_fd == get_scout_fd(S->get_primary_id())) {
            close(serv_fd);
            set_scout_fd(S->get_primary_id(), -1);
        } else {
            RemoveFromCommanderFDSet(serv_fd);
        }
    }
    else {
        D(cout << "SA" << S->get_pid() << ": " << type << " message sent: " << msg << endl;)
    }
}

/**
 * sends phase 1B message to scout
 * @param b  current best ballot num of acceptor
 * @param st accepted set of acceptor
 */
 void Acceptor::SendP1b(const Ballot& b, const unordered_set<Triple> &st)
 {
    string msg = kP1b + kInternalDelim + to_string(S->get_pid());
    msg += kInternalDelim + ballotToString(b) + kInternalDelim;
    msg += tripleSetToString(st) + kMessageDelim;
    Unicast(kP1b, msg);
}

/**
 * sends phase 2B message to commander
 * @param b         current best ballot num of acceptor
 * @param return_fd acceptor side fd for the commander-acceptor connection
 */
 void Acceptor::SendP2b(const Ballot& b, int return_fd)
 {
    string msg = kP2b + kInternalDelim + to_string(S->get_pid());
    msg += kInternalDelim + ballotToString(b) + kMessageDelim;
    Unicast(kP2b, msg, return_fd);
}

/**
 * function for performing acceptor related job
 */
 void Acceptor::AcceptorMode()
 {
    int num_bytes;

    fd_set recv_from;
    vector<int> fds;

    while (true) {  // always listen to messages from the acceptors

        int fd_max, fd_temp;
        GetCommanderFdSet(recv_from, fds, fd_max);
        int primary_id = S->get_primary_id();
        fd_temp = get_scout_fd(primary_id);
        if (fd_temp != -1)
        {
            fds.push_back(fd_temp);
            FD_SET(fd_temp, &recv_from);
            fd_max = max(fd_max, fd_temp);
        }

        if (fds.empty()) {
            usleep(kBusyWaitSleep);
            continue;
        }

        struct timeval timeout = kSelectTimeoutTimeval;
        int rv = select(fd_max + 1, &recv_from, NULL, NULL, &timeout);

        if (rv == -1) { //error in select
            D(cout << "SA" << S->get_pid() << ": ERROR in select() for Acceptor" << endl;)
        } else if (rv == 0) {
            // D(cout << "SA" << S->get_pid() << ": Unexpected select timeout in Acceptor" << endl;)
        } else {
            for (int i = 0; i < fds.size(); i++) {
                if (FD_ISSET(fds[i], &recv_from)) { // we got one!!
                    char buf[kMaxDataSize];
                    if ((num_bytes = recv(fds[i], buf, kMaxDataSize - 1, 0)) == -1) {
                        D(cout << "SA" << S->get_pid() << ": ERROR in receiving from scout or commander" << endl;)
                        if (i != fds.size() - 1) {
                            RemoveFromCommanderFDSet(fds[i]);
                        } else { // scout fd
                            close(fds[i]);
                            set_scout_fd(primary_id, -1);
                        }
                    } else if (num_bytes == 0) {     //connection closed
                        D(cout << "SA" << S->get_pid() << ": Connection closed by scout or commander." << endl;)
                        if (i != fds.size() - 1) {
                            RemoveFromCommanderFDSet(fds[i]);
                        } else { // scout fd
                            close(fds[i]);
                            set_scout_fd(primary_id, -1);
                        }
                    } else {
                        buf[num_bytes] = '\0';
                        std::vector<string> message = split(string(buf), kMessageDelim[0]);
                        for (const auto &msg : message) {
                            std::vector<string> token = split(string(msg), kInternalDelim[0]);
                            if (token[0] == kP1a)
                            {
                                D(cout << "SA" << S->get_pid() << ": Received P1A message" <<  endl;)
                                Ballot recvd_ballot = stringToBallot(token[2]);
                                if (recvd_ballot > get_best_ballot_num())
                                    set_best_ballot_num(recvd_ballot);
                                SendP1b(get_best_ballot_num(), accepted_);
                            }
                            else if (token[0] == kP2a)
                            {
                                D(cout << "SA" << S->get_pid() << ": Received P2A message" <<  endl;)

                                int return_fd = stoi(token[1]);
                                Triple recvd_triple = stringToTriple(token[2]);
                                if (recvd_triple.b >= get_best_ballot_num())
                                {
                                    set_best_ballot_num(recvd_triple.b);
                                    accepted_.insert(recvd_triple);
                                }
                                SendP2b(get_best_ballot_num(), return_fd);
                            }
                            else {    //other messages
                                D(cout << "SA" << S->get_pid() << ": Unexpected message received: " << msg << endl;)
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * thread entry function for acceptor
 * @param  _S pointer to server class object
 * @return    NULL
 */
 void* AcceptorEntry(void *_S) {
    Acceptor A((Server*)_S);

    pthread_t accept_connections_thread;
    CreateThread(AcceptConnectionsAcceptor, (void*)&A, accept_connections_thread);

    int primary_id = A.S->get_primary_id();

    // sleep for some time to make sure accept threads of scouts are running
    usleep(kGeneralSleep);
    usleep(kGeneralSleep);
    if (A.ConnectToScout(primary_id)) {
        D(cout << "SA" << A.S->get_pid() << ": Connected to scout of S"
          << primary_id << endl;)
    } else {
        D(cout << "SA" << A.S->get_pid() << ": ERROR in connecting to scout of S"
          << primary_id << endl;)
        return NULL;
    }

    A.AcceptorMode();
}