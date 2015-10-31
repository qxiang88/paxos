#include "client.h"
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

extern void* AcceptConnections(void* _C);
extern std::vector<string> split(const std::string &s, char delim);
extern void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread);

int Client::get_pid() {
    return pid_;
}

int Client::get_master_fd() {
    return master_fd_;
}

int Client::get_leader_fd() {
    return leader_fd_;
}

int Client::get_master_port() {
    return master_port_;
}

int Client::get_server_listen_port(const int server_id) {
    return server_listen_port_[server_id];
}

int Client::get_my_chat_port() {
    return my_chat_port_;
}

int Client::get_my_listen_port() {
    return my_listen_port_;
}

int Client::get_leader_id() {
    return leader_id_;
}

void Client::set_pid(const int pid) {
    pid_ = pid;
}

void Client::set_master_fd(const int fd) {
    master_fd_ = fd;
}

int Client::set_leader_fd(const int fd) {
    leader_fd_ = fd;
}

int Client::set_leader_id(const int leader_id) {
    leader_id_ = leader_id;
}

/**
 * @return size of chat list
 */
size_t Client::ChatListSize() {
    return chat_list_.size();
}
/**
 * reads ports-file and populates port related vectors/maps
 * @return true is ports-file was read successfully
 */
bool Client::ReadPortsFile() {
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
            if (i == get_pid()) {
                my_listen_port_ = port;
            }
        }

        for (int i = 0; i < num_servers_; i++) {
            fin >> port;
            // server_paxos_port
            // client doesn't need this info
        }

        for (int i = 0; i < num_clients_; i++) {
            fin >> port;
            if (i == get_pid()) {
                my_chat_port_ = port;
            }
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
void Client::Initialize(const int pid,
                        const int num_servers,
                        const int num_clients) {
    set_pid(pid);
    set_leader_id(0);
    num_servers_ = num_servers;
    num_clients_ = num_clients;
    server_listen_port_.resize(num_servers_);
}

/**
 * sends chat message to leader along with chat id
 * @param chat_id      id of the chat message
 * @param chat_message body of chat message
 */
void Client::SendChatToLeader(const int chat_id, const string &chat_message) {
    string msg = to_string(chat_id) + kTagDelim + chat_message + kMessageDelim;
    int leader_id = get_leader_id();
    if (send(get_leader_fd(), msg.c_str(), msg.size(), 0) == -1) {
        cout << "M: ERROR: Cannot send chat message to leader S" << leader_id << endl;
        //TODO: Need to take some action like increment leader_id?
        //or wait for update command from master?
    } else {
        cout << "M: Chat message sent to leader S" << leader_id << ": " << msg << endl;
    }
}

/**
 * adds a new chat to the chat list
 * @param chat new chat to be added to the list
 */
void Client::AddChatToChatList(const string &chat) {
    chat_list_.push_back(chat);
}

/**
 * function for client's receive messages from master thread
 * @param _C object of Client class
 */
void* ReceiveMessagesFromMaster(void* _C) {
    Client* C = (Client*)_C;
    char buf[kMaxDataSize];
    int num_bytes;

    while (true) {  // always listen to messages from the master
        num_bytes = recv(C->get_master_fd(), buf, kMaxDataSize - 1, 0);
        if (num_bytes == -1) {
            cout << "C" << C->get_pid() << ": ERROR in receiving message from M" << endl;
            return NULL;
        } else if (num_bytes == 0) {    // connection closed by master
            cout << "C" << C->get_pid() << ": Connection closed by Master. Exiting." << endl;
            return NULL;
        } else {
            buf[num_bytes] = '\0';

            // extract multiple messages from the received buf
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> tokens = split(string(msg), kTagDelim[0]);
                if (tokens[0] == kChat) {   // new chat message received from master
                    cout << "C" << C->get_pid() << ": Chat message received from M: " << tokens[1] <<  endl;
                    C->AddChatToChatList(tokens[1]);
                    C->SendChatToLeader(C->ChatListSize() - 1, tokens[1]);
                } else {    //other messages

                }
            }


        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    Client C;
    C.Initialize(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
    if (!C.ReadPortsFile())
        return 1;

    pthread_t accept_connections_thread;
    CreateThread(AcceptConnections, (void*)&C, accept_connections_thread);

    void* status;
    pthread_join(accept_connections_thread, &status);

    if (C.ConnectToLeader()) {
        cout << "C" << C.get_pid() << ": Connected to leader S" << C.get_leader_id() << endl;
    } else {
        cout << "C" << C.get_pid() << ": ERROR in connecting to leader S" << C.get_leader_id() << endl;
        //TODO: Need to take some action like increment leader_id?
        //or wait for update command from master?
    }

    pthread_t receive_from_master_thread;
    CreateThread(ReceiveMessagesFromMaster, (void*)&C, receive_from_master_thread);

    pthread_join(receive_from_master_thread, &status);
    return 0;
}