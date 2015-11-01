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

extern void* AcceptConnectionsScout(void* _S);

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

void Server::SendP1a(const Ballot& b)
{
    string msg = kP1a + kInternalDelim + to_string(get_pid());
    msg += kInternalDelim + ballotToString(b) + kMessageDelim;
    SendToServers(kP1a, msg);
}

void Server::SendAdopted(const Ballot& recvd_ballot, unordered_set<Triple> pvalues) {
    string msg = kAdopted + kInternalDelim + ballotToString(recvd_ballot) + kInternalDelim;
    msg += tripleSetToString(pvalues) + kMessageDelim;
    Unicast(kAdopted, msg);
}


void* Scout(void* _rcv_thread_arg) {
    ScoutThreadArgument *rcv_thread_arg = (ScoutThreadArgument *)_rcv_thread_arg;
    Server *S = rcv_thread_arg->S;
    Ballot * ball = rcv_thread_arg->ball;

    S->SendP1a(*ball);

    int num_bytes;
    unordered_set<Triple> pvalues;
    fd_set acceptor_set = S->GetAcceptorFdSet();
    fd_set temp_set;

    int fd_max = S->GetMaxAcceptorFd();
    int num_servers = S->get_num_servers();
    int waitfor = num_servers;
    while (true) {  // always listen to messages from the acceptors
        temp_set = acceptor_set;
        int rv = select(fd_max + 1, &temp_set, NULL, NULL, NULL);

        if (rv == -1) { //error in select
            D(cout << "ERROR in select() for Scout" << endl;)
        } else if (rv == 0) {
            break;
        } else {
            for (int i = 0; i < num_servers; i++) {
                if (FD_ISSET(S->get_acceptor_fd(i), &temp_set)) { // we got one!!
                    char buf[kMaxDataSize];
                    if ((num_bytes = recv(S->get_acceptor_fd(i), buf, kMaxDataSize - 1, 0)) == -1) {
                        D(cout << "ERROR in receiving p1b from " << i << endl;)
                        // pthread_exit(NULL); //TODO: think about whether it should be exit or not
                    } else if (num_bytes == 0) {     //connection closed
                        D(cout << "Scout connection closed by " << i << endl;)
                    } else {
                        buf[num_bytes] = '\0';
                        std::vector<string> message = split(string(buf), kMessageDelim[0]);
                        for (const auto &msg : message) {
                            std::vector<string> token = split(string(msg), kInternalDelim[0]);
                            if (token[0] == kP1b) {
                                D(cout << "P1b" << "message received from " << i <<  endl;)

                                Ballot recvd_ballot = stringToBallot(token[2]);
                                unordered_set<Triple> r = stringToTripleSet(token[3]);
                                if (recvd_ballot == *ball)
                                {
                                    union_set(pvalues, r);
                                    waitfor--;
                                    if (waitfor < (num_servers / 2))
                                    {
                                        S->SendAdopted(recvd_ballot, pvalues);
                                        return NULL;
                                    }
                                } else {
                                    S->SendPreEmpted(recvd_ballot);
                                    return NULL;
                                }

                            } else {    //other messages
                                D(cout << "Unexpected message in Scout " << msg << endl;)
                            }
                        }
                    }
                }
            }
        }
    }
    return NULL;
}