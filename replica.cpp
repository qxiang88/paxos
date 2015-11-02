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

extern void* AcceptConnectionsReplica(void* _S);

int Server::get_replica_fd(const int server_id) {
    return replica_fd_[server_id];
}

int Server::get_replica_listen_port(const int server_id) {
    return replica_listen_port_[server_id];
}

int Server::get_replica_port(const int server_id) {
    return replica_port_[server_id];
}

void Server::set_replica_fd(const int server_id, const int fd) {
    // if (fd == -1 || replica_fd_[server_id] == -1) {
        replica_fd_[server_id] = fd;
    // }
}

/**
 * checks if given port corresponds to a replica's port
 * @param  port port number to be checked
 * @return      id of server whose replica port matches param port
 * @return      -1 if param port is not the replica port of any server
 */
int Server::IsReplicaPort(const int port) {
    if (replica_port_map_.find(port) != replica_port_map_.end()) {
        return replica_port_map_[port];
    } else {
        return -1;
    }
}

/**
 * creates threads for receiving messages from clients
 */
void Server::CreateReceiveThreadsForClients() {
    std::vector<pthread_t> receive_from_client_thread(num_clients_);

    ReceiveThreadArgument **rcv_thread_arg = new ReceiveThreadArgument*[num_clients_];
    for (int i = 0; i < num_clients_; i++) {
        rcv_thread_arg[i] = new ReceiveThreadArgument;
        rcv_thread_arg[i]->S = this;
        rcv_thread_arg[i]->client_id = i;
        CreateThread(ReceiveMessagesFromClient,
                     (void *)rcv_thread_arg[i],
                     receive_from_client_thread[i]);
    }
}

void Server::Propose(const Proposal &p) {
    for (auto it = decisions_.begin(); it != decisions_.end(); ++it ){
        if (it->second == p)
            return;
    }

    int min_slot;
    if(proposals_.rbegin()==proposals_.rend())
        min_slot = 0;
    else
        min_slot = proposals_.rbegin()->first;

    while(decisions_.find(min_slot)!=decisions_.end())
        min_slot++;
    
    proposals_[min_slot] = p;
    SendProposal(min_slot, p);
}

void Server::SendProposal(const int& s, const Proposal& p)
{
    string msg = kPropose + kInternalDelim;
    msg += to_string(s) + kInternalDelim;
    msg += proposalToString(p) + kMessageDelim;
    Unicast(kPropose, msg);
}
void Server::Perform(const int& slot, const Proposal& p)
{
    for(auto it=decisions_.begin(); it!=decisions_.end(); it++)
    {
        if(it->second == p && it->first < slot_num_)
        {   //think why not increase in loop
            IncrementSlotNum();
            return;
        }
    }

    IncrementSlotNum();
    SendResponseToClient(slot, p);
}

void Server::SendResponseToClient(const int& s, const Proposal& p)
{
    string msg = kResponse + kInternalDelim;
    msg += to_string(s) + kInternalDelim;
    msg += proposalToString(p) + kMessageDelim;
    
    if (send(get_client_chat_fd(stoi(p.client_id)), msg.c_str(), msg.size(), 0) == -1) {
        D(cout << ": ERROR: sending response to client"<< endl;)
    }
    else {
        D(cout << ": Response Msg sent"<<endl;)
    }
}

