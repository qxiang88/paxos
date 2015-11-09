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
pthread_mutex_t mode_lock;
pthread_mutex_t replica_ready_lock;
pthread_mutex_t leader_ready_lock;
pthread_mutex_t acceptor_ready_lock;
pthread_mutex_t all_clear_lock;
pthread_mutex_t message_quota_lock;

// #define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

extern void* AcceptConnectionsServer(void* _S);
extern void* AcceptConnectionsCommander(void* _C);
extern void* AcceptConnectionsScout(void* _SC);

extern void *ReplicaEntry(void *_S);
extern void *AcceptorEntry(void *_S);
extern void *LeaderEntry(void *_S);


int Server::get_pid() {
    return pid_;
}

int Server::get_master_port() {
    return master_port_;
}

int Server::get_replica_port(const int server_id) {
    return replica_port_[server_id];
}

int Server::get_acceptor_port(const int server_id) {
    return acceptor_port_[server_id];
}

int Server::get_leader_port(const int server_id) {
    return leader_port_[server_id];
}

int Server::get_server_listen_port(const int server_id) {
    return server_listen_port_[server_id];
}
Status Server::get_mode() {
    Status m;
    pthread_mutex_lock(&mode_lock);
    m = mode_;
    pthread_mutex_unlock(&mode_lock);
    return m;
}
int Server::get_client_listen_port(const int client_id) {
    return client_listen_port_[client_id];
}

int Server::get_acceptor_listen_port(const int server_id) {
    return acceptor_listen_port_[server_id];
}

int Server::get_leader_listen_port(const int server_id) {
    return leader_listen_port_[server_id];
}

int Server::get_replica_listen_port(const int server_id) {
    return replica_listen_port_[server_id];
}

int Server::get_commander_listen_port(const int server_id) {
    return commander_listen_port_[server_id];
}

int Server::get_scout_listen_port(const int server_id) {
    return scout_listen_port_[server_id];
}

int Server::get_primary_id() {
    return primary_id_;
}

int Server::get_num_servers() {
    return num_servers_;
}

int Server::get_master_fd() {
    return master_fd_;
}

string Server::get_all_clear(string role)
{
    string temp;
    if (role == kLeaderRole)
    {
        pthread_mutex_lock(&all_clear_lock);
        temp = all_clear_[role];
        pthread_mutex_unlock(&all_clear_lock);
    }
    if (role == kReplicaRole)
    {
        pthread_mutex_lock(&all_clear_lock);
        temp = all_clear_[role];
        pthread_mutex_unlock(&all_clear_lock);
    }
    return temp;
}

bool Server::get_leader_ready() {
    bool b;
    pthread_mutex_lock(&leader_ready_lock);
    b = leader_ready_;
    pthread_mutex_unlock(&leader_ready_lock);
    return b;
}

bool Server::get_replica_ready() {
    bool b;
    pthread_mutex_lock(&replica_ready_lock);
    b = replica_ready_;
    pthread_mutex_unlock(&replica_ready_lock);
    return b;
}

bool Server::get_acceptor_ready() {
    bool b;
    pthread_mutex_lock(&acceptor_ready_lock);
    b = acceptor_ready_;
    pthread_mutex_unlock(&acceptor_ready_lock);
    return b;
}

int Server::get_num_clients() {
    return num_clients_;
}

Scout* Server:: get_scout_object() {
    return scout_object_;
}

int Server::get_message_quota() {
    int quota;
    pthread_mutex_lock(&message_quota_lock);
    quota = message_quota_;
    pthread_mutex_unlock(&message_quota_lock);
    return quota;
}

