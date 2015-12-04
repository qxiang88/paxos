#ifndef SERVER_H_
#define SERVER_H_
#include "commander.h"
#include "scout.h"
#include "vector"
#include "string"
#include "unordered_set"
#include "map"
#include "utilities.h"
#include "set"
using namespace std;

class Commander;
class Scout;

void* ReceiveMessagesFromClient(void* _rcv_thread_arg); //R
void* ReceiveMessagesFromMaster(void* _S );

typedef enum {
    DEAD, RUNNING, RECOVER
} Status;

class Server {
public:
    void Initialize(const int pid,
                    const int num_servers,
                    const int num_clients,
                    int mode,
                    int primary_id);
    int IsReplicaPort(const int port);
    int IsAcceptorPort(const int port);
    int IsLeaderPort(const int port);
    int IsClientChatPort(const int port);
    bool ReadPortsFile();
    void CommanderAcceptThread(Commander* C);
    void ScoutAcceptThread(Scout* SC);
    void AllClearPhase();
    void FinishAllClear();
    void HandleNewPrimary(const int new_primary_id);
    void SendGoAheadToMaster();
    void Die();
    void ContinueOrDie();
    void DecrementMessageQuota();

    bool get_leader_ready();
    bool get_replica_ready();
    bool get_acceptor_ready();
    int get_pid();
    int get_num_servers();
    int get_num_clients();
    string get_all_clear(string);
    int get_master_port();
    int get_server_listen_port(const int server_id);
    int get_client_listen_port(const int client_id);
    int get_commander_listen_port(const int server_id);
    int get_scout_listen_port(const int server_id);
    int get_replica_listen_port(const int server_id);
    int get_acceptor_listen_port(const int server_id);
    int get_leader_listen_port(const int server_id);
    int get_client_chat_port(const int client_id);
    int get_acceptor_port(const int server_id);
    int get_replica_port(const int server_id);
    int get_leader_port(const int server_id);
    int get_primary_id();
    int get_message_quota();
    Scout* get_scout_object();
    int get_master_fd();
    Status get_mode();

    void set_leader_ready(bool b);
    void set_replica_ready(bool b);
    void set_acceptor_ready(bool b);
    void set_mode(Status);
    void set_pid(const int pid);
    void set_master_fd(const int fd);
    void set_primary_id(const int primary_id);
    void set_scout_object();
    void set_all_clear(string, string);
    void set_message_quota(const int num_messages);

    ~Server();

private:
    int pid_;   // server's ID
    int num_servers_;
    int num_clients_;
    int primary_id_;

    bool leader_ready_;
    bool acceptor_ready_;
    bool replica_ready_;
    Status mode_;
    int message_quota_;

    int master_fd_;

    std::map<string, string> all_clear_;

    int master_port_;   // port used by master for communication
    std::vector<int> server_listen_port_;
    std::vector<int> client_listen_port_;
    std::vector<int> commander_listen_port_;
    std::vector<int> scout_listen_port_;
    std::vector<int> replica_listen_port_;
    std::vector<int> acceptor_listen_port_;
    std::vector<int> leader_listen_port_;

    std::vector<int> client_chat_port_;
    std::vector<int> acceptor_port_;
    std::vector<int> replica_port_;
    std::vector<int> leader_port_;

    std::map<int, int> client_chat_port_map_;
    std::map<int, int> acceptor_port_map_;
    std::map<int, int> replica_port_map_;
    std::map<int, int> leader_port_map_;

    Scout* scout_object_;
};

struct CommanderThreadArgument {
    Commander *C;
    Triple toSend;
};

struct ScoutThreadArgument {
    Scout *SC;
    Ballot ball;
    time_t sleep_time;
};

#endif //SERVER_H_