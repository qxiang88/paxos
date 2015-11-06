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

pthread_mutex_t final_chat_log_lock;

int Client::get_pid() {
    return pid_;
}

int Client::get_master_fd() {
    return master_fd_;
}

int Client::get_primary_fd() {
    return primary_fd_;
}

int Client::get_master_port() {
    return master_port_;
}

int Client::get_primary_listen_port(const int server_id) {
    return primary_listen_port_[server_id];
}

int Client::get_my_chat_port() {
    return my_chat_port_;
}

int Client::get_my_listen_port() {
    return my_listen_port_;
}

int Client::get_primary_id() {
    return primary_id_;
}

void Client::set_pid(const int pid) {
    pid_ = pid;
}

void Client::set_master_fd(const int fd) {
    master_fd_ = fd;
}

int Client::set_primary_fd(const int fd) {
    primary_fd_ = fd;
}

int Client::set_primary_id(const int primary_id) {
    primary_id_ = primary_id;
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
        for (int i = 0; i < num_clients_; i++) {
            if (i == get_pid()) {
                fin >> port;
                my_listen_port_ = port;
                fin >> port;
                my_chat_port_ = port;
            } else {
                fin >> port >> port;
            }
        }

        for (int i = 0; i < num_servers_; i++) {
            fin >> port >> port >> port;
            fin >> port;
            primary_listen_port_[i] = port;
            fin >> port >> port >> port >> port;
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
    set_primary_id(0);
    num_servers_ = num_servers;
    num_clients_ = num_clients;
    primary_listen_port_.resize(num_servers_);
}

/**
 * sends chat message to primary along with chat id
 * @param chat_id      id of the chat message
 * @param chat_message body of chat message
 */
void Client::SendChatToPrimary(const int chat_id, const string &chat_message) {
    string msg = kChat + kInternalDelim +
                 to_string(get_pid()) + kInternalStructDelim +
                 to_string(chat_id) + kInternalStructDelim +
                 chat_message + kMessageDelim;
    int primary_id = get_primary_id();
    if (send(get_primary_fd(), msg.c_str(), msg.size(), 0) == -1) {
        D(cout << "C" << get_pid() << " : ERROR: Cannot send chat message to primary S"
          << primary_id << endl;)
    } else {
        D(cout << "C" << get_pid() << " : Chat message sent to primary S"
          << primary_id << ": " << msg << endl;)
    }
}

/**
 * sends all chats which have not yet been decided to the (new) primary
 */
void Client::ResendChats() {
    for (int i = 0; i < chat_list_.size(); ++i) {
        if (decided_chat_ids_.find(i) == decided_chat_ids_.end()) {
            SendChatToPrimary(i, chat_list_[i]);
        }
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
 * adds a decided chat id to the decided_chat_ids list
 * @param chat_id chat id to be added
 */
void Client::AddToDecidedChatIDs(const int chat_id) {
    decided_chat_ids_.insert(chat_id);
}

/**
 * adds the chat message sent by the primary to final chat log
 * @param sequence_num sequence number of chat message as assigned by Paxos
 * @param sender_index id of original sender of chat message
 * @param body         chat message body
 */
void Client::AddToFinalChatLog(const string &sequence_num,
                               const string &sender_index,
                               const string &body) {
    int seq_num = stoi(sequence_num);

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
        D(cout << "C" << get_pid() << " : ERROR: Cannot send ChatLog M" << endl;)
    } else {
        D(cout << "C" << get_pid() << " : ChatLog sent to M" << endl;)
    }
}

/**
 * handles new primary related tasks, like connecting to new primary
 * and resending undecided chats to new primary
 */
void Client::HandleNewPrimary(const int new_primary) {
    set_primary_id(new_primary);
    
    if (ConnectToPrimary()) {
        D(cout << "C" << get_pid() << " : Connected to primary S" << get_primary_id() << endl;)
    } else {
        D(cout << "C" << get_pid() << " : ERROR in connecting to primary S" << get_primary_id() << endl;)
    }

    ResendChats();
}

/**
 * Initialize all mutex locks
 */
void Client::InitializeLocks() {
    if (pthread_mutex_init(&final_chat_log_lock, NULL) != 0) {
        D(cout << "C" << get_pid() << " : Mutex init failed" << endl;)
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
            D(cout << "C" << C->get_pid() << " : ERROR in receiving message from M" << endl;)
            return NULL;
        } else if (num_bytes == 0) {    // connection closed by master
            D(cout << "C" << C->get_pid() << " : Connection closed by Master. Exiting." << endl;)
            return NULL;
        } else {
            buf[num_bytes] = '\0';

            // extract multiple messages from the received buf
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kChat) {   // new chat message received from master
                    D(cout << "C" << C->get_pid()
                      << " : Chat message received from M: " << token[1] <<  endl;)
                    C->AddChatToChatList(token[1]);
                    C->SendChatToPrimary(C->ChatListSize() - 1, token[1]);
                } else if (token[0] == kChatLog) {  // chat log request from master
                    D(cout << "C" << C->get_pid() << " : ChatLog request received from M" <<  endl;)
                    C->SendChatLogToMaster();
                } else if (token[0] == kNewPrimary) {
                    int new_primary = stoi(token[1]);
                    D(cout << "C" << C->get_pid()
                      << " : New primary id received from M: " << new_primary <<  endl;)
                    C->HandleNewPrimary(new_primary);
                } else {    //other messages
                    D(cout << "C" << C->get_pid()
                      << " : ERROR Unexpected message received from M" <<  endl;)
                }
            }
        }
    }
    return NULL;
}

