#include "server.h"
#include "constants.h"
#include "iostream"
#include "vector"
#include "string"
#include "fstream"
#include "sstream"
using namespace std;

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

void Server::set_server_paxos_fd(const int server_id, const int fd) {
    if (fd == -1 || server_paxos_fd_[server_id] != -1)
        server_paxos_fd_[server_id] = fd;
}

void Server::set_client_chat_fd(const int client_id, const int fd) {
    if (fd == -1 || client_chat_fd_[client_id] != -1)
        client_chat_fd_[client_id] = fd;
}

void Server::set_pid(const int pid) {
    pid_ = pid;
}

void Server::set_master_fd(const int fd) {
    master_fd_ = fd;
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
 * creates a new thread
 * @param  f function pointer to be passed to the new thread
 * @param  arg arguments for the function f passed to the thread
 * @param  thread thread identifier for new thread
 */
void Server::CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread) {
    if (pthread_create(&thread, NULL, f, arg)) {
        cout << "S" << get_pid() << ": ERROR: Unable to create thread" << endl;
        pthread_exit(NULL);
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
        cout << e.what() << endl;
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
    num_servers_ = num_servers;
    num_clients_ = num_clients;
    server_paxos_fd_.resize(num_servers_, -1);
    client_chat_fd_.resize(num_clients_, -1);
    server_listen_port_.resize(num_servers_);
    client_listen_port_.resize(num_clients_);
    server_paxos_port_.resize(num_servers_);
    client_chat_port_.resize(num_clients_);
}

int main(int argc, char *argv[]) {
    Server S;
    S.Initialize(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
    if (!S.ReadPortsFile())
        return 1;

    pthread_t accept_connections_thread;
    S.CreateThread(AcceptConnections, (void*)&S, accept_connections_thread);

    void* status;
    pthread_join(accept_connections_thread, &status);
    return 0;
}