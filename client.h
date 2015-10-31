#ifndef CLIENT_H_
#define CLIENT_H_

#include "vector"
#include "string"
#include "map"
using namespace std;

void* ReceiveMessagesFromMaster(void* _C);

class Client {
public:
    int get_pid();
    int get_master_fd();
    int get_leader_fd();
    int get_master_port();
    int get_server_listen_port(const int server_id);
    int get_my_chat_port();
    int get_my_listen_port();
    int get_leader_id();

    void set_pid(const int pid);
    void set_master_fd(const int fd);
    int set_leader_fd(const int fd);
    int set_leader_id(const int leader_id);

    size_t ChatListSize();
    void Initialize(const int pid,
                    const int num_servers,
                    const int num_clients);
    bool ReadPortsFile();
    void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread);
    void SendChatToLeader(const int chat_id, const string &chat_message);
    void AddChatToChatList(const string &chat);
    bool ConnectToLeader();

private:
    int pid_;   // client's ID
    int num_servers_;
    int num_clients_;
    int leader_id_;

    int master_fd_;
    int leader_fd_;     // fd for communication with leader server

    int master_port_;   // port used by master for communication
    std::vector<int> server_listen_port_;
    int my_chat_port_;
    int my_listen_port_;

    std::vector<string> chat_list_;
};

#endif //CLIENT_H_