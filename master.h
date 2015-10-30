#ifndef MASTER_H_
#define MASTER_H_
#include "vector"
#include "string"
using namespace std;

class Master {
public:
    bool ReadPortsFile();
    void ReadTest();
    void SpawnServers(const int n);
    void SpawnClient(const int n);

private:
    int num_servers;
    int num_clients;

    std::vector<int> server_pid_;
    std::vector<int> client_pid_;
    std::vector<int> server_listen_port_;
    std::vector<int> client_listen_port_;
    // std::map<int, int> server_listen_port_;
    // std::map<int, int> client_listen_port_;

};
#endif //MASTER_H_