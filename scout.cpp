#include "scout.h"
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

extern void* AcceptConnectionsScout(void* _S);

Scout::Scout(Server* _S) {
    S = _S;

    int num_servers = S->get_num_servers();

    leader_fd_.resize(num_servers, -1);
    replica_fd_.resize(num_servers, -1);
    acceptor_fd_.resize(num_servers, -1);
}

int Scout::get_leader_fd(const int server_id) {
    return leader_fd_[server_id];
}

int Scout::get_replica_fd(const int server_id) {
    return replica_fd_[server_id];
}

int Scout::get_acceptor_fd(const int server_id) {
    return acceptor_fd_[server_id];
}

void Scout::set_leader_fd(const int server_id, const int fd) {
    leader_fd_[server_id] = fd;
}

void Scout::set_replica_fd(const int server_id, const int fd) {
    replica_fd_[server_id] = fd;
}

void Scout::set_acceptor_fd(const int server_id, const int fd) {
    acceptor_fd_[server_id] = fd;
}

void Scout::SendToServers(const string& type, const string& msg)
{
    for (int i = 0; i < S->get_num_servers(); i++)
    {
        if (send(get_acceptor_fd(i), msg.c_str(), msg.size(), 0) == -1) {
            D(cout << "SS" << S->get_pid() << ": ERROR: sending to acceptor S" << (i) << endl;)
        }
        else {
            D(cout << "SS" << S->get_pid() << ": Message sent to acceptor S" << i << ": " << msg << endl;)
        }
    }
}

void Scout::GetAcceptorFdSet(fd_set& acceptor_set, int& fd_max)
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

void Scout::Unicast(const string &type, const string& msg)
{
    if (send(get_leader_fd(S->get_pid()), msg.c_str(), msg.size(), 0) == -1) {
        D(cout << "SS" << S->get_pid() << ": ERROR in sending " << type << endl;)
    }
    else {
        D(cout << "SS" << S->get_pid() << ": " << type << " message sent: " << msg << endl;)
    }
}

void Scout::SendP1a(const Ballot &b)
{
    string msg = kP1a + kInternalDelim + to_string(S->get_pid());
    msg += kInternalDelim + ballotToString(b) + kMessageDelim;
    SendToServers(kP1a, msg);
}

void Scout::SendAdopted(const Ballot& recvd_ballot, unordered_set<Triple> pvalues) {
    string msg = kAdopted + kInternalDelim + ballotToString(recvd_ballot) + kInternalDelim;
    msg += tripleSetToString(pvalues) + kMessageDelim;
    Unicast(kAdopted, msg);
}

void Scout::SendPreEmpted(const Ballot& b)
{
    string msg = kPreEmpted + kInternalDelim + ballotToString(b) + kMessageDelim;
    Unicast(kPreEmpted, msg);
}

void* ScoutMode(void* _rcv_thread_arg) {
    ScoutThreadArgument *rcv_thread_arg = (ScoutThreadArgument *)_rcv_thread_arg;
    Scout *SC = rcv_thread_arg->SC;
    Ballot ball = rcv_thread_arg->ball;

    SC->SendP1a(ball);

    int num_bytes;
    unordered_set<Triple> pvalues;
    // fd_set temp_set;

    int num_servers = SC->S->get_num_servers();
    int waitfor = num_servers;
    while (true) {  // always listen to messages from the acceptors
        fd_set acceptor_set;
        int fd_max;
        SC->GetAcceptorFdSet(acceptor_set, fd_max);

        int rv = select(fd_max + 1, &acceptor_set, NULL, NULL, NULL);
        if (rv == -1) { //error in select
            D(cout << "SS" << SC->S->get_pid() << ": ERROR in select()" << endl;)
        } else if (rv == 0) {
            D(cout << "SS" << SC->S->get_pid() << ": ERROR Unexpected select timeout" << endl;)
        } else {
            for (int i = 0; i < num_servers; i++) {
                if (FD_ISSET(SC->get_acceptor_fd(i), &acceptor_set)) { // we got one!!
                    char buf[kMaxDataSize];
                    if ((num_bytes = recv(SC->get_acceptor_fd(i), buf, kMaxDataSize - 1, 0)) == -1) {
                        D(cout << "SS" << SC->S->get_pid()
                          << ": ERROR in receiving p1b from acceptor S" << i << endl;)
                        close(SC->get_acceptor_fd(i));
                        SC->set_acceptor_fd(i, -1);
                    } else if (num_bytes == 0) {     //connection closed
                        D(cout << "SS" << SC->S->get_pid()
                          << ": ERROR Connection closed connection closed by acceptor S" << i << endl;)
                        close(SC->get_acceptor_fd(i));
                        SC->set_acceptor_fd(i, -1);
                    } else {
                        buf[num_bytes] = '\0';
                        std::vector<string> message = split(string(buf), kMessageDelim[0]);
                        for (const auto &msg : message) {
                            std::vector<string> token = split(string(msg), kInternalDelim[0]);
                            if (token[0] == kP1b) {
                                D(cout << "SS" << SC->S->get_pid() << ": received P1B from acceptor S" << i <<  endl;)
                                
                                Ballot recvd_ballot = stringToBallot(token[2]);
                                unordered_set<Triple> r;

                                if(token.size()==4)
                                    r = stringToTripleSet(token[3]);
                                
                                if (recvd_ballot == ball)
                                {  
                                    union_set(pvalues, r);
                                    waitfor--;
                                    if ((float)waitfor < (num_servers / 2.0))
                                    {
                                        SC->SendAdopted(recvd_ballot, pvalues);
                                        return NULL;
                                    }
                                } else {
                                    SC->SendPreEmpted(recvd_ballot);
                                    return NULL;
                                }
                            } else {    //other messages
                                D(cout << "SS" << SC->S->get_pid() << ": ERROR Unexpected message received: " << msg << endl;)
                            }

                        }
                    }
                }
            }
        }
    }
    D(cout<<"Scout exiting"<<endl;)
    return NULL;
}