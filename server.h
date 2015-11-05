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


class Server {
public:
    void Initialize(const int pid,
                    const int num_servers,
                    const int num_clients);
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
    int get_client_chat_port(const int client_id);
    int get_acceptor_port(const int server_id);
    int get_replica_port(const int server_id);
    int get_leader_port(const int server_id);
    int get_primary_id();  // common
    Scout* get_scout_object();
    int get_master_fd();


    void set_pid(const int pid);
    void set_master_fd(const int fd);
    void set_primary_id(const int primary_id);
    void set_scout_object();
    void set_all_clear(string, string);


private:
    int pid_;   // server's ID
    int num_servers_;
    int num_clients_;
    int primary_id_;

    int master_fd_;

    std::map<string, string> all_clear_;

    int master_port_;   // port used by master for communication
    std::vector<int> server_listen_port_;
    std::vector<int> client_listen_port_;
    std::vector<int> commander_listen_port_;
    std::vector<int> scout_listen_port_;
    std::vector<int> replica_listen_port_;
    std::vector<int> acceptor_listen_port_;

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
};

#endif //SERVER_H_