#include "leader.h"
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

extern void* CommanderMode(void* _rcv_thread_arg);

Leader::Leader(Server* _S) {
    S = _S;

    set_ballot_num(Ballot(S->get_pid(), 0));
    set_leader_active(false);

    int num_servers = S->get_num_servers();

    commander_fd_.resize(num_servers, -1);
    scout_fd_.resize(num_servers, -1);
    replica_fd_.resize(num_servers, -1);
}

int Leader::get_commander_fd(const int server_id) {
    return commander_fd_[server_id];
}

int Leader::get_scout_fd(const int server_id) {
    return scout_fd_[server_id];
}

int Leader::get_replica_fd(const int server_id) {
    return replica_fd_[server_id];
}

Ballot Leader::get_ballot_num() {
    return ballot_num_;
}

bool Leader::get_leader_active() {
    return leader_active_;
}

void Leader::set_commander_fd(const int server_id, const int fd) {
    commander_fd_[server_id] = fd;
}

void Leader::set_scout_fd(const int server_id, const int fd) {
    scout_fd_[server_id] = fd;
}

void Leader::set_replica_fd(const int server_id, const int fd) {
    replica_fd_[server_id] = fd;
}

void Leader::set_ballot_num(const Ballot &ballot_num) {
    ballot_num_.id = ballot_num.id;
    ballot_num_.seq_num = ballot_num.seq_num;
}

void Leader::set_leader_active(const bool b) {
    leader_active_ = b;
}

/**
 * increments the value of ballot_num_
 */
void Leader::IncrementBallotNum() {
    Ballot b = get_ballot_num();
    b.seq_num++;
    set_ballot_num(b);
}

void Leader::GetFdSet(fd_set& recv_from_set, int& fd_max, vector<int>& fds)
{
    fd_max = INT_MIN;
    int fd_temp;
    FD_ZERO(&recv_from_set);
    fds.clear();
    
    fd_temp = get_replica_fd(S->get_pid());
    FD_SET(fd_temp, &recv_from_set);
    fd_max = max(fd_max, fd_temp);
    fds.push_back(fd_temp);

    fd_temp = get_scout_fd(S->get_pid());
    FD_SET(fd_temp, &recv_from_set);
    fd_max = max(fd_max, fd_temp);
    fds.push_back(fd_temp);

    fd_temp = get_commander_fd(S->get_pid());
    FD_SET(fd_temp, &recv_from_set);
    fd_max = max(fd_max, fd_temp);
    fds.push_back(fd_temp);
}
/**
 * thread entry function for leader
 * @param  _S pointer to server class object
 * @return    NULL
 */
void* LeaderEntry(void *_S) {
    Leader L((Server*)_S);

    // does not need accept threads since it does not listen to connections from anyone

    // sleep for some time to make sure accept threads of commanders,scouts,replica are running
    usleep(kGeneralSleep);
    usleep(kGeneralSleep);
    int primary_id = L.S->get_primary_id();
    if (L.ConnectToCommander(primary_id)) {
        D(cout << "SL" << L.S->get_pid() << ": Connected to commander of S"
          << primary_id << endl;)
    } else {
        D(cout << "SL" << L.S->get_pid() << ": ERROR in connecting to commander of S"
          << primary_id << endl;)
        return NULL;
    }

    if (L.ConnectToScout(primary_id)) {
        D(cout << "SL" << L.S->get_pid() << ": Connected to scout of S"
          << primary_id << endl;)
    } else {
        D(cout << "SL" << L.S->get_pid() << ": ERROR in connecting to scout of S"
          << primary_id << endl;)
        return NULL;
    }

    if (L.ConnectToReplica(primary_id)) { // same as R.S->get_pid()
        D(cout << "SL" << L.S->get_pid() << ": Connected to replica of S"
          << primary_id << endl;)
    } else {
        D(cout << "SL" << L.S->get_pid() << ": ERROR in connecting to replica of S"
          << primary_id << endl;)
        return NULL;
    }

    L.LeaderMode();
    return NULL;
}

/**
 * function for performing leader related job
 */
