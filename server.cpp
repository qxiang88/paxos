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

#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

extern void* AcceptConnections(void* _S);


int Server::get_pid() {
    return pid_;
}

int Server::get_server_paxos_fd(const int server_id) {
    return server_paxos_fd_[server_id];
}

int Server::get_client_chat_fd(const int client_id) {
    return client_chat_fd_[client_id];
}

int Server::get_master_port() {
    return master_port_;
}

int Server::get_server_listen_port(const int server_id) {
    return server_listen_port_[server_id];
}

int Server::get_client_listen_port(const int client_id) {
    return client_listen_port_[client_id];
}

int Server::get_server_paxos_port(const int server_id) {
    return server_paxos_port_[server_id];
}

int Server::get_leader_id() {
    return leader_id_;
}

void Server::set_server_paxos_fd(const int server_id, const int fd) {
    if (fd == -1 || server_paxos_fd_[server_id] == -1)
        server_paxos_fd_[server_id] = fd;
}

void Server::set_client_chat_fd(const int client_id, const int fd) {
    if (fd == -1 || client_chat_fd_[client_id] == -1) {
        client_chat_fd_[client_id] = fd;
    }
}

void Server::set_pid(const int pid) {
    pid_ = pid;
}

void Server::set_master_fd(const int fd) {
    master_fd_ = fd;
}

void Server::set_leader_id(const int leader_id) {
    leader_id_ = leader_id;
}

/**
 * checks if given port corresponds to a server's paxos port
 * @param  port port number to be checked
 * @return      id of server whose paxos port matches param port
 * @return      -1 if param port is not the paxos port of any server
 */
int Server::IsServerPaxosPort(const int port) {
    if (server_paxos_port_map_.find(port) != server_paxos_port_map_.end()) {
        return server_paxos_port_map_[port];
    } else {
        return -1;
    }
}

/**
 * checks if given port corresponds to a client's chat port
 * @param  port port number to be checked
 * @return      id of client whose chat port matches param port
 * @return      -1 if param port is not the chat port of any client
 */
int Server::IsClientChatPort(const int port) {
    if (client_chat_port_map_.find(port) != client_chat_port_map_.end()) {
        return client_chat_port_map_[port];
    } else {
        return -1;
    }
}

/**
 * reads ports-file and populates port related vectors/maps
 * @return true is ports-file was read successfully
 */
bool Server::ReadPortsFile() {
    ifstream fin;
    fin.exceptions ( ifstream::failbit | ifstream::badbit );
    try {
        fin.open(kPortsFile.c_str());
        fin >> master_port_;

        int port;
        for (int i = 0; i < num_servers_; i++) {
            fin >> port;
            server_listen_port_[i] = port;
        }

        for (int i = 0; i < num_clients_; i++) {
            fin >> port;
            client_listen_port_[i] = port;
        }

        for (int i = 0; i < num_servers_; i++) {
            fin >> port;
            server_paxos_port_[i] = port;
            server_paxos_port_map_.insert(make_pair(port, i));
        }

        for (int i = 0; i < num_clients_; i++) {
            fin >> port;
            client_chat_port_[i] = port;
            client_chat_port_map_.insert(make_pair(port, i));
        }

        fin.close();
        return true;

    } catch (ifstream::failure e) {
        D(cout << e.what() << endl;)
        if (fin.is_open()) fin.close();
        return false;
    }
}

/**
 * initialize data members and resize vectors
 * @param  pid process's self id
 * @param  num_servers number of servers
 * @param  num_clients number of clients
 */
void Server::Initialize(const int pid,
                        const int num_servers,
                        const int num_clients) {
    set_pid(pid);
    set_leader_id(0);
    num_servers_ = num_servers;
    num_clients_ = num_clients;
    server_paxos_fd_.resize(num_servers_, -1);
    client_chat_fd_.resize(num_clients_, -1);
    server_listen_port_.resize(num_servers_);
    client_listen_port_.resize(num_clients_);
    server_paxos_port_.resize(num_servers_);
    client_chat_port_.resize(num_clients_);
}

/**
 * connects to all servers whose id is less than self
 * this logic makes sure that a pair is connected only once
 */
void Server::ConnectToOtherServers() {
    for (int i = 0; i < get_pid(); ++i) {
        if (ConnectToServer(i)) {
            D(cout << "S" << get_pid() << ": Connected to server S" << i << endl;)
        } else {
            //TODO: Do we need to update something when a server realizes that
            // another server is dead?
        }
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
            D(cout << "S" << S->get_pid() << ": Message received from C" << client_id << " - " << buf << endl;)

            // extract multiple messages from the received buf
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                // token[0] is chat_id
                // token[1] is the chat message
            }
        }
    }
    return NULL;
}


fd_set Server::GetAcceptorFdSet()
{
    fd_set acceptor_set;
    int fd_max=INT_MIN, fd_temp;
    FD_ZERO(&acceptor_set);
    for (int i=0; i<S->get_num_servers(); i++) {
        fd_temp = S->get_acceptor_fd(i);
        FD_SET(fd_temp, &acceptor_set);
        fd_max = max(fd_max, fd_temp);
    }
    return acceptor_set;
}

int Server::GetMaxAcceptorFd()
{
    int fd_max=INT_MIN, fd_temp;
    for (int i=0; i<S->get_num_servers(); i++) {
        fd_temp = S->get_acceptor_fd(i);
        fd_max = max(fd_max, fd_temp);
    }
    return fd_max;
}

void Server::SendToServers(const string& type, const string& msg)
{
    int serv_fd;
    for(int i=0;i<num_servers_; i++)
    {
        if(i==get_pid())
        {
            if(type==kDecision)
                serv_fd = get_self_replica_fd(i);
            else if(type==kP2a || type==kP1a)
                serv_fd = get_self_acceptor_fd(i);
        }
        else{
            if(type==kDecision)
                serv_fd = get_replica_fd(i);
            else if(type==kP2a || type==kP1a)
                serv_fd = get_acceptor_fd(i);
        }

        if (send(serv_fd, msg.c_str(), msg.size(), 0) == -1) {
            D(cout << "P" << get_pid() << ": ERROR: sending to P" << (it->first) << endl;)
        }
        else {
            D(cout << "P" << get_pid() << ": Msg sent to P" << (it->first) << ": " << msg << endl;)
        }
    }    
}


void Server::SendToLeader(const string&msg)
{
    int serv_fd = get_self_leader_fd(); //confirm line
    if (send(serv_fd, msg.c_str(), msg.size(), 0) == -1) {
        D(cout << ": ERROR: sending to leader" << endl;)
    }
    else {
        D(cout << ": Msg sent to leader" << endl;)
    }
}

void Server::SendPreEmpted(const Ballot& b)
{
    string msg = kPreEmpted + kInternalDelim + ballotToString(b)+ kMessageDelim;
    SendToLeader(kPreEmpted, msg);
}

int main(int argc, char *argv[]) {
    Server S;
    S.Initialize(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
    if (!S.ReadPortsFile())
        return 1;

    pthread_t accept_connections_thread;
    CreateThread(AcceptConnections, (void*)&S, accept_connections_thread);

    // sleep for some time to make sure accept threads of all servers are running
    usleep(kGeneralSleep);
    S.ConnectToOtherServers();

    // sleep for some time to make sure all connections are established
    usleep(kGeneralSleep);
    usleep(kGeneralSleep);
    usleep(kGeneralSleep);
    if (S.get_pid() == S.get_leader_id()) {
        S.CreateReceiveThreadsForClients();
    }

    void* status;
    pthread_join(accept_connections_thread, &status);
    return 0;
}