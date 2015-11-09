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

#define DEBUG

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
    // replica_fd_.resize(num_servers, -1);
    acceptor_fd_.resize(num_servers, -1);
}

int Scout::get_leader_fd(const int server_id) {
    return leader_fd_[server_id];
}

// int Scout::get_replica_fd(const int server_id) {
//     return replica_fd_[server_id];
// }

int Scout::get_acceptor_fd(const int server_id) {
    return acceptor_fd_[server_id];
}

void Scout::set_leader_fd(const int server_id, const int fd) {
    leader_fd_[server_id] = fd;
}

// void Scout::set_replica_fd(const int server_id, const int fd) {
//     replica_fd_[server_id] = fd;
// }

void Scout::set_acceptor_fd(const int server_id, const int fd) {
    acceptor_fd_[server_id] = fd;
}

int Scout::SendToServers(const string& type, const string& msg)
{
    int num_send = 0;
    for (int i = 0; i < S->get_num_servers(); i++)
    {
        S->ContinueOrDie();

        int serv_id = get_acceptor_fd(i);
        if (serv_id != -1)
        {
            if (send(serv_id, msg.c_str(), msg.size(), 0) == -1) {
                D(cout << "SS" << S->get_pid() << ": ERROR: sending to acceptor S" << (serv_id) << endl;)
                close(get_acceptor_fd(i));
                set_acceptor_fd(i, -1);
            }
            else {
                D(cout << "SS" << S->get_pid() << ": Message sent to acceptor S" << i << ": " << msg << endl;)
                num_send++;
            }
        }

        S->DecrementMessageQuota();
    }
    return num_send;
}

void Scout::GetAcceptorFdSet(fd_set& acceptor_set, vector<int>& fds, int& fd_max)
{
    char buf;
    fd_max = INT_MIN;
    int fd_temp;
    FD_ZERO(&acceptor_set);
    fds.clear();
    for (int i = 0; i < S->get_num_servers(); i++) {
        fd_temp = get_acceptor_fd(i);
        if (fd_temp == -1) continue;

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

void Scout::Unicast(const string &type, const string& msg)
{
    int serv_fd = get_leader_fd(S->get_pid());
    if (send(serv_fd, msg.c_str(), msg.size(), 0) == -1) {
        D(cout << "SS" << S->get_pid() << ": ERROR in sending " << type << endl;)
    }
    else {
        D(cout << "SS" << S->get_pid() << ": " << type << " message sent: " << msg << endl;)
    }
}

int Scout::SendP1a(const Ballot &b)
{
    string msg = kP1a + kInternalDelim + to_string(S->get_pid());
    msg += kInternalDelim + ballotToString(b) + kMessageDelim;
    return SendToServers(kP1a, msg);
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

int Scout::GetServerIdFromFd(int fd)
{
    for (int i = 0; i < S->get_num_servers(); i++)
    {
        if (get_acceptor_fd(i) == fd)
        {
            return i;
        }
    }
}

void Scout::CloseAndUnSetAcceptor(int id)
{
    close(get_acceptor_fd(id));
    set_acceptor_fd(id, -1);

}

/**
 * counts and returns the number of acceptors currently alive
 * @return number of alive acceptors
 */
int Scout::CountAcceptorsAlive() {
    char buf;
    int rv;
    int count = 0;
    for (int i = 0; i < S->get_num_servers(); ++i)
    {
        int fd_temp = get_acceptor_fd(i);
        if (fd_temp == -1)
            continue;

        rv = recv(fd_temp, &buf, 1, MSG_DONTWAIT | MSG_PEEK);
        if (rv == 0) {
            close(fd_temp);
            set_acceptor_fd(i, -1);
        } else {
            count++;
        }
    }
    return count;
}

void* ScoutMode(void* _rcv_thread_arg) {
    signal(SIGPIPE, SIG_IGN);

    ScoutThreadArgument *rcv_thread_arg = (ScoutThreadArgument *)_rcv_thread_arg;
    Scout *SC = rcv_thread_arg->SC;
    Ballot ball = rcv_thread_arg->ball;
    time_t sleep_time = rcv_thread_arg->sleep_time;

    usleep(sleep_time);
    int num_servers = SC->S->get_num_servers();
    int num_send;

    int num_alive_acceptors = SC->CountAcceptorsAlive();
    if ((float)num_alive_acceptors < (num_servers / 2.0)) {
        num_send = 0;   // won't send to anyone since only a minority is alive
    } else {
        num_send = SC->SendP1a(ball);   // number of servers to which p1a successfully sent
    }

    int num_bytes;
    unordered_set<Triple> pvalues;

    int waitfor = num_servers;
    vector<int> fds;
    while (num_send) {  // always listen to messages from the acceptors
        fd_set acceptor_set;
        int fd_max;
        SC->GetAcceptorFdSet(acceptor_set, fds, fd_max);
        if (fd_max == INT_MIN) {
            usleep(kBusyWaitSleep);
            continue;
        }
        struct timeval timeout = kSelectTimeoutTimeval;
        int rv = select(fd_max + 1, &acceptor_set, NULL, NULL, &timeout);
        if (rv == -1) { //error in select
            D(cout << "SS" << SC->S->get_pid() << ": ERROR in select()" << endl;)
        } else if (rv == 0) {
            // D(cout << "SS" << SC->S->get_pid() << ": ERROR Unexpected select timeout" << endl;)
        } else {
            for (int i = 0; i < fds.size(); i++) {
                if (FD_ISSET(fds[i], &acceptor_set)) { // we got one!!
                    char buf[kMaxDataSize];
                    int serv_id = SC->GetServerIdFromFd(fds[i]);
                    if ((num_bytes = recv(fds[i], buf, kMaxDataSize - 1, 0)) == -1) {
                        SC->CloseAndUnSetAcceptor(serv_id);
                        num_send--;
                        D(cout << "SS" << SC->S->get_pid()
                          << ": ERROR in receiving p1b from acceptor S" << serv_id << endl;)
                    } else if (num_bytes == 0) {     //connection closed
                        SC->CloseAndUnSetAcceptor(serv_id);
                        num_send--;
                        D(cout << "SS" << SC->S->get_pid()
                          << ": ERROR Connection closed connection closed by acceptor S" << serv_id << endl;)
                    } else {
                        buf[num_bytes] = '\0';
                        std::vector<string> message = split(string(buf), kMessageDelim[0]);
                        for (const auto &msg : message) {
                            std::vector<string> token = split(string(msg), kInternalDelim[0]);
                            if (token[0] == kP1b) {
                                D(cout << "SS" << SC->S->get_pid()
                                  << ": received P1B from acceptor S" << serv_id << ": " << msg << endl;)

                                num_send--;
                                Ballot recvd_ballot = stringToBallot(token[2]);
                                unordered_set<Triple> r;
                                if (token.size() == 4)
                                {
                                    r = stringToTripleSet(token[3]);
                                }

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

    // if scout reaches here, num_send=0 and only minority of acceptors are alive.
    SC->SendPreEmpted(ball);
    return NULL;
}