void Server::set_mode(Status m) {
    pthread_mutex_lock(&mode_lock);
    mode_ = m;
    pthread_mutex_unlock(&mode_lock);
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

void Server::set_scout_object() {
    scout_object_ = new Scout(this);
}

void Server::set_all_clear(string role, string status)
{
    if (role == kLeaderRole)
    {
        pthread_mutex_lock(&all_clear_lock);
        all_clear_[role] = status;
        pthread_mutex_unlock(&all_clear_lock);
    }
    if (role == kReplicaRole)
    {
        pthread_mutex_lock(&all_clear_lock);
        all_clear_[role] = status;
        pthread_mutex_unlock(&all_clear_lock);
    }
}

void Server::set_leader_ready(bool b) {
    pthread_mutex_lock(&leader_ready_lock);
    leader_ready_ = b;
    pthread_mutex_unlock(&leader_ready_lock);
}

void Server::set_replica_ready(bool b) {
    pthread_mutex_lock(&replica_ready_lock);
    replica_ready_ = b;
    pthread_mutex_unlock(&replica_ready_lock);
}

void Server::set_acceptor_ready(bool b) {
    pthread_mutex_lock(&acceptor_ready_lock);
    acceptor_ready_ = b;
    pthread_mutex_unlock(&acceptor_ready_lock);
}

void Server::set_message_quota(const int num_messages) {
    pthread_mutex_lock(&message_quota_lock);
    message_quota_ = num_messages;
    pthread_mutex_unlock(&message_quota_lock);
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
 * checks if given port corresponds to an acceptor's port
 * @param  port port number to be checked
 * @return      id of server whose acceptor port matches param port
 * @return      -1 if param port is not the acceptor port of any server
 */
 int Server::IsAcceptorPort(const int port) {
    if (acceptor_port_map_.find(port) != acceptor_port_map_.end()) {
        return acceptor_port_map_[port];
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
            acceptor_listen_port_[i] = port;

            fin >> port;
            leader_listen_port_[i] = port;

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
    const int num_clients,
    int mode,
    int primary_id) {
    set_pid(pid);
    set_primary_id(primary_id);
    num_servers_ = num_servers;
    num_clients_ = num_clients;
    mode_ = static_cast<Status>(mode);

    server_listen_port_.resize(num_servers_);
    client_listen_port_.resize(num_clients_);
    commander_listen_port_.resize(num_servers_);
    scout_listen_port_.resize(num_servers_);
    replica_listen_port_.resize(num_servers_);
    acceptor_listen_port_.resize(num_servers_);
    leader_listen_port_.resize(num_servers_);

    client_chat_port_.resize(num_clients_);
    acceptor_port_.resize(num_servers_);
    replica_port_.resize(num_servers_);
    leader_port_.resize(num_servers_);

    if (pthread_mutex_init(&all_clear_lock, NULL) != 0) {
        D(cout << "S" << get_pid() << " : Mutex init failed" << endl;)
    }
    if (pthread_mutex_init(&replica_ready_lock, NULL) != 0) {
        D(cout << "S" << get_pid() << " : Mutex init failed" << endl;)
    }
    if (pthread_mutex_init(&leader_ready_lock, NULL) != 0) {
        D(cout << "S" << get_pid() << " : Mutex init failed" << endl;)
    }
    if (pthread_mutex_init(&acceptor_ready_lock, NULL) != 0) {
        D(cout << "S" << get_pid() << " : Mutex init failed" << endl;)
    }
    if (pthread_mutex_init(&message_quota_lock, NULL) != 0) {
        D(cout << "S" << get_pid() << " : Mutex init failed" << endl;)
    }

    set_all_clear(kLeaderRole, kAllClearNotSet);
    set_all_clear(kReplicaRole, kAllClearNotSet);

    set_leader_ready(false);
    set_replica_ready(false);
    set_acceptor_ready(false);

    set_message_quota(INT_MAX);
}

/**
 * creates accept thread for commander
 */
 void Server::CommanderAcceptThread(Commander* C) {
    pthread_t accept_connections_thread;
    CreateThread(AcceptConnectionsCommander, (void*)C, accept_connections_thread);
}

/**
 * creates accept thread for scout
 */
 void Server::ScoutAcceptThread(Scout* SC) {
    pthread_t accept_connections_thread;
    CreateThread(AcceptConnectionsScout, (void*)SC, accept_connections_thread);
}

void Server::AllClearPhase()
{
    // sleep(2); //for testing allclear
    set_all_clear(kReplicaRole, kAllClearSet);

    if (get_pid() == get_primary_id())
        set_all_clear(kLeaderRole, kAllClearSet);
    else
        set_all_clear(kLeaderRole, kAllClearDone);

    while ((get_all_clear(kReplicaRole) != kAllClearDone) || (get_all_clear(kLeaderRole) != kAllClearDone))
    {
        usleep(kAllClearSleep);
    }

    string message = kAllClearDone + kInternalDelim + kMessageDelim;
    if (get_master_fd() == -1)
    {
        D(cout << "S" << get_pid() << " : ERROR: Master fd = -1" <<  endl;)
        return;
    }
    if (send(get_master_fd(), message.c_str(), message.size(), 0) == -1) {
        D(cout << "S" << get_pid() << " : ERROR: Cannot send all clear done to master" <<  endl;)
    } else {
        D(cout << "S" << get_pid() << " : All clear done message sent to master" << endl;)
    }
    //wait for messages from leader and replica. once received. send to master
}

void Server::FinishAllClear()
{
    set_all_clear(kReplicaRole, kAllClearNotSet);
    set_all_clear(kLeaderRole, kAllClearNotSet);
}

/**
 * handles functions to be executed on receipt of new primary
 * @param new_primary_id id of the new primary elected
 */
 void Server::HandleNewPrimary(const int new_primary_id) {
    set_primary_id(new_primary_id);

    if (get_pid() != get_primary_id())
        return;

    Commander *C = new Commander(this, get_num_servers());
    CommanderAcceptThread(C);

    set_scout_object();
    ScoutAcceptThread(get_scout_object());

    pthread_t leader_thread;
    CreateThread(LeaderEntry, (void*)this, leader_thread);

    while (!get_leader_ready() || !get_replica_ready() || !get_acceptor_ready()) {
        usleep(kBusyWaitSleep);
    }

    set_leader_ready(false);
    set_acceptor_ready(false);
    set_replica_ready(false);
    SendGoAheadToMaster();
}

/**
 * sends GoAhead message to master
 */
 void Server::SendGoAheadToMaster() {
    string message = kGoAhead + kInternalDelim + kMessageDelim;
    if (send(get_master_fd(), message.c_str(), message.size(), 0) == -1) {
        D(cout << "S" << get_pid() << " : ERROR: Cannot send GOAHEAD done to master" <<  endl;)
    } else {
        D(cout << "S" << get_pid() << " : GOAHEAD sent to master" << endl;)
    }
}

void Server::Die() {
    D(cout << "S" << get_pid() << " : Going down..." <<  endl;)
    close(get_master_fd());
    exit(1);
}

/**
 * checks whether the server has exhausted its message quota or not
 * it the message quota has been exhausted, then it sends a KillMe message to master
 * it sleeps after sending KillMe to master, untill the master kills the process
 */
 void Server::ContinueOrDie() {
    if (get_message_quota() <= 0) {
        Die();
    }
}

/**
 * decrements the message quota by 1
 * after decrementing, it checks whether or not has it exhausted its message quota
 */
 void Server::DecrementMessageQuota() {
    set_message_quota(get_message_quota() - 1);
    ContinueOrDie();
}

void* ReceiveMessagesFromMaster(void* _S ) {
    Server* S = (Server*)_S;
    char buf[kMaxDataSize];
    int num_bytes;
    while (true) {  // always listen to messages from the master
        num_bytes = recv(S->get_master_fd(), buf, kMaxDataSize - 1, 0);
        if (num_bytes == -1) {
            D(cout << "S" << S->get_pid() << " : ERROR in receiving message from M" << endl;)
        } else if (num_bytes == 0) {    // connection closed by master
            D(cout << "S" << S->get_pid() << " : Connection closed by M" << endl;)
        } else {
            buf[num_bytes] = '\0';

            // extract multiple messages from the received buf
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kAllClear) {
                    S->AllClearPhase(); //send to (leader)x and replica
                }
                else if (token[0] == kAllClearRemove) {
                    S->FinishAllClear();
                } else if (token[0] == kNewPrimary) {
                    D(cout << "S" << S->get_pid() << " : Received new primary id S" << token[1] << endl;)
                    S->HandleNewPrimary(stoi(token[1]));
                } else if (token[0] == kTimeBomb) {
                    D(cout << "S" << S->get_pid() << " : Received TimeBomb " << token[1] << endl;)
                    S->set_message_quota(stoi(token[1]));
                    S->SendGoAheadToMaster();
                } else {    //other messages
                    D(cout << "S" << S->get_pid() << " : ERROR Unexpected message received from M" << endl;)
                }
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);

    Server S;
    S.Initialize(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
    if (!S.ReadPortsFile()) {
        return 1;
    }

    pthread_t accept_connections_thread;
    CreateThread(AcceptConnectionsServer, (void*)&S, accept_connections_thread);

    if (S.get_pid() == S.get_primary_id()) {
        Commander *C = new Commander(&S, S.get_num_servers());
        S.CommanderAcceptThread(C);

        S.set_scout_object();
        S.ScoutAcceptThread(S.get_scout_object());
    }

    pthread_t replica_thread;
    CreateThread(ReplicaEntry, (void*)&S, replica_thread);

    pthread_t acceptor_thread;
    CreateThread(AcceptorEntry, (void*)&S, acceptor_thread);

    pthread_t leader_thread;
    if (S.get_pid() == S.get_primary_id()) {
        CreateThread(LeaderEntry, (void*)&S, leader_thread);
    }

    void *status;
    pthread_join(accept_connections_thread, &status);
    pthread_join(replica_thread, &status);
    pthread_join(acceptor_thread, &status);
    pthread_join(leader_thread, &status);
    return 0;
}