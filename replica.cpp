#include "server.h"
#include "constants.h"
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

extern void* AcceptConnectionsReplica(void* _S);
extern std::vector<string> split(const std::string &s, char delim);
extern void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread);

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
    if (fd == -1 || replica_fd_[server_id] == -1) {
        replica_fd_[server_id] = fd;
    }
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

    void *status;
    pthread_join(accept_connections_thread, &status);
    return NULL;

}