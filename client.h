#ifndef CLIENT_H_
#define CLIENT_H_

#include "vector"
#include "string"
#include "map"
#include "unordered_set"
using namespace std;

void* ReceiveMessagesFromMaster(void* _C);
void* ReceiveMessagesFromPrimary(void* _C);

struct FinalChatLog {
    string sequence_num;
    string sender_index;
    string body;

    FinalChatLog(const string &seq_num,
                 const string &sender_idx,
                 const string &body)
        : sequence_num(seq_num),
          sender_index(sender_idx),
          body(body) { }
};

class Client {
public:
    size_t ChatListSize();
    void Initialize(const int pid,
                    const int num_servers,
                    const int num_clients);
    bool ReadPortsFile();
    void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread);
    void SendChatToPrimary(const int chat_id, const string &chat_message);
    void AddChatToChatList(const string &chat);
    bool ConnectToPrimary();
    void AddToFinalChatLog(const string &sequence_number,
                           const string &sender_index,
                           const string &body);
    void InitializeLocks();
    void ConstructChatLogMessage(string &msg);
    void SendChatLogToMaster();
    void ResendChats();
    void AddToDecidedChatIDs(const int chat_id);


    int get_pid();
    int get_master_fd();
    int get_primary_fd();
    int get_master_port();
    int get_primary_listen_port(const int server_id);
    int get_my_chat_port();
    int get_my_listen_port();
    int get_primary_id();

    void set_pid(const int pid);
    void set_master_fd(const int fd);
    int set_primary_fd(const int fd);
    int set_primary_id(const int primary_id);

private:
    int pid_;   // client's ID
    int num_servers_;
    int num_clients_;
    int primary_id_;

    int master_fd_;
    int primary_fd_;     // fd for communication with primary server

    int master_port_;   // port used by master for communication
    std::vector<int> primary_listen_port_;
    int my_chat_port_;
    int my_listen_port_;

    std::vector<string> chat_list_;
    std::vector<FinalChatLog> final_chat_log_;
    std::unordered_set<int> decided_chat_ids_;
};

#endif //CLIENT_H_