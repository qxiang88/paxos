#include "client.h"
#include "utilities.h"
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

#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

extern void* AcceptConnections(void* _C);
extern std::vector<string> split(const std::string &s, char delim);
extern void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread);

pthread_mutex_t final_chat_log_lock;

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
    string msg = kChat + kInternalDelim +
                 to_string(get_pid()) + kInternalDelim +
                 to_string(chat_id) + kInternalDelim +
                 chat_message + kMessageDelim;
    int leader_id = get_leader_id();
    if (send(get_leader_fd(), msg.c_str(), msg.size(), 0) == -1) {
        D(cout << "C" << get_pid() << ": ERROR: Cannot send chat message to leader S"
          << leader_id << endl;)
        //TODO: Need to take some action like increment leader_id?
        //or wait for update command from master?
    } else {
        D(cout << "C" << get_pid() << ": Chat message sent to leader S"
          << leader_id << ": " << msg << endl;)
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
 * adds the chat message sent by the leader to final chat log
 * @param sequence_num sequence number of chat message as assigned by Paxos
 * @param sender_index id of original sender of chat message
 * @param body         chat message body
 */
void Client::AddToFinalChatLog(const string &sequence_num,
                               const string &sender_index,
                               const string &body) {
    const int n = sequence_num.size();
    char buf[n + 1];
    sequence_num.copy(buf, sequence_num.size(), 0);
    buf[n] = '\0';
    int seq_num = atoi(buf);

    pthread_mutex_lock(&final_chat_log_lock);

    if (seq_num >= final_chat_log_.size()) {
        final_chat_log_.push_back(FinalChatLog(sequence_num, sender_index, body));
    }

    pthread_mutex_unlock(&final_chat_log_lock);
}

/**
 * constructs final chat log message to be sent to master.
 * acquires the final_chat_log_lock while constructing the message
 * @param msg [out] constructed chat log message
 */
void Client::ConstructChatLogMessage(string &msg) {
    pthread_mutex_lock(&final_chat_log_lock);

    msg = "";
    // msg = to_string(final_chat_log_.size()) + kMessageDelim;
    for (auto const &c : final_chat_log_) {
        msg += c.sequence_num + kInternalDelim
               + c.sender_index + kInternalDelim
               + c.body + kMessageDelim;
    }

    pthread_mutex_unlock(&final_chat_log_lock);
}

/**
 * sends final chat log to the master
 */
void Client::SendChatLogToMaster() {
    string chat_log_message;
    ConstructChatLogMessage(chat_log_message);

    if (send(get_master_fd(), chat_log_message.c_str(),
             chat_log_message.size(), 0) == -1) {
        D(cout << "C" << get_pid() << ": ERROR: Cannot send ChatLog M" << endl;)
    } else {
        D(cout << "C" << get_pid() << ": ChatLog sent to M" << endl;)
    }
}

/**
 * Initialize all mutex locks
 */
void Client::InitializeLocks() {
    if (pthread_mutex_init(&final_chat_log_lock, NULL) != 0) {
        D(cout << "C" << get_pid() << ": Mutex init failed" << endl;)
        pthread_exit(NULL);
    }
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
            D(cout << "C" << C->get_pid() << ": ERROR in receiving message from M" << endl;)
            return NULL;
        } else if (num_bytes == 0) {    // connection closed by master
            D(cout << "C" << C->get_pid() << ": Connection closed by Master. Exiting." << endl;)
            return NULL;
        } else {
            buf[num_bytes] = '\0';

            // extract multiple messages from the received buf
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kChat) {   // new chat message received from master
                    D(cout << "C" << C->get_pid() << ": Chat message received from M: " << token[1] <<  endl;)
                    C->AddChatToChatList(token[1]);
                    C->SendChatToLeader(C->ChatListSize() - 1, token[1]);
                } else if (token[0] == kChatLog) {  // chat log request from master
                    D(cout << "C" << C->get_pid() << ": ChatLog request received from M: " <<  endl;)
                    C->SendChatLogToMaster();
                } else {    //other messages

                }
            }
        }
    }
    return NULL;
}

/**
 * function for client's receive messages from leader thread
 * the recv calls times-out every kReceiveTimeoutTimeval
 * so that it can potentially start receiving from a new leader
 * and not remain stuck in recv waiting from an old dead leader
 * @param _C object of Client class
 */
void* ReceiveMessagesFromLeader(void* _C) {
    Client* C = (Client*)_C;
    char buf[kMaxDataSize];
    int num_bytes;

    while (true) {  // always listen to messages from the master
        int leader_id = C->get_leader_id();
        int leader_fd = C->get_leader_fd();

        // recv call times out after kReceiveTimeoutTimeval
        num_bytes = recv(leader_fd, buf, kMaxDataSize - 1, 0);
        if (num_bytes == -1) {
            // D(cout << "C" << C->get_pid() <<
            // ": ERROR in receiving message from leader S" << leader_id << endl;)
        } else if (num_bytes == 0) {    // connection closed by leader
            // D(cout << "C" << C->get_pid() << ": Connection closed by leader S" << leader_id << endl;)
            //TODO: Need to take some action like increment leader_id?
            //or wait for update command from master?
        } else {
            buf[num_bytes] = '\0';

            // extract multiple messages from the received buf
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                // token[0] = sequence number of chat as assigned by Paxos
                // token[1] = id of original sender of chat message
                // token[2] = chat message body
                C->AddToFinalChatLog(token[0], token[1], token[2]);
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    Client C;
    C.Initialize(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
    C.InitializeLocks();
    if (!C.ReadPortsFile())
        return 1;

    pthread_t accept_connections_thread;
    CreateThread(AcceptConnections, (void*)&C, accept_connections_thread);

    void* status;
    pthread_join(accept_connections_thread, &status);

    if (C.ConnectToLeader()) {
        D(cout << "C" << C.get_pid() << ": Connected to leader S" << C.get_leader_id() << endl;)
    } else {
        D(cout << "C" << C.get_pid() << ": ERROR in connecting to leader S" << C.get_leader_id() << endl;)
        //TODO: Need to take some action like increment leader_id?
        //or wait for update command from master?
    }

    pthread_t receive_from_master_thread;
    CreateThread(ReceiveMessagesFromMaster, (void*)&C, receive_from_master_thread);

    pthread_t receive_from_leader_thread;
    CreateThread(ReceiveMessagesFromLeader, (void*)&C, receive_from_leader_thread);

    pthread_join(receive_from_master_thread, &status);
    return 0;
}