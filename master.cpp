#include "master.h"
#include "constants.h"
#include "iostream"
#include "vector"
#include "string"
#include "fstream"
#include "sstream"
#include "cstring"
#include "unistd.h"
#include "spawn.h"
using namespace std;

extern char **environ;

int Master::get_server_fd(const int server_id) {
    return server_fd_[server_id];
}

int Master::get_client_fd(const int client_id) {
    return client_fd_[client_id];
}

int Master::get_master_port() {
    return master_port_;
}

int Master::get_server_listen_port(const int server_id) {
    return server_listen_port_[server_id];
}

int Master::get_client_listen_port(const int client_id) {
    return client_listen_port_[client_id];
}

void Master::set_server_pid(const int server_id, const int pid) {
    server_pid_[server_id] = pid;
}

void Master::set_client_pid(const int client_id, const int pid) {
    client_pid_[client_id] = pid;
}

void Master::set_server_fd(const int server_id, const int fd) {
    server_fd_[server_id] = fd;
}

void Master::set_client_fd(const int client_id, const int fd) {
    client_fd_[client_id] = fd;
}

/**
 * reads ports-file and populates port related vectors/maps
 * @return true is ports-file was read successfully
 */
bool Master::ReadPortsFile() {
    ifstream fin;
    fin.exceptions ( ifstream::failbit | ifstream::badbit );
    try {
        fin.open(kPortsFile.c_str());
        fin >> master_port_;

        int port;
        for (int i = 0; i < num_servers; i++) {
            fin >> port;
            server_listen_port_[i] = port;
        }

        for (int i = 0; i < num_clients; i++) {
            fin >> port;
            client_listen_port_[i] = port;
        }
        fin.close();
        return true;

    } catch (ifstream::failure e) {
        cout << e.what() << endl;
        if (fin.is_open()) fin.close();
        return false;
    }
}

/**
 * reads test commands from stdin
 */
void Master::ReadTest() {
    string line;
    while (getline(std::cin, line)) {
        std::istringstream iss(line);
        string keyword;
        iss >> keyword;
        if (keyword == kStart) {
            iss >> num_servers >> num_clients;
            Initialize();
            if (!ReadPortsFile())
                return ;
            if(!SpawnServers(num_servers))
                return;
            if(!SpawnClients(num_clients))
                return;
        }
        if (keyword == kSendMessage) {

        }
        if (keyword == kCrashServer) {

        }
        if (keyword == kRestartServer) {

        }
        if (keyword == kAllClear) {

        }
        if (keyword == kTimeBombLeader) {

        }
        if (keyword == kPrintChatLog) {

        }
    }
}

/**
 * initialize data members and resize vectors/maps
 */
void Master::Initialize() {
    server_pid_.resize(num_servers);
    client_pid_.resize(num_clients);
    server_fd_.resize(num_servers, -1);
    client_fd_.resize(num_clients, -1);
    server_listen_port_.resize(num_servers);
    client_listen_port_.resize(num_clients);
}

/**
 * spawns n servers and connects to them
 * @param n number of servers to be spawned
 * @return true if n servers were spawned and connected to successfully
 */
bool Master::SpawnServers(const int n) {
    pid_t pid;
    int status;
    for (int i = 0; i < n; ++i) {
        char *argv[] = {(char*)kServerExecutable.c_str(), (char*)to_string(i).c_str(), NULL};
        status = posix_spawn(&pid, (char*)kServerExecutable.c_str(), NULL, NULL, argv, environ);
        if (status == 0) {
            cout << "M: Spawed server S" << i << endl;
            set_server_pid(i, pid);
            if (ConnectToServer(i)) {
                cout << "M: Connected to server S" << i << endl;
            } else {
                cout << "M: Error connecting to server S" << i << endl;
                return false;
            }
        } else {
            cout << "M: Error spawing server S" << i << " error:" << strerror(status) << endl;
            return false;
        }
    }
    return true;
}

/**
 * spawns n clients and connects to them
 * @param n number of clients to be spawned
 * @return true if n client were spawned and connected to successfully
 */
bool Master::SpawnClients(const int n) {
    pid_t pid;
    int status;
    for (int i = 0; i < n; ++i) {
        char *argv[] = {(char*)kClientExecutable.c_str(), (char*)to_string(i).c_str(), NULL};
        status = posix_spawn(&pid, (char*)kClientExecutable.c_str(), NULL, NULL, argv, environ);
        if (status == 0) {
            cout << "M: Spawed client C" << i << endl;
            set_client_pid(i, pid);
            if (ConnectToClient(i)) {
                cout << "M: Connected to client C" << i << endl;
            } else {
                cout << "M: Error connecting to client C" << i << endl;
                return false;
            }
        } else {
            cout << "M: Error spawing client C" << i << " error:" << strerror(status) << endl;
            return false;
        }
    }
    return true;
}

int main() {
    Master M;
    M.ReadTest();
    return 0;
}