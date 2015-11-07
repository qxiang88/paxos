#ifndef MASTER_H_
#define MASTER_H_
#include "vector"
#include "string"
#include "fstream"
#include "iostream"
using namespace std;

typedef enum {
    DEAD, RUNNING, RECOVER
} Status;

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
    void SendMessageToServer(const int server_id, const string &message);
    void ReceiveChatLogFromClient(const int client_id, string &chat_log);
    void PrintChatLog(const int client_id, const string &chat_log);
    void ElectNewLeader();
    void TimeBombLeader(const int num_messages);
    void SendAllClearToServers(const string&);
    void WaitForAllClearDone();
    void GetServerFdSet(fd_set& server_fd_set, vector<int>& fds, int& fd_max);
    void ConstructAllClearMessage(string &message, const string& type);
    // int GetServerIdWithFd(int);
    void NewPrimaryElection();
    void ElectNewPrimary();
    void InformClientsAboutNewPrimary();
    void InformServersAboutNewPrimary();
    void WaitForGoAhead();
    void CloseAndUnSetServer(int id);
    int GetServerIdFromFd(int fd);


    int get_server_fd(const int server_id);
    int get_client_fd(const int client_id);
    int get_master_port();
    int get_server_listen_port(const int server_id);
    int get_client_listen_port(const int client_id);
    int get_server_pid(const int server_id);
    int get_client_pid(const int client_id);
    int get_primary_id();
    int get_num_servers();
    Status get_server_status(const int server_id);

    void set_server_pid(const int server_id, const int pid);
    void set_client_pid(const int client_id, const int pid);
    void set_server_fd(const int server_id, const int fd);
    void set_client_fd(const int client_id, const int fd);
    void set_primary_id(const int primary_id);
    void set_server_status(const int server_id, const Status s);

private:
    int num_servers_;
    int num_clients_;
    int primary_id_;

    ofstream* fout_;
    std::vector<Status> server_status_;

    std::vector<int> server_pid_;
    std::vector<int> client_pid_;

    std::vector<int> server_fd_;
    std::vector<int> client_fd_;

    int master_port_;   // port used by master for communication
    std::vector<int> server_listen_port_;
    std::vector<int> client_listen_port_;

};
#endif //MASTER_H_