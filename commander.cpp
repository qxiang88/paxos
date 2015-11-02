#include "commander.h"
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

// static member definitions
std::vector<int> Commander::leader_fd_;
std::vector<int> Commander::replica_fd_;

Commander::Commander(Server* _S, const int num_servers) {
    S = _S;
    leader_fd_.resize(num_servers, -1);
    replica_fd_.resize(num_servers, -1);
}

Commander::Commander(Server* _S) {
    S = _S;
    acceptor_fd_.resize(S->get_num_servers(), -1);
}

int Commander::get_leader_fd(const int server_id) {
    return leader_fd_[server_id];
}

int Commander::get_replica_fd(const int server_id) {
    return replica_fd_[server_id];
}

int Commander::get_acceptor_fd(const int server_id) {
    return acceptor_fd_[server_id];
}

void Commander::set_leader_fd(const int server_id, const int fd) {
    leader_fd_[server_id] = fd;
}

void Commander::set_replica_fd(const int server_id, const int fd) {
    replica_fd_[server_id] = fd;
}

void Commander::set_acceptor_fd(const int server_id, const int fd) {
    acceptor_fd_[server_id] = fd;
}

void Commander::SendToServers(const string& type, const string& msg)
{
    int serv_fd;
    for (int i = 0; i < S->get_num_servers(); i++)
    {
        if (type == kDecision)
            serv_fd = get_replica_fd(i);
        else if (type == kP1a) //p2a sent in commander
            serv_fd = get_acceptor_fd(i);

        if (send(serv_fd, msg.c_str(), msg.size(), 0) == -1) {
            D(cout << "SC" << S->get_pid() << ": ERROR in sending to S" << (i) << endl;)
        }
        else {
            D(cout << "SC" << S->get_pid() << ": Message sent to S" << (i) << endl;)
        }
    }
}

void Commander::SendP2a(const Triple & t, vector<int> acceptor_peer_fd)
{
    int serv_fd;
    for (int i = 0; i < S->get_num_servers(); i++)
    {
        serv_fd = get_acceptor_fd(i);
        string msg = kP2a + kInternalDelim + to_string(acceptor_peer_fd[i]);
        msg += kInternalDelim + tripleToString(t) + kMessageDelim;

        if (send(serv_fd, msg.c_str(), msg.size(), 0) == -1) {
            D(cout << "SC" << S->get_pid() << ": ERROR in sending Phase2A message to acceptor S" << i << endl;)
        }
        else {
            D(cout << "SC" << S->get_pid() << ": Phase2A message sent to acceptor S" << i << endl;)
        }
    }
}

void Commander::SendDecision(const Triple & t)
{
    string msg = kDecision + kInternalDelim + to_string(t.s) + kInternalDelim;
    msg += proposalToString(t.p)  + kMessageDelim;
    SendToServers(kDecision, msg);
}

void Commander::SendPreEmpted(const Ballot& b)
{
    string msg = kPreEmpted + kInternalDelim + ballotToString(b) + kMessageDelim;
    S->Unicast(kPreEmpted, msg);
}

void Commander::ConnectToAllAcceptors(std::vector<int> &acceptor_peer_fd) {
    for (int i = 0; i < S->get_num_servers(); ++i) {
        if (!ConnectToAcceptor(i)) {
            D(cout << "SC" << S->get_pid() << ": ERROR in connecting to acceptor S " << i << endl;)
        } else {
            int num_bytes;
            char buf[kMaxDataSize];
            num_bytes = recv(get_acceptor_fd(i), buf, kMaxDataSize - 1, 0);
            if (num_bytes == -1) {
                D(cout << "SC" << S->get_pid() <<
                  ": ERROR in receiving fd from acceptor S " << i << endl;)
                //TODO: should I close the acceptor_fd?
                close(get_acceptor_fd(i));
                set_acceptor_fd(i, -1);
            } else if (num_bytes == 0) {
                D(cout << "SC" << S->get_pid() <<
                  ": Connection closed by acceptor S " << i << endl;)
                //TODO: should I close the acceptor_fd?
                close(get_acceptor_fd(i));
                set_acceptor_fd(i, -1);
            } else {
                buf[num_bytes] = '\0';
                // acceptor side fd of this connection received.
                acceptor_peer_fd[i] = atoi(buf);
            }
        }
    }
}

void Commander::GetAcceptorFdSet(fd_set& acceptor_set, int& fd_max)
{
    fd_max = INT_MIN;
    int fd_temp;
    FD_ZERO(&acceptor_set);
    for (int i = 0; i < S->get_num_servers(); i++) {
        fd_temp = get_acceptor_fd(i);
        if (fd_temp != -1)
        {
            FD_SET(fd_temp, &acceptor_set);
            fd_max = max(fd_max, fd_temp);
        }
    }
}

void* CommanderMode(void* _rcv_thread_arg) {
    CommanderThreadArgument *rcv_thread_arg = (CommanderThreadArgument *)_rcv_thread_arg;
    Commander *C = rcv_thread_arg->C;
    Triple * toSend = rcv_thread_arg->toSend;

    std::vector<int> acceptor_peer_fd;
    C->ConnectToAllAcceptors(acceptor_peer_fd);
    C->SendP2a(*toSend, acceptor_peer_fd);

    int num_bytes;

    fd_set acceptor_set;
    int fd_max;

    int num_servers = C->S->get_num_servers();
    int waitfor = num_servers;
    while (true) {  // always listen to messages from the acceptors
        C->GetAcceptorFdSet(acceptor_set, fd_max);
        int rv = select(fd_max + 1, &acceptor_set, NULL, NULL, NULL);

        if (rv == -1) { //error in select
            D(cout << "SC" << C->S->get_pid() << "ERROR in select()" << endl;)
        } else if (rv == 0) {
            D(cout << "SC" << C->S->get_pid() << "ERROR: Unexpected timeout in select()" << endl;)
        } else {
            for (int i = 0; i < num_servers; i++) {
                if (FD_ISSET(C->get_acceptor_fd(i), &acceptor_set)) { // we got one!!
                    char buf[kMaxDataSize];
                    if ((num_bytes = recv(C->get_acceptor_fd(i), buf, kMaxDataSize - 1, 0)) == -1) {
                        D(cout << "SC" << C->S->get_pid() << "ERROR in receiving p2b from acceptor S" << i << endl;)
                        // pthread_exit(NULL); //TODO: think about whether it should be exit or not
                    } else if (num_bytes == 0) {     //connection closed
                        D(cout << "SC" << C->S->get_pid() << "Connection closed by acceptor S" << i << endl;)
                    } else {
                        buf[num_bytes] = '\0';
                        std::vector<string> message = split(string(buf), kMessageDelim[0]);
                        for (const auto &msg : message) {
                            std::vector<string> token = split(string(msg), kInternalDelim[0]);
                            if (token[0] == kP2b) {
                                D(cout << "SC" << C->S->get_pid() << "P2b message received from acceptor S" << i <<  endl;)

                                Ballot recvd_ballot = stringToBallot(token[2]);
                                if (recvd_ballot == toSend->b)
                                {
                                    waitfor--;
                                    if (waitfor < (num_servers / 2))
                                    {
                                        C->SendDecision(*toSend);
                                        return NULL;
                                    }
                                } else {
                                    C->SendPreEmpted(recvd_ballot);
                                    return NULL;
                                }
                            } else {    //other messages
                                D(cout << "SC" << C->S->get_pid() << "Unexpected message received: " << msg << endl;)
                            }
                        }
                    }
                }
            }
        }
    }
    return NULL;
}