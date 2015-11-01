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

extern void* AcceptConnectionsServer(void* _S);
void *ReplicaEntry(void *_S);
void *AcceptorEntry(void *_S);
void *LeaderEntry(void *_S);
extern std::vector<string> split(const std::string &s, char delim);
extern void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread);

bool Ballot::operator>(const Ballot &b2) const {
    if (this->seq_num > b2.seq_num)
        return true;
    else if (this->seq_num < b2.seq_num)
        return false;
    else if (this->seq_num == b2.seq_num) {
        if (this->id > b2.id)
            return true;
        else
            return false;
    }
}

bool Ballot::operator<(const Ballot &b2) const {
    return !((*this) >= b2);
}

bool Ballot::operator==(const Ballot &b2) const {
    return ((this->seq_num == b2.seq_num) && (this->id == b2.id));
}

bool Ballot::operator>=(const Ballot &b2) const {
    return ((*this) == b2 || (*this) > b2);
}

bool Ballot::operator<=(const Ballot &b2) const {
    return !((*this) > b2);
}

int Server::get_pid() {
    return pid_;
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

int Server::get_primary_id() {
    return primary_id_;
}

int Server::get_slot_num() {
    return slot_num_;
}

Ballot Server::get_ballot_num() {
    return ballot_num_;
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

void Server::set_primary_id(const int primary_id) {
    primary_id_ = primary_id;
}

void Server::set_slot_num(const int slot_num) {
    slot_num_ = slot_num;
}

void Server::set_ballot_num(const Ballot &ballot_num) {
    ballot_num_.id = ballot_num.id;
    ballot_num_.seq_num = ballot_num.seq_num;
}

/**
 * increments the value of slot_num_
 */
void Server::IncrementSlotNum() {
    set_slot_num(get_slot_num() + 1);
}

/**
 * increments the value of ballot_num_
 */
void Server::IncrementBallotNum() {
    Ballot b = get_ballot_num();
    b.seq_num++;
    set_ballot_num(b);
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
        for (int i = 0; i < num_clients_; i++) {
            fin >> port;
            client_listen_port_[i] = port;
            fin >> port;
            client_chat_port_[i] = port;
            client_chat_port_map_.insert(make_pair(port, i));
        }

        for (int i = 0; i < num_servers_; ++i) {
            fin >> port;
            server_listen_port_[i] = port;

            fin >> port;
            commander_listen_port_[i] = port;

            fin >> port;
            scout_listen_port_[i] = port;

            fin >> port;
            replica_listen_port_[i] = port;

            fin >> port;
            acceptor_port_[i] = port;
            acceptor_port_map_.insert(make_pair(port, i));

            fin >> port;
            replica_port_[i] = port;
            replica_port_map_.insert(make_pair(port, i));

            fin >> port;
            leader_port_[i] = port;
            leader_port_map_.insert(make_pair(port, i));
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
    set_primary_id(0);
    set_slot_num(0);
    set_ballot_num(Ballot(pid, 0));
    num_servers_ = num_servers;
    num_clients_ = num_clients;

    commander_fd_.resize(num_servers_, -1);
    scout_fd_.resize(num_servers_, -1);
    replica_fd_.resize(num_servers_, -1);
    acceptor_fd_.resize(num_servers_, -1);
    leader_fd_.resize(num_servers_, -1);
    client_chat_fd_.resize(num_clients_, -1);

    server_listen_port_.resize(num_servers_);
    client_listen_port_.resize(num_clients_);
    commander_listen_port_.resize(num_servers_);
    scout_listen_port_.resize(num_servers_);
    replica_listen_port_.resize(num_servers_);

    client_chat_port_.resize(num_clients_);
    acceptor_port_.resize(num_servers_);
    replica_port_.resize(num_servers_);
    leader_port_.resize(num_servers_);
}

int main(int argc, char *argv[]) {
    Server S;
    S.Initialize(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
    if (!S.ReadPortsFile()){
        return 1;
    }

    pthread_t accept_connections_thread;
    CreateThread(AcceptConnectionsServer, (void*)&S, accept_connections_thread);

    if (S.get_pid() == S.get_primary_id()) {
        S.CommanderAcceptThread();
        S.ScoutAcceptThread();
    }
    
    pthread_t replica_thread;
    CreateThread(ReplicaEntry, (void*)&S, replica_thread);

    pthread_t acceptor_thread;
    CreateThread(AcceptorEntry, (void*)&S, acceptor_thread);

    if (S.get_pid() == S.get_primary_id()) {
        pthread_t leader_thread;
        CreateThread(LeaderEntry, (void*)&S, leader_thread);
    }

    void *status;
    pthread_join(accept_connections_thread, &status);
    return 0;
}