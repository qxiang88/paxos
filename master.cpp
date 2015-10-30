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

/**
 * reads ports-file and populates port related vectors/maps
 * @return true is ports-file was read successfully
 */
bool Master::ReadPortsFile() {
    ifstream fin;
    fin.exceptions ( ifstream::failbit | ifstream::badbit );
    try {
        fin.open(kPortsFile.c_str());
        int port;
        for (int i = 0; i < num_servers; i++) {
            fin >> port;
            server_listen_port_.push_back(port);
        }

        for (int i = 0; i < num_clients; i++) {
            fin >> port;
            client_listen_port_.push_back(port);
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
            if(!ReadPortsFile()) 
                return ;
            SpawnServers(num_servers);
            SpawnClient(num_clients);
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
 * spawns n servers
 * @param n number of servers to be spawned
 */
void Master::SpawnServers(const int n) {
    pid_t pid;
    int status;
    for (int i = 0; i < n; ++i) {
        char *argv[] = {(char*)kServerExecutable.c_str(), (char*)to_string(i).c_str(), NULL};
        status = posix_spawn(&pid, (char*)kServerExecutable.c_str(), NULL, NULL, argv, environ);
        if (status == 0) {
            cout << "M: Spawed server S" << i << endl;
            server_pid_.push_back(pid);
        } else {
            cout << "M: Error spawing server S" << i << " error:" << strerror(status) << endl;
        }
    }
}

/**
 * spawns n clients
 * @param n number of clients to be spawned
 */
void Master::SpawnClient(const int n) {
    pid_t pid;
    int status;
    for (int i = 0; i < n; ++i) {
        char *argv[] = {(char*)kClientExecutable.c_str(), (char*)to_string(i).c_str(), NULL};
        status = posix_spawn(&pid, (char*)kClientExecutable.c_str(), NULL, NULL, argv, environ);
        if (status == 0) {
            cout << "M: Spawed client C" << i << endl;
            client_pid_.push_back(pid);
        } else {
            cout << "M: Error spawing client C" << i << " error:" << strerror(status) << endl;
        }
    }
}

int main() {
    Master M;
    M.ReadTest();
    return 0;
}