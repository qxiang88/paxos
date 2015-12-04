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

Commander::~Commander() {

}

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

void Commander::Unicast(const string &type, const string& msg)
{
    if (send(get_leader_fd(S->get_pid()), msg.c_str(), msg.size(), 0) == -1) {
        D(cout << "SC" << S->get_pid() << ": ERROR in sending " << type << endl;)
    }
    else {
        D(cout << "SC" << S->get_pid() << ": " << type << " message sent: " << msg << endl;)
    }
}

void Commander::SendToServers(const string& type, const string& msg)
{
    for (int i = 0; i < S->get_num_servers(); i++)
    {
        if (get_replica_fd(i) == -1)
            continue;

        if (send(get_replica_fd(i), msg.c_str(), msg.size(), 0) == -1) {
            D(cout << "SC" << S->get_pid()
              << ": ERROR in sending decision to replica S" << (i) << endl;)
            close(get_replica_fd(i));
            set_replica_fd(i, -1);
        }
        else {
            D(cout << "SC" << S->get_pid()
              << ": Decision sent to replica S" << (i) << ": " << msg << endl;)
        }
    }
}

void Commander::SendP2a(const Triple &t, const vector<int> &acceptor_peer_fd)
{
    int serv_fd;
    for (int i = 0; i < S->get_num_servers(); i++)
    {
        S->ContinueOrDie();

        serv_fd = get_acceptor_fd(i);

        if (serv_fd == -1) {
            continue;
        }

        string msg = kP2a + kInternalDelim + to_string(acceptor_peer_fd[i]);
        msg += kInternalDelim + tripleToString(t) + kMessageDelim;

        if (send(serv_fd, msg.c_str(), msg.size(), 0) == -1) {
            D(cout << "SC" << S->get_pid()
              << ": ERROR in sending P2A message to acceptor S" << i << endl;)
            close(serv_fd);
            set_acceptor_fd(i, -1);
        }
        else {
            D(cout << "SC" << S->get_pid()
              << ": P2A message sent to acceptor S" << i << ": " << msg << endl;)
        }

        S->DecrementMessageQuota();
    }
}

void Commander::SendDecision(const Triple &t)
{
    string msg = kDecision + kInternalDelim + to_string(t.s) + kInternalDelim;
    msg += proposalToString(t.p)  + kMessageDelim;
    SendToServers(kDecision, msg);
    Unicast(kDecision, msg);

}

void Commander::SendPreEmpted(const Ballot& b)
{
    string msg = kPreEmpted + kInternalDelim + ballotToString(b) + kMessageDelim;
    Unicast(kPreEmpted, msg);
}

/**
 * connects to all acceptors, and gets their fds
 * @param  acceptor_peer_fd [out] peer fds of commander-acceptor connection
 * @return                  num of alive acceptors with which this exchange was successfull
 */
int Commander::ConnectToAllAcceptors(std::vector<int> &acceptor_peer_fd) {
    int num_servers = S->get_num_servers();
    int num_alive_acceptors = 0;

    for (int i = 0; i < num_servers; ++i) {
        if (!ConnectToAcceptor(i)) {
            D(cout << "SC" << S->get_pid() << ": ERROR in connecting to acceptor S" << i << endl;)
        } else {
            int num_bytes;
            char buf[kMaxDataSize];
            num_bytes = recv(get_acceptor_fd(i), buf, kMaxDataSize - 1, 0);
            if (num_bytes == -1) {
                D(cout << "SC" << S->get_pid() <<
                  ": ERROR in receiving fd from acceptor S" << i << endl;)
                close(get_acceptor_fd(i));
                set_acceptor_fd(i, -1);
            } else if (num_bytes == 0) {
                D(cout << "SC" << S->get_pid() <<
                  ": Connection closed by acceptor S" << i << endl;)
                close(get_acceptor_fd(i));
                set_acceptor_fd(i, -1);
            } else {
                buf[num_bytes] = '\0';
                D(cout << "SC" << S->get_pid() <<
                  ": Received fd from acceptor S" << i << endl;)
                // acceptor side fd of this connection received.
                acceptor_peer_fd[i] = atoi(buf);
                num_alive_acceptors++;
            }
        }
    }
    return num_alive_acceptors;
}

void Commander::GetAcceptorFdSet(fd_set& acceptor_set, vector<int>& fds, int& fd_max)
{
    fd_max = INT_MIN;
    int fd_temp;
    FD_ZERO(&acceptor_set);
    fds.clear();
    for (int i = 0; i < S->get_num_servers(); i++) {
        fd_temp = get_acceptor_fd(i);
        if (fd_temp != -1)
        {
            char buf;
            int rv = recv(fd_temp, &buf, 1, MSG_DONTWAIT | MSG_PEEK);
            if (rv == 0) {
                close(fd_temp);
                set_acceptor_fd(i, -1);
            } else {
                FD_SET(fd_temp, &acceptor_set);
                fd_max = max(fd_max, fd_temp);
                fds.push_back(fd_temp);
            }
        }
    }
}

