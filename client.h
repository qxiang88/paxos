#ifndef CLIENT_H_
#define CLIENT_H_

#include "vector"
#include "string"
#include "map"
using namespace std;

class Client {
public:
    int get_pid();
    int get_master_fd();
    int get_primary_server_fd();
    int get_master_port();
    int get_server_listen_port(const int server_id);
    int get_my_chat_port();
    int get_my_listen_port();

    void set_pid(const int pid);
    void set_master_fd(const int fd);
    int set_primary_server_fd(const int fd);

    void Initialize(const int pid,
                    const int num_servers,
                    const int num_clients);
    bool ReadPortsFile();
    void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread);

private:
    int pid_;   // client's ID
    int num_servers_;
    int num_clients_;

    int master_fd_;
    int primary_server_fd_;     // fd for communication with primary server

    int master_port_;   // port used by master for communication
    std::vector<int> server_listen_port_;
    int my_chat_port_;
    int my_listen_port_;
};

#endif //CLIENT_H_