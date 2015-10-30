#ifndef MASTER_H_
#define MASTER_H_
#include "vector"
#include "string"
using namespace std;

class Master {
public:
    bool ReadPortsFile();
    void ReadTest();
    void Initialize();
    bool SpawnServers(const int n);
    bool SpawnClients(const int n);
    void CrashServer(const int server_id);
    void CrashClient(const int client_id);
    void KillAllServers();
    void KillAllClients();
    bool ConnectToServer(const int server_id);
    bool ConnectToClient(const int client_id);
    void ExtractChatMessage(const string &command, string &message);
    void ConstructChatMessage(const string &chat_message, string &message);
    void SendMessageToClient(const int client_id, const string &message);

    int get_server_fd(const int server_id);
    int get_client_fd(const int client_id);
    int get_master_port();
    int get_server_listen_port(const int server_id);
    int get_client_listen_port(const int client_id);
    int get_server_pid(const int server_id);
    int get_client_pid(const int client_id);

    void set_server_pid(const int server_id, const int pid);
    void set_client_pid(const int client_id, const int pid);
    void set_server_fd(const int server_id, const int fd);
    void set_client_fd(const int client_id, const int fd);

private:
    int num_servers_;
    int num_clients_;

    std::vector<int> server_pid_;
    std::vector<int> client_pid_;

    std::vector<int> server_fd_;
    std::vector<int> client_fd_;

    int master_port_;   // port used by master for communication
    std::vector<int> server_listen_port_;
    std::vector<int> client_listen_port_;
    // std::map<int, int> server_listen_port_;
    // std::map<int, int> client_listen_port_;

};
#endif //MASTER_H_