/**
 * close all connections with acceptors before exiting
 */
void Commander::CloseAllConnections() {
    int acceptor_fd;
    for (int i = 0; i < S->get_num_servers(); ++i) {
        acceptor_fd = get_acceptor_fd(i);
        if (acceptor_fd != -1) {
            close(acceptor_fd);
        }
    }
}

int Commander::GetAcceptorIdFromFd(int fd)
{
    for (int i = 0; i < S->get_num_servers(); i++)
    {
        if (get_acceptor_fd(i) == fd)
            return i;
    }

}

void* CommanderMode(void* _rcv_thread_arg) {
    signal(SIGPIPE, SIG_IGN);

    CommanderThreadArgument *rcv_thread_arg = (CommanderThreadArgument *)_rcv_thread_arg;
    Commander *C = rcv_thread_arg->C;
    Triple toSend = rcv_thread_arg->toSend;
    int num_servers = C->S->get_num_servers();

    std::vector<int> acceptor_peer_fd(num_servers, -1);
    int num_alive_acceptors = C->ConnectToAllAcceptors(acceptor_peer_fd);

    if ((float)num_alive_acceptors < (num_servers / 2.0)) {
        // won't send P2a to anyone since only a minority is alive
        D(cout << "SC" << C->S->get_pid()
          << ": Exiting because only minority of acceptors are alive" << endl;)
        usleep(kMinoritySleep);

        Triple no_op(toSend.b, toSend.s, Proposal(to_string(0), to_string(0), kNoop));
        C->SendDecision(no_op);
        return NULL;
    }

    C->SendP2a(toSend, acceptor_peer_fd);

    int num_bytes;

    fd_set acceptor_set;
    int fd_max = INT_MIN;
    vector<int> fds;
    int waitfor = num_servers;
    while (true) {  // always listen to messages from the acceptors
        C->GetAcceptorFdSet(acceptor_set, fds, fd_max);

        if (fd_max == INT_MIN) {
            D(cout << "SC" << C->S->get_pid()
              << ": Exiting because no more interesting acceptors left" << endl;)

            usleep(kMinoritySleep);

            Triple no_op(toSend.b, toSend.s, Proposal(to_string(0), to_string(0), kNoop));
            C->SendDecision(no_op);
            return NULL;
        }

        int rv = select(fd_max + 1, &acceptor_set, NULL, NULL, NULL);

        if (rv == -1) { //error in select
            D(cout << "SC" << C->S->get_pid() << ": ERROR in select()" << endl;)
        } else if (rv == 0) {
            D(cout << "SC" << C->S->get_pid() << ": ERROR: Unexpected timeout in select()" << endl;)
        } else {
            for (int i = 0; i < fds.size(); i++) {
                if (FD_ISSET(fds[i], &acceptor_set)) { // we got one!!
                    char buf[kMaxDataSize];
                    int serv_id = C->GetAcceptorIdFromFd(fds[i]);
                    if ((num_bytes = recv(fds[i], buf, kMaxDataSize - 1, 0)) == -1) {
                        D(cout << "SC" << C->S->get_pid() << ": ERROR in receiving p2b from acceptor S" << serv_id << endl;)
                        close(fds[i]);
                        C->set_acceptor_fd(serv_id, -1);
                    } else if (num_bytes == 0) {     //connection closed
                        D(cout << "SC" << C->S->get_pid() << ": Connection closed by acceptor S" << serv_id << endl;)
                        close(fds[i]);
                        C->set_acceptor_fd(serv_id, -1);
                    } else {
                        buf[num_bytes] = '\0';
                        std::vector<string> message = split(string(buf), kMessageDelim[0]);
                        for (const auto &msg : message) {
                            std::vector<string> token = split(string(msg), kInternalDelim[0]);

                            if (token[0] == kP2b) {
                                D(cout << "SC" << C->S->get_pid()
                                  << ": P2b message received from acceptor S" << serv_id << ": " << msg <<  endl;)

                                // close connection with this acceptor
                                // because no future communication with it will happen
                                close(fds[i]);
                                C->set_acceptor_fd(serv_id, -1);

                                Ballot recvd_ballot = stringToBallot(token[2]);
                                if (recvd_ballot == toSend.b)
                                {
                                    waitfor--;
                                    if ((float)waitfor < (num_servers / 2.0))
                                    {
                                        C->SendDecision(toSend);
                                        C->CloseAllConnections();
                                        return NULL;
                                    }
                                } else {
                                    C->SendPreEmpted(recvd_ballot);
                                    C->CloseAllConnections();
                                    return NULL;
                                }
                            } else {    //other messages
                                D(cout << "SC" << C->S->get_pid() << ": Unexpected message received: " << msg << endl;)
                            }
                        }
                    }
                }
            }
        }
    }
    return NULL;
}