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

extern void* AcceptConnectionsCommander(void* _S);

int Server::get_commander_fd(const int server_id) {
    return commander_fd_[server_id];
}

int Server::get_commander_listen_port(const int server_id) {
    return commander_listen_port_[server_id];
}

void Server::set_commander_fd(const int server_id, const int fd) {
    if (fd == -1 || commander_fd_[server_id] == -1) {
        commander_fd_[server_id] = fd;
    }
}

/**
 * creates accept thread for commander
 */
void Server::CommanderAcceptThread() {
    pthread_t accept_connections_thread;
    CreateThread(AcceptConnectionsCommander, (void*)this, accept_connections_thread);
}

void Server::SendP2a(const Triple & t)
{
    string msg = kP2a + kInternalDelim + to_string(get_pid());
    msg += kInternalDelim + tripleToString(t) + kMessageDelim;
    SendToServers(kP2a, msg);
}

void Server::SendDecision(const Triple & t)
{
    string msg = kDecision + kInternalDelim + to_string(t.s) + kInternalDelim;
    msg += proposalToString(t.p)  + kMessageDelim;
    SendToServers(kDecision, msg);
}


void* Commander(void* _rcv_thread_arg) {
    CommanderThreadArgument *rcv_thread_arg = (CommanderThreadArgument *)_rcv_thread_arg;
    Server *S = rcv_thread_arg->S;
    Triple * toSend = rcv_thread_arg->toSend;

    S->SendP2a(*toSend);

    int num_bytes;

    fd_set acceptor_set = S->GetAcceptorFdSet();
    fd_set temp_set;

    int fd_max = S->GetMaxAcceptorFd();
    int num_servers = S->get_num_servers();
    int waitfor = num_servers;
    while (true) {  // always listen to messages from the acceptors
        temp_set = acceptor_set;
        int rv = select(fd_max + 1, &temp_set, NULL, NULL, NULL);

        if (rv == -1) { //error in select
            D(cout << "ERROR in select() for Commander" << endl;)
        } else if (rv == 0) {
            break;
        } else {
            for (int i = 0; i < num_servers; i++) {
                if (FD_ISSET(S->get_acceptor_fd(i), &temp_set)) { // we got one!!
                    char buf[kMaxDataSize];
                    if ((num_bytes = recv(S->get_acceptor_fd(i), buf, kMaxDataSize - 1, 0)) == -1) {
                        D(cout << "ERROR in receiving p2b from " << i << endl;)
                        // pthread_exit(NULL); //TODO: think about whether it should be exit or not
                    } else if (num_bytes == 0) {     //connection closed
                        D(cout << "Commander connection closed by " << i << endl;)
                    } else {
                        buf[num_bytes] = '\0';
                        std::vector<string> message = split(string(buf), kMessageDelim[0]);
                        for (const auto &msg : message) {
                            std::vector<string> token = split(string(msg), kInternalDelim[0]);
                            if (token[0] == kP2b) {
                                D(cout << "P2b" << "message received from " << i <<  endl;)

                                Ballot recvd_ballot = stringToBallot(token[2]);
                                if (recvd_ballot == toSend->b)
                                {
                                    waitfor--;
                                    if (waitfor < (num_servers / 2))
                                    {
                                        S->SendDecision(*toSend);
                                        return NULL;
                                    }
                                } else {
                                    S->SendPreEmpted(recvd_ballot);
                                    return NULL;
                                }

                            } else {    //other messages
                                D(cout << "Unexpected message in commander " << msg << endl;)
                            }
                        }
                    }
                }
            }
        }
    }
    return NULL;
}