void Server::Replica()
{
    char buf[kMaxDataSize];
    int num_bytes;

    fd_set fromset, temp_set;
    vector<int> fds;
    
    while (true) {  // always listen to messages from the acceptors
        int fd_max = INT_MIN, fd_temp;
        FD_ZERO(&fromset);
        for(int i=0; i<get_num_clients(); i++)
        {   
            fd_temp = get_client_chat_fd(i);
            fd_max = max(fd_max, fd_temp);
            fds.push_back(fd_temp);
            FD_SET(fd_temp, &fromset);    
        }
        fd_temp = get_leader_fd(get_primary_id());
        fd_max = max(fd_max, fd_temp);
        fds.push_back(fd_temp);
        FD_SET(fd_temp, &fromset);


        int rv = select(fd_max + 1, &fromset, NULL, NULL, NULL);

        if (rv == -1) { //error in select
            D(cout << "ERROR in select() for Replica" << endl;)
        } else if (rv == 0) {
            D(cout<<"Unexpected select timeout in Replica"<<endl;)
           continue;
        } else {
            for (int i = 0; i < fds.size(); i++) {
                if (FD_ISSET(fds[i], &fromset)) { // we got one!!
                    char buf[kMaxDataSize];
                    if ((num_bytes = recv(fds[i], buf, kMaxDataSize - 1, 0)) == -1) {
                        D(cout << "ERROR in receiving from leader or clients"<< endl;)
                        // pthread_exit(NULL); //TODO: think about whether it should be exit or not
                    } else if (num_bytes == 0) {     //connection closed
                        D(cout << "Replica connection for "<<get_pid()<<" closed by leader or client"<< endl;)
                    } else {
                        buf[num_bytes] = '\0';
                        std::vector<string> message = split(string(buf), kMessageDelim[0]);
                        for (const auto &msg : message) {
                            std::vector<string> token = split(string(msg), kInternalDelim[0]);
                            if (token[0] == kChat) 
                            {
                                D(cout << get_pid()<< " received chat from client "<<  endl;)
                                Proposal p = stringToProposal(token[1]);
                                Propose(p);
                            }
                            else if(token[0] == kDecision)
                            {
                                D(cout << get_pid()<< " received decision message"<<  endl;)
                                int s = stoi(token[1]);
                                Proposal p = stringToProposal(token[2]);
                                decisions_[s] = p;

                                Proposal currdecision;
                                while(decisions_.find(slot_num_)!=decisions_.end())
                                {
                                    currdecision = decisions_[slot_num_];
                                    if(proposals_.find(slot_num_)!=proposals_.end())
                                    {
                                        if(!(proposals_[slot_num_]==currdecision))
                                            Propose(proposals_[slot_num_]);
                                    }

                                    Perform(slot_num_, currdecision);
                                    //s has to slot_num. check if it is slotnum in recovery too.
                                    //if so can remove argument from perform, sendresponse functions
                                }
                            }
                            else {    //other messages
                                D(cout << "Unexpected message in Replica " << msg << endl;)
                            }
                        }
                    }
                }
            }
        }
      }
}
/**
 * function for the thread receiving messages from a client with id=client_id
 * @param _rcv_thread_arg argument containing server object and client_id
 */
void* ReceiveMessagesFromClient(void* _rcv_thread_arg) {
    ReceiveThreadArgument *rcv_thread_arg = (ReceiveThreadArgument *)_rcv_thread_arg;
    Server *S = rcv_thread_arg->S;
    int client_id = rcv_thread_arg->client_id;

    char buf[kMaxDataSize];
    int num_bytes;

    while (true) {  // always listen to messages from the client
        num_bytes = recv(S->get_client_chat_fd(client_id), buf, kMaxDataSize - 1, 0);
        if (num_bytes == -1) {
            D(cout << "S" << S->get_pid() << ": ERROR in receiving message from C"
              << client_id << endl;)
            return NULL;
        } else if (num_bytes == 0) {    // connection closed by client
            D(cout << "S" << S->get_pid() << ": ERROR: Connection closed by Client." << endl;)
            return NULL;
        } else {
            buf[num_bytes] = '\0';
            D(cout << "S" << S->get_pid() << ": Message received from C"
              << client_id << " - " << buf << endl;)

            // extract multiple messages from the received buf
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                // token[0] is the CHAT tag
                // token[1] is client id
                // token[2] is chat_id
                // token[3] is the chat message
                if (token[0] == kChat) {
                    Proposal p(token[1], token[2], token[3]);
                    S->Propose(p);
                } else {
                    D(cout << "S" << S->get_pid()
                      << ": ERROR Unexpected message received from C" << client_id
                      << " - " << buf << endl;)
                }
            }
        }
    }
    return NULL;
}

/**
 * thread entry function for replica
 * @param  _S pointer to server class object
 * @return    NULL
 */
void *ReplicaEntry(void *_S) {
    Server *S = (Server*) _S;

    pthread_t accept_connections_thread;
    CreateThread(AcceptConnectionsReplica, (void*)S, accept_connections_thread);

    // sleep for some time to make sure accept threads of commanders and scouts are running
    usleep(kGeneralSleep);
    if (S->ConnectToCommanderR(S->get_primary_id())) {
        D(cout << "SR" << S->get_pid() << ": Connected to commander of S"
          << S->get_primary_id() << endl;)
    } else {
        D(cout << "SR" << S->get_pid() << ": ERROR in connecting to commander of S"
          << S->get_primary_id() << endl;)
        return NULL;
    }

    if (S->ConnectToScoutR(S->get_primary_id())) {
        D(cout << "SR" << S->get_pid() << ": Connected to scout of S"
          << S->get_primary_id() << endl;)
    } else {
        D(cout << "SR" << S->get_pid() << ": ERROR in connecting to scout of S"
          << S->get_primary_id() << endl;)
        return NULL;
    }

    // sleep for some time to make sure all connections are established
    usleep(kGeneralSleep);
    usleep(kGeneralSleep);
    usleep(kGeneralSleep);
    if (S->get_pid() == S->get_primary_id()) {
        S->CreateReceiveThreadsForClients();
    }


    S->Replica();


    void *status;
    pthread_join(accept_connections_thread, &status);


    return NULL;

}