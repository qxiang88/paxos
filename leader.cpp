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
using namespace std;

typedef pair<int, Proposal> SPtuple;

#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

int Server::get_leader_fd(const int server_id) {
    return leader_fd_[server_id];
}

int Server::get_leader_port(const int server_id) {
    return leader_port_[server_id];
}

void Server::set_leader_fd(const int server_id, const int fd) {
    if (fd == -1 || leader_fd_[server_id] == -1) {
        leader_fd_[server_id] = fd;
    }
}

/**
 * checks if given port corresponds to a leader's port
 * @param  port port number to be checked
 * @return      id of server whose leader port matches param port
 * @return      -1 if param port is not the leader port of any server
 */
int Server::IsLeaderPort(const int port) {
    if (leader_port_map_.find(port) != leader_port_map_.end()) {
        return leader_port_map_[port];
    } else {
        return -1;
    }
}

/**
 * thread entry function for leader
 * @param  _S pointer to server class object
 * @return    NULL
 */
void *LeaderEntry(void *_S) {
    Server *S = (Server*) _S;

    // does not need accept threads since it does not listen to connections from anyone

    // sleep for some time to make sure accept threads of commanders,scouts,replica are running
    usleep(kGeneralSleep);
    if (S->ConnectToCommanderL(S->get_primary_id())) {
        D(cout << "SL" << S->get_pid() << ": Connected to commander of S"
          << S->get_primary_id() << endl;)
    } else {
        D(cout << "SL" << S->get_pid() << ": ERROR in connecting to commander of S"
          << S->get_primary_id() << endl;)
        return NULL;
    }

    if (S->ConnectToScoutL(S->get_primary_id())) {
        D(cout << "SL" << S->get_pid() << ": Connected to scout of S"
          << S->get_primary_id() << endl;)
    } else {
        D(cout << "SL" << S->get_pid() << ": ERROR in connecting to scout of S"
          << S->get_primary_id() << endl;)
        return NULL;
    }

    if (S->ConnectToReplica(S->get_primary_id())) { // same as S->get_pid()
        D(cout << "SL" << S->get_pid() << ": Connected to replica of S"
          << S->get_primary_id() << endl;)
    } else {
        D(cout << "SL" << S->get_pid() << ": ERROR in connecting to replica of S"
          << S->get_primary_id() << endl;)
        return NULL;
    }
}

void Server::Leader()
{
    leader_active_ = false;
    proposals_.clear();
    ballot_num_.id = get_pid();
    ballot_num_.seq_num = 0;

    pthread_t scout_thread;
    CreateThread(Scout, (void*)this, scout_thread);

    char buf[kMaxDataSize];
    int num_bytes;
    int num_servers = get_num_servers();
    while (true) {  // always listen to messages from the acceptors
        if ((num_bytes = recv(get_replica_fd(get_primary_id()), buf, kMaxDataSize - 1, 0)) == -1) 
        {
          D(cout << "Leader ERROR in receiving" << endl;)
          // pthread_exit(NULL); //TODO: think about whether it should be exit or not
        } else if (num_bytes == 0) {     //connection closed
          D(cout << "Leader connection closed by self??"<< endl;)
        } else {
            buf[num_bytes] = '\0';
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) 
            {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kPropose) 
                {
                    D(cout << "Leader receives PROPOSE" << "message"<<  endl;)
                    if(proposals_.find(stoi(token[1]))==proposals_.end())
                    {
                      proposals_[stoi(token[1])]=stringToProposal(token[2]);
                      if(leader_active_)
                      {
                        pthread_t commander_thread;
                        CommanderThreadArgument* arg = new CommanderThreadArgument;
                        arg->S = this;
                        Triple tempt = Triple(ballot_num_, stoi(token[1]), proposals_[stoi(token[1])]);
                        arg->toSend = &tempt;
                        CreateThread(Commander, (void*)arg, commander_thread);
                      }
                    }
                } 
                else if(token[0] == kAdopted)
                {
                  unordered_set<Triple> pvalues = stringToTripleSet(token[2]);
                  proposals_ = pairxor(proposals_, pmax(pvalues));

                  pthread_t commander_thread[proposals_.size()];
                  int i=0;
                  for(auto it = proposals_.begin(); it!=proposals_.end(); it++)
                  {
                    CommanderThreadArgument* arg = new CommanderThreadArgument;
                    arg->S = this;
                    Triple tempt = Triple(ballot_num_, stoi(token[1]), proposals_[stoi(token[1])]);
                    arg->toSend = &tempt;
                    CreateThread(Commander, (void*)arg, commander_thread[i]);
                    i++;
                  }
                  leader_active_ = true;
                }

                else if(token[0]==kPreEmpted)
                {
                  Ballot recvd_b = stringToBallot(token[1]);
                  if(recvd_b>ballot_num_)
                  {
                    leader_active_ = false;
                    IncrementBallotNum();
                  }
                }

                else {    //other messages
                    D(cout<<"Unexpected message in Leader "<<msg<<endl;)
                }
            }
        }
      }
    
}
//actually leader thread for each replica required. but here only one replica sends leader anything
void* LeaderThread(void* _rcv_thread_arg) {
    LeaderThreadArgument *rcv_thread_arg = (LeaderThreadArgument *)_rcv_thread_arg;
    Server *S = rcv_thread_arg->S;
    S->Leader();
    return NULL;
}