void Leader::LeaderMode()
{
    // scout
    pthread_t scout_thread;
    ScoutThreadArgument* arg = new ScoutThreadArgument;
    arg->SC = S->get_scout_object();
    arg->ball = get_ballot_num();
    CreateThread(ScoutMode, (void*)arg, scout_thread);

    
    int num_servers = S->get_num_servers();

    while (true) {
        fd_set recv_from_set;
        int fd_max;
        vector<int> fds;
        GetFdSet(recv_from_set, fd_max, fds);

        int rv = select(fd_max + 1, &recv_from_set, NULL, NULL, NULL);
        
        if (rv == -1) { //error in select
            D(cout << "SL" << S->get_pid() << ": ERROR in select()" << endl;)
        } else if (rv == 0) {
            D(cout << "SL" << S->get_pid() << ": ERROR Unexpected select timeout" << endl;)
        } else {
            for(int i=0;i<fds.size();i++)
            {
                if (FD_ISSET(fds[i], &recv_from_set)) { // we got one!!
                    char buf[kMaxDataSize];
                    int num_bytes;
                    if ((num_bytes = recv(fds[i], buf, kMaxDataSize - 1, 0)) == -1)
                    {
                        D(cout << "SL" << S->get_pid() << ": ERROR in receving" << endl;)
                        // pthread_exit(NULL); //TODO: think about whether it should be exit or not
                    } else if (num_bytes == 0) {     //connection closed
                        D(cout << "SL" << S->get_pid() << ": ERROR Connection closed" << endl;)
                    } else {
                        buf[num_bytes] = '\0';
                        std::vector<string> message = split(string(buf), kMessageDelim[0]);
                        for (const auto &msg : message)
                        {
                            // D(cout << "SL" << S->get_pid() << ": Message received: " << msg <<  endl;)
                            std::vector<string> token = split(string(msg), kInternalDelim[0]);
                            if (token[0] == kPropose)
                            {
                                D(cout << "SL" << S->get_pid() << ": Propose message received: " << msg <<  endl;)
                                if (proposals_.find(stoi(token[1])) == proposals_.end())
                                {
                                    proposals_[stoi(token[1])] = stringToProposal(token[2]);
                                    if (get_leader_active())
                                    {
                                        // commander
                                        pthread_t commander_thread;
                                        Commander C(S);
                                        CommanderThreadArgument* arg = new CommanderThreadArgument;
                                        arg->C = &C;
                                        Triple tempt = Triple(get_ballot_num(), stoi(token[1]), proposals_[stoi(token[1])]);
                                        arg->toSend = &tempt;
                                        CreateThread(CommanderMode, (void*)arg, commander_thread);
                                    }
                                }
                            }
                            else if (token[0] == kAdopted)
                            {
                                D(cout << "SL" << S->get_pid() << ": Adopted message received: " << msg <<  endl;)

                                unordered_set<Triple> pvalues;
                                if(token.size()==3)
                                    {
                                        pvalues = stringToTripleSet(token[2]);
                                        proposals_ = pairxor(proposals_, pmax(pvalues));
                                    }

                                pthread_t commander_thread[proposals_.size()];
                                int i = 0;
                                for (auto it = proposals_.begin(); it != proposals_.end(); it++)
                                {
                                    // commander
                                    Commander C(S);
                                    CommanderThreadArgument* arg = new CommanderThreadArgument;
                                    arg->C = &C;
                                    Triple tempt = Triple(get_ballot_num(), stoi(token[1]), proposals_[stoi(token[1])]);
                                    arg->toSend = &tempt;
                                    CreateThread(CommanderMode, (void*)arg, commander_thread[i]);
                                    i++;
                                }
                                set_leader_active(true);
                            }

                            else if (token[0] == kPreEmpted)
                            {
                                D(cout << "SL" << S->get_pid() << ": PreEmpted message received: " << buf <<  endl;)
                                Ballot recvd_b = stringToBallot(token[1]);
                                if (recvd_b > get_ballot_num())
                                {
                                    set_leader_active(false);
                                    IncrementBallotNum();

                                    // scout
                                    // pthread_t scout_thread;
                                    ScoutThreadArgument* arg = new ScoutThreadArgument;
                                    arg->SC = S->get_scout_object();
                                    arg->ball = get_ballot_num();
                                    CreateThread(ScoutMode, (void*)arg, scout_thread);
                                }
                            }
                            else {    //other messages
                                D(cout << "SL" << S->get_pid() << ": ERROR: Unexpected message received: " << msg << endl;)
                            }
                        }
                    }
                }
            }
        }
    }

    D(cout<<"Leader exiting"<<endl;)
}