/**
 * function for client's receive messages from primary thread
 * the recv calls times-out every kReceiveTimeoutTimeval
 * so that it can potentially start receiving from a new primary
 * and not remain stuck in recv waiting from an old dead primary
 * @param _C object of Client class
 */
void* ReceiveMessagesFromPrimary(void* _C) {
    Client* C = (Client*)_C;
    char buf[kMaxDataSize];
    int num_bytes;

    while (true) {  // always listen to messages from the master
        int primary_id = C->get_primary_id();
        int primary_fd = C->get_primary_fd();

        // recv call times out after kReceiveTimeoutTimeval
        num_bytes = recv(primary_fd, buf, kMaxDataSize - 1, 0);
        if (num_bytes == -1) {
            // D(cout << "C" << C->get_pid() <<
            // " : ERROR in receiving message from primary S" << primary_id << endl;)
            usleep(kBusyWaitSleep);
        } else if (num_bytes == 0) {    // connection closed by primary
            D(cout << "C" << C->get_pid() << " : Connection closed by primary S" << primary_id << endl;)
            usleep(kBusyWaitSleep);
        } else {
            buf[num_bytes] = '\0';

            // extract multiple messages from the received buf
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                // token[0] = kresponse
                if (token[0] == kResponse) {
                    D(cout << "C" << C->get_pid()
                      << " : Decision received from primary S" << primary_id << ": " << msg << endl;)
                    // token[1] = sequence number of chat as assigned by Paxos
                    // token[2] = proposal
                    std::vector<string> proposal_token =
                        split(string(token[2]), kInternalStructDelim[0]);
                    // proposal_token[0] = (client id) id of original sender of chat message
                    // proposal_token[1] = (chat id) wrt to original sender of chat message
                    // proposal_token[2] = (msg) chat message body
                    C->AddToFinalChatLog(token[1], proposal_token[1], proposal_token[2]);
                    C->AddToDecidedChatIDs(stoi(token[1]));
                } else {
                    D(cout << "C" << C->get_pid() << " : Unexpected message received: " << msg << endl;)

                }
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);

    Client C;
    C.Initialize(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
    C.InitializeLocks();
    if (!C.ReadPortsFile())
        return 1;

    pthread_t accept_connections_thread;
    CreateThread(AcceptConnections, (void*)&C, accept_connections_thread);

    void* status;
    pthread_join(accept_connections_thread, &status);

    if (C.ConnectToPrimary()) {
        D(cout << "C" << C.get_pid() << " : Connected to primary S" << C.get_primary_id() << endl;)
    } else {
        D(cout << "C" << C.get_pid() << " : ERROR in connecting to primary S" << C.get_primary_id() << endl;)
        //TODO: Need to take some action like increment primary_id?
        //or wait for update command from master?
    }

    pthread_t receive_from_master_thread;
    CreateThread(ReceiveMessagesFromMaster, (void*)&C, receive_from_master_thread);

    pthread_t receive_from_primary_thread;
    CreateThread(ReceiveMessagesFromPrimary, (void*)&C, receive_from_primary_thread);

    pthread_join(receive_from_master_thread, &status);
    pthread_join(receive_from_primary_thread, &status);
    return 0;
}