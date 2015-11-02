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

class Server {
public:
    void Initialize(const int pid,
                    const int num_servers,
                    const int num_clients);
    int IsReplicaPort(const int port);  // common
    int IsAcceptorPort(const int port);  // common
    int IsLeaderPort(const int port);  // common
    int IsClientChatPort(const int port);  // common
    bool ReadPortsFile(); // server
    // void CreateReceiveThreadsForClients();  //R
    // bool ConnectToServer(const int server_id);
    // void ConnectToAllAcceptors(std::vector<int> &acceptor_peer_fd);

    // void SendP1a(const Ballot& b);  //SC
    // void SendP2a(const Triple& , vector<int> ); //C
    // void SendP1b(const Ballot& b, const unordered_set<Triple> &st); //A
    // void SendP2b(const Ballot& , int);  //A
    // void SendDecision(const Triple& t); //C
    // void SendAdopted(const Ballot& recvd_ballot, unordered_set<Triple> pvalues);    //SC
    // void SendPreEmpted(const Ballot& b);    //C, Scout - remove from here
    // void SendProposal(const int &, const Proposal&);    //R
    // void SendToServers(const string& type, const string& msg);  //C, Scout - remove from here
    // void SendToLeader(const string&);
    void Unicast(const string &type, const string& msg, int r_fd = -1); //common
    // void SendResponseToClient(const int& , const Proposal&);    //R

    // int GetMaxAcceptorFd();
    // void GetAcceptorFdSet(fd_set&, int&);   //C, Scout - remove from here
    // void GetCommanderFdSet(fd_set&, vector<int>&, int&);    //A
    // void AddToCommanderFDSet(const int fd); //A
    // void RemoveFromCommanderFDSet(const int fd);    //A
    // void SendBackOwnFD(const int fd);   //A

    // void IncrementSlotNum();    //R
    // void IncrementBallotNum();  //L
    void CommanderAcceptThread(Commander* C);   //S
    void ScoutAcceptThread(Scout* SC);   //S
    // bool ConnectToCommanderL(const int server_id);  //L
    // bool ConnectToCommanderR(const int server_id);  //R
    // bool ConnectToScoutL(const int server_id);  // L
    // bool ConnectToScoutR(const int server_id);  //R
    // bool ConnectToScoutA(const int server_id);  //A
    // bool ConnectToReplica(const int server_id); //L
    // bool ConnectToAcceptor(const int server_id);    //C

    // void Leader();  // L
    // void Acceptor();    //A
    // void Replica(); //R
    // void Perform(const int&, const Proposal&);  //R
    // void Propose(const Proposal &p);    //R

    int get_pid();  //common
    // set<int> get_commander_fd_set();    //A
    // int get_commander_fd(const int server_id);  // R,L
    // int get_scout_fd(const int server_id);  // R,L,A
    // int get_replica_fd(const int server_id);    //L
    // int get_acceptor_fd(const int server_id);
    // int get_leader_fd(const int server_id); //R
    // int get_client_chat_fd(const int client_id); //R
    int get_num_servers();  // common
    int get_num_clients();  // common
    int get_master_port();  // common
    int get_server_listen_port(const int server_id);  // common
    int get_client_listen_port(const int client_id);  // common
    int get_commander_listen_port(const int server_id);  // common
    int get_scout_listen_port(const int server_id);  // common
    int get_replica_listen_port(const int server_id);  // common
    int get_acceptor_listen_port(const int server_id);  // common
    int get_client_chat_port(const int client_id);  // common
    int get_acceptor_port(const int server_id);
    int get_replica_port(const int server_id);
    int get_leader_port(const int server_id);
    int get_primary_id();  // common
    // int get_slot_num(); //R
    // Ballot get_ballot_num();    //L
    Scout* get_scout_object();

    // void set_commander_fd(const int server_id, const int fd);   //R,L
    // void set_scout_fd(const int server_id, const int fd);   //R,L,A
    // void set_replica_fd(const int server_id, const int fd); //L
    // void set_acceptor_fd(const int server_id, const int fd);
    // void set_leader_fd(const int server_id, const int fd);  //R
    // void set_client_chat_fd(const int client_id, const int fd); //R
    void set_pid(const int pid);  // common
    void set_master_fd(const int fd);  // common
    void set_primary_id(const int primary_id);  // common
    // void set_slot_num(const int slot_num);  //R
    // void set_ballot_num(const Ballot &ballot_num);  //L
    void set_scout_object(Scout* sc);

    std::map<int, Proposal> proposals_;  //R
    std::map<int, Proposal> decisions_; //R
    std::unordered_set<Triple> accepted_;   //A

private:
    int pid_;   // server's ID
    int num_servers_;
    int num_clients_;
    int primary_id_;
    // int slot_num_;  //R

    // Ballot ballot_num_; //L
    // bool leader_active_; //L

    int master_fd_;
    // std::vector<int> commander_fd_; // R,L
    // std::vector<int> scout_fd_; // R,L,A
    // std::vector<int> replica_fd_;   //L
    // std::vector<int> acceptor_fd_;
    // std::vector<int> leader_fd_;    //R
    // std::vector<int> client_chat_fd_;   //R
    // std::set<int> commander_fd_set_;    //A

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
    Triple *toSend;
};

struct ScoutThreadArgument {
    Scout *SC;
    Ballot ball;
};

#endif //SERVER_H_