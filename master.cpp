#include "master.h"
#include "constants.h"
#include "utilities.h"
#include "iostream"
#include "vector"
#include "string"
#include "fstream"
#include "sstream"
#include "cstring"
#include "unistd.h"
#include "fcntl.h"
#include "spawn.h"
#include "sys/types.h"
#include "signal.h"
#include "errno.h"
#include "limits.h"
#include "sys/socket.h"
#include "pthread.h"
using namespace std;

// #define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

#define WAIT true
#define NORMAL false

extern char **environ;
pthread_mutex_t primary_id_lock;
pthread_mutex_t proceed_lock;

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

int Master::get_server_pid(const int server_id) {
    return server_pid_[server_id];
}

int Master::get_client_pid(const int client_id) {
    return client_pid_[client_id];
}

int Master::get_primary_id() {
    int p_id;
    pthread_mutex_lock(&primary_id_lock);
    p_id = primary_id_;
    pthread_mutex_unlock(&primary_id_lock);
    return p_id;
}

bool Master::get_proceed() {
    int p;
    pthread_mutex_lock(&proceed_lock);
    p = proceed_;
    pthread_mutex_unlock(&proceed_lock);
    return p;
}

int Master::get_num_servers() {
    return num_servers_;
}

Status Master::get_server_status(const int server_id) {
    return server_status_[server_id];
}

void Master::set_server_pid(const int server_id, const int pid) {
    server_pid_[server_id] = pid;
}

void Master::set_client_pid(const int client_id, const int pid) {
    client_pid_[client_id] = pid;
}

void Master::set_server_fd(const int server_id, const int fd) {
    server_fd_[server_id] = fd;
    SetCloseExecFlag(fd);
}

void Master::set_client_fd(const int client_id, const int fd) {
    client_fd_[client_id] = fd;
    SetCloseExecFlag(fd);
}

void Master::set_primary_id(const int primary_id) {
    pthread_mutex_lock(&primary_id_lock);
    primary_id_ = primary_id;
    pthread_mutex_unlock(&primary_id_lock);
}

void Master::set_proceed(const bool p) {
    pthread_mutex_lock(&proceed_lock);
    proceed_ = p;
    pthread_mutex_unlock(&proceed_lock);
}

void Master::set_server_status(const int server_id, const Status s) {
    server_status_[server_id] = s;
}

void Master::SetCloseExecFlag(const int fd) {
    if (fd == -1)
        return;

    int flags;

    flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
        D(cout << "M  : ERROR in setting CloseExec for " << fd << endl;)
    }
    flags |= FD_CLOEXEC;
    if (fcntl(fd, F_SETFD, flags) == -1) {
        D(cout << "M  : ERROR in setting CloseExec for " << fd << endl;)
    }
}

/**
 * extracts chat message from the sendMessage command
 * @param command the sendMessage command given by user
 * @param message [out] extracted chat message
 */
 void Master::ExtractChatMessage(const string &command, string &message) {
    int n = command.size();
    int i = 0;
    int spaces_count = 0;
    while (i < n && spaces_count != 2) {
        if (command[i] == ' ')
            spaces_count++;
        i++;
    }
    while (i < n) {
        message.push_back(command[i]);
        i++;
    }
}

/**
 * constructs the following templated message from the chat message body
 * CHAT-<chat_message>
 * @param chat_message body of chat message
 * @param message      [out] templated message which can be sent
 */
 void Master::ConstructChatMessage(const string &chat_message, string &message) {
    message = kChat + kInternalDelim + chat_message + kMessageDelim;
}

/**
 * reads ports-file and populates port related vectors/maps
 * @return true is ports-file was read successfully
 */
 bool Master::ReadPortsFile() {
    ifstream fin;
    fin.exceptions ( ifstream::failbit | ifstream::badbit );
    try {
        fin.open((kPortsFile+to_string(get_num_servers())).c_str());
        fin >> master_port_;

        int port;
        for (int i = 0; i < num_clients_; i++) {
            fin >> port;
            client_listen_port_[i] = port;
            fin >> port;
        }

        for (int i = 0; i < num_servers_; i++) {
            fin >> port;
            server_listen_port_[i] = port;
            for (int j = 0; j < 8; ++j) {
                fin >> port;
            }
        }
        fin.close();
        return true;

    } catch (ifstream::failure e) {
        D(cout << e.what() << endl;)
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
        if(line[0]=='#')
            continue;

        std::istringstream iss(line);
        string keyword;
        iss >> keyword;
        if (keyword == kStart) {
            iss >> num_servers_ >> num_clients_;
            Initialize();
            if (!ReadPortsFile())
                return ;
            if (!SpawnServers(num_servers_))
                return;
            if (!SpawnClients(num_clients_))
                return;
            usleep(kGeneralSleep);
            // usleep(kGeneralSleep);
            pthread_t peek_server_activities_thread;
            CreateThread(PeekServerActivities, (void*)this, peek_server_activities_thread);
        }
        if (keyword == kSendMessage) {
            int client_id;
            iss >> client_id;
            string chat_message;
            ExtractChatMessage(line, chat_message);
            string message;
            ConstructChatMessage(chat_message, message);
            SendMessageToClient(client_id, message);
            usleep(kGeneralSleep);
            usleep(kGeneralSleep);
        }
        if (keyword == kCrashServer) {
            int server_id;
            iss >> server_id;
            // set_proceed(false);
            set_proceed(WAIT);
            CrashServer(server_id);
            while (get_proceed() == WAIT) {
                usleep(kBusyWaitSleep);
            }
            // set_proceed(NORMAL);

            // if (server_id == get_primary_id()) {
            //     NewPrimaryElection();
            //     WaitForGoAhead(get_primary_id());
            // }
        }
        if (keyword == kRestartServer) {
            int server_id;
            iss >> server_id;
            if (get_server_status(server_id) != DEAD)
                continue;
            if (!RestartServer(server_id))
                return;

            WaitForGoAhead(server_id);
        }
        if (keyword == kAllClear) {
            usleep(kGeneralSleep);//to ensure all clear is not received before whatever was sent previously.
            usleep(kGeneralSleep);
            usleep(kGeneralSleep);//to give time for client to resend chat if needed
            usleep(kGeneralSleep);

            // set_proceed(WAIT);
            while (get_proceed() == WAIT) {
                usleep(kBusyWaitSleep);
            }
            // set_proceed(NORMAL);

            SendAllClearToServers(kAllClear); //sends to primary server
            WaitForAllClearDone();
            SendAllClearToServers(kAllClearRemove); //sends to primary server
        }
        if (keyword == kTimeBombLeader) {
            int num_messages;
            iss >> num_messages;
            TimeBombLeader(num_messages);
            WaitForGoAhead(get_primary_id());
        }
        if (keyword == kPrintChatLog) {
            int client_id;
            iss >> client_id;
            string message = kChatLog + kInternalDelim;
            SendMessageToClient(client_id, message);
            string chat_log;
            ReceiveChatLogFromClient(client_id, chat_log);
            PrintChatLog(client_id, chat_log);
        }
    }
}

void Master::ConstructAllClearMessage(string &message, const string& type) {
    message = type + kInternalDelim + kMessageDelim;
}

void Master::WaitForGoAhead(const int server_id) {
    char buf[kMaxDataSize];
    int num_bytes;
    if ((num_bytes = recv(get_server_fd(server_id), buf, kMaxDataSize - 1, 0)) == -1) {
        D(cout << "M  : ERROR in receiving GOAHEAD from primary S" << server_id << endl;)
    }
    else if (num_bytes == 0)
    {   //connection closed
        D(cout << "M  : ERROR Connection closed by primary S" << server_id << endl;)
    }
    else
    {
        buf[num_bytes] = '\0';
        std::vector<string> message = split(string(buf), kMessageDelim[0]);
        for (const auto &msg : message)
        {
            std::vector<string> token = split(string(msg), kInternalDelim[0]);
            if (token[0] == kGoAhead)
            {
                D(cout << "M  : GOAHEAD received from S" << server_id << endl;)
            }
        }
    }
}
int Master::GetServerIdFromFd(int fd)
{
    for (int i = 0; i < get_num_servers(); i++)
    {
        if (get_server_fd(i) == fd)
        {
            return i;
        }
    }
}
void Master::CloseAndUnSetServer(int id)
{
    close(get_server_fd(id));
    set_server_fd(id, -1);

}
void Master::WaitForAllClearDone()
{
    fd_set server_set;
    int fd_max, num_bytes;
    int num_servers = get_num_servers();

    std::vector<int> fds;
    int waitfor = 0;
    for (int i = 0; i < num_servers; ++i) {
        if (get_server_status(i) != DEAD)
            waitfor++;
    }
    while (waitfor) {  // always listen to messages from the acceptors
        GetServerFdSet(server_set, fds, fd_max);

        if (fd_max == INT_MIN) {
            usleep(kBusyWaitSleep);
            continue;
        }
        int rv = select(fd_max + 1, &server_set, NULL, NULL, NULL);

        if (rv == -1) { //error in select
            D(cout << "M  : ERROR in select() while waiting for all clear" << endl;)
        } else if (rv == 0) {
            D(cout << "M  : ERROR: Unexpected timeout in select() while waiting for all clear" << endl;)
        } else {
            for (int i = 0; i < fds.size(); i++) {
                if (get_server_status(GetServerIdFromFd(fds[i])) == DEAD)
                    continue;


                if (FD_ISSET(fds[i], &server_set)) { // we got one!!
                    char buf[kMaxDataSize];
                    int serv_id = GetServerIdFromFd(fds[i]);
                    if ((num_bytes = recv(fds[i], buf, kMaxDataSize - 1, 0)) == -1) {
                        CloseAndUnSetServer(serv_id);
                        D(cout << "M: ERROR in receiving all clear done from server S" << serv_id << endl;)
                        waitfor--;
                    }
                    else if (num_bytes == 0)
                    {   //connection closed
                        CloseAndUnSetServer(serv_id);
                        waitfor--;
                        // D(cout << "M: ERROR Connection closed by server S" << i << endl;)
                    }
                    else
                    {
                        buf[num_bytes] = '\0';
                        std::vector<string> message = split(string(buf), kMessageDelim[0]);
                        for (const auto &msg : message)
                        {
                            std::vector<string> token = split(string(msg), kInternalDelim[0]);
                            if (token[0] == kAllClearDone)
                            {
                                D(cout << "M  : S" << serv_id << " is allClearDone" << endl;)
                                waitfor--;
                            }
                            else
                            {
                                D(cout << "M  : ERROR Unexpected message received from server S"
                                  << serv_id << ": " << msg << endl;)
                            }
                        }
                    }
                }
            }
        }
    }
}

void Master::SendAllClearToServers(const string& type)
{
    string message;
    ConstructAllClearMessage(message, type);

    for (int i = 0; i < get_num_servers(); i++)
    {
        if (get_server_status(i) != DEAD) {
            SendMessageToServer(i, message);
        }
    }
}

void Master::GetServerFdSet(fd_set& server_fd_set, vector<int>& server_fd_vec, int& fd_max)
{
    int fd_temp;
    fd_max = INT_MIN;
    server_fd_vec.clear();
    FD_ZERO(&server_fd_set);
    for (int i = 0; i < get_num_servers(); i++)
    {
        if (get_server_status(i) != DEAD) {
            fd_temp = get_server_fd(i);
            if (fd_temp != -1)
            {
                FD_SET(fd_temp, &server_fd_set);
                fd_max = max(fd_max, fd_temp);
                server_fd_vec.push_back(fd_temp);
            }
        }
    }
}

/**
 * sends timebomb message to primary
 * @param num_messages primary's alloted quota of messages
 */
 void Master::TimeBombLeader(const int num_messages) {
    int primary_id = get_primary_id();
    string msg = kTimeBomb + kInternalDelim + to_string(num_messages) + kMessageDelim;
    SendMessageToServer(primary_id, msg);
}

/**
 * performs all tasks related to new primary election
 * and informing servers and clients about the new primary
 */
 void Master::NewPrimaryElection() {
    ElectNewPrimary();
    InformServersAboutNewPrimary();
}

/**
 * elects a new primary among the set of running servers using round robin
 */
 void Master::ElectNewPrimary() {
    int curr_primary = get_primary_id();
    int i = (curr_primary + 1) % num_servers_;
    while (true) {
        if (server_status_[i] != DEAD) {
            set_primary_id(i);
            D(cout << "M  : New primary elected: S" << i << endl;)
            break;
        }
        i = (i + 1) % num_servers_;
    }
}

/**
 * sends id of new primary to each client
 */
 void Master::InformClientsAboutNewPrimary() {
    string msg = kNewPrimary + kInternalDelim + to_string(get_primary_id()) + kMessageDelim;
    for (int i = 0; i < num_clients_; ++i) {
        SendMessageToClient(i, msg);
    }
}

/**
 * sends id of new primary to each server
 */
 void Master::InformServersAboutNewPrimary() {
    string msg = kNewPrimary + kInternalDelim + to_string(get_primary_id()) + kMessageDelim;
    for (int i = 0; i < num_servers_; ++i) {
        if (server_status_[i] != DEAD) {
            SendMessageToServer(i, msg);
        }
    }
}

/**
 * receives chatlog from a client
 * @param client_id id of client from which chatlog is to be received
 * @param chat_log  [out] chatlog received in concatenated string form
 */
 void Master::ReceiveChatLogFromClient(const int client_id, string & chat_log) {
    char buf[kMaxDataSize];
    int num_bytes;
    num_bytes = recv(get_client_fd(client_id), buf, kMaxDataSize - 1, 0);
    if (num_bytes == -1) {
        D(cout << "M  : ERROR in receiving ChatLog from client C" << client_id << endl;)
    } else if (num_bytes == 0) {    // connection closed by master
        D(cout << "M  : Connection closed by client C" << client_id << endl;)
    } else {
        buf[num_bytes] = '\0';
        vector<string> messages = split(string(buf), kMessageDelim[0]);
        for(auto &m: messages)
        {
            std::vector<string> token = split(m, kInternalDelim[0]);
            
            if(token.size()<2){
                D(cout<<"M  : Received chatlog from "<<client_id<<" is empty"<<endl;)
            }
            else
                chat_log = token[1];
        }
        //receives onyl last message if multiple
    }
}

/**
 * prints chat log received from a client in the expected format
 * @param chat_log chat log in concatenated string format
 */
 void Master::PrintChatLog(const int client_id, const string & chat_log) {
    std::vector<string> chat = split(chat_log, kInternalSetDelim[0]);
    for (auto &c : chat) {
        std::vector<string> token = split(c, kInternalStructDelim[0]);
        // fout_[client_id]<<token[0]<<" "<<token[1]<<": "<<token[2]<<endl;
        cout << token[0] << " " << token[1] << ": " << token[2] << endl;
    }
    // fout_[client_id] << "-------------" << endl;
}

/**
 * initialize data members and resize vectors
 */
 void Master::Initialize() {
    set_primary_id(0);
    server_pid_.resize(num_servers_, -1);
    client_pid_.resize(num_clients_, -1);
    server_fd_.resize(num_servers_, -1);
    client_fd_.resize(num_clients_, -1);
    server_listen_port_.resize(num_servers_);
    client_listen_port_.resize(num_clients_);
    // fout_.resize(num_clients_);
    server_status_.resize(num_servers_, RUNNING);
    fout_ = new ofstream[num_clients_];
    for (int i = 0; i < num_clients_; ++i) {
        fout_[i].open(kChatLogFile + to_string(i), ios::app);
    }

    set_proceed(NORMAL);
}

/**
 * restarts server by spawing it again and connecting to it
 * @param  server_id id of server to be restarted
 * @return           true if server was restarted and connected to successfully
 */
 bool Master::RestartServer(const int server_id) {
    if (!SpawnOneServer(server_id, RECOVER))
        return false;

    // sleep for some time to make sure accept threads of the server are running
    usleep(kGeneralSleep);
    usleep(kGeneralSleep);
    if (ConnectToServer(server_id)) {
        D(cout << "M  : Connected to server S" << server_id << endl;)
    } else {
        D(cout << "M  : ERROR: Cannot connect to server S" << server_id << endl;)
        return false;
    }

    set_server_status(server_id, RECOVER);
    return true;
}

/**
 * spawns n servers and connects to them
 * @param n number of servers to be spawned
 * @return true if n servers were spawned and connected to successfully
 */
 bool Master::SpawnServers(const int n) {
    for (int i = 0; i < n; ++i) {
        if (!SpawnOneServer(i, RUNNING))
            return false;
    }

    // sleep for some time to make sure accept threads of servers are running
    usleep(kGeneralSleep);
    usleep(kGeneralSleep);
    for (int i = 0; i < n; ++i) {
        if (ConnectToServer(i)) {
            D(cout << "M  : Connected to server S" << i << endl;)
        } else {
            D(cout << "M  : ERROR: Cannot connect to server S" << i << endl;)
            return false;
        }
    }
    return true;
}

/**
 * spawns 1 servers and connects to them
 * @param server_id id of server to be spawned
 * @return true if server wes spawned successfully
 */
 bool Master::SpawnOneServer(const int server_id, Status mode) {
    pid_t pid;
    int status;
    char server_id_arg[10];
    char num_servers_arg[10];
    char num_clients_arg[10];
    char primary_id_arg[10];
    char mode_arg[10];
    sprintf(server_id_arg, "%d", server_id);
    sprintf(num_servers_arg, "%d", num_servers_);
    sprintf(num_clients_arg, "%d", num_clients_);
    sprintf(primary_id_arg, "%d", get_primary_id());

    if (mode == RECOVER)
        sprintf(mode_arg, "%d", 2);
    else
        sprintf(mode_arg, "%d", 1);

    char *argv[] = {(char*)kServerExecutable.c_str(),
        server_id_arg,
        num_servers_arg,
        num_clients_arg,
        mode_arg,
        primary_id_arg,
        NULL
    };
    status = posix_spawn(&pid,
     (char*)kServerExecutable.c_str(),
     NULL,
     NULL,
     argv,
     environ);
    if (status == 0) {
        D(cout << "M  : Spawned server S" << server_id << endl;)
        set_server_pid(server_id, pid);
    } else {
        D(cout << "M  : ERROR: Cannot spawn server S"
          << server_id << " - " << strerror(status) << endl);
        return false;
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
        char server_id_arg[10];
        char num_servers_arg[10];
        char num_clients_arg[10];
        sprintf(server_id_arg, "%d", i);
        sprintf(num_servers_arg, "%d", num_servers_);
        sprintf(num_clients_arg, "%d", num_clients_);
        char *argv[] = {(char*)kClientExecutable.c_str(),
            server_id_arg,
            num_servers_arg,
            num_clients_arg,
            NULL
        };
        status = posix_spawn(&pid,
         (char*)kClientExecutable.c_str(),
         NULL,
         NULL,
         argv,
         environ);
        if (status == 0) {
            D(cout << "M  : Spawned client C" << i << endl;)
            set_client_pid(i, pid);
        } else {
            D(cout << "M  : ERROR: Cannot spawn client C" << i << " - " << strerror(status) << endl;)
            return false;
        }
    }

    // sleep for some time to make sure accept threads of clients are running
    usleep(kGeneralSleep);
    for (int i = 0; i < n; ++i) {
        if (ConnectToClient(i)) {
            D(cout << "M  : Connected to client C" << i << endl;)
        } else {
            D(cout << "M  : ERROR: Cannot connect to client C" << i << endl;)
            return false;
        }
    }
    return true;
}

/**
 * kills the specified server. Set its pid and fd to -1
 * @param server_id id of server to be killed
 */
 void Master::CrashServer(const int server_id)
 {
    int pid = get_server_pid(server_id);
    if (pid != -1) {
        // TODO: Think whether SIGKILL or SIGTERM
        // TODO: If using SIGTERM, consider signal handling in server.cpp
        // close(get_server_fd(server_id));
        kill(pid, SIGKILL);
        set_server_pid(server_id, -1);
        // set_server_fd(server_id, -1);
        // set_server_status(server_id, DEAD);
        D(cout << "M  : Server S" << server_id << " killed" << endl;)
    }
}

/**
 * kills the specified client. Set its pid and fd to -1
 * @param client_id id of client to be killed
 */
 void Master::CrashClient(const int client_id) {
    int pid = get_client_pid(client_id);
    if (pid != -1) {
        // TODO: Think whether SIGKILL or SIGTERM
        // TODO: If using SIGTERM, consider signal handling in client.cpp
        kill(pid, SIGKILL);
        set_client_pid(client_id, -1);
        set_client_fd(client_id, -1);
    }
}

/**
 * kills all running servers
 */
 void Master::KillAllServers() {
    for (int i = 0; i < num_servers_; ++i) {
        CrashServer(i);
    }
}

/**
 * kills all running clients
 */
 void Master::KillAllClients() {
    for (int i = 0; i < num_clients_; ++i) {
        CrashClient(i);
    }
}

/**
 * sends message to a client
 * @param client_id id of client to which message needs to be sent
 * @param message   message to be sent
 */
 void Master::SendMessageToClient(const int client_id, const string & message) {
    if (send(get_client_fd(client_id), message.c_str(), message.size(), 0) == -1) {
        D(cout << "M  : ERROR: Cannot send message to client C" << client_id << endl;)
    } else {
        D(cout << "M  : Message sent to client C" << client_id << ": " << message << endl;)
    }
}

/**
 * sends message to a server
 * @param server_id id of server to which message needs to be sent
 * @param message   message to be sent
 */
 void Master::SendMessageToServer(const int server_id, const string & message) {
    if (send(get_server_fd(server_id), message.c_str(), message.size(), 0) == -1) {
        D(cout << "M  : ERROR: Cannot send message to server S" << server_id << endl;)
    } else {
        D(cout << "M  : Message sent to server S" << server_id << ": " << message << endl;)
    }
}

/**
 * initializes all locks
 */
 bool Master::InitializeLocks() {

    if (pthread_mutex_init(&primary_id_lock, NULL) != 0) {
        D(cout << "M  : Mutex init failed" << endl;)
        return false;
    }

    if (pthread_mutex_init(&proceed_lock, NULL) != 0) {
        D(cout << "M  : Mutex init failed" << endl;)
        return false;
    }
    return true;
}

/**
 * thread for peeking server activities to check if they are alive or dead
 * if a primary dies, new primary election and wait for go ahead are executed
 * if a non-primary dies
 * @param _M pointer to Master class object
 */
 void* PeekServerActivities(void *_M) {
    Master *M = (Master*) _M;

    fd_set server_set;
    int fd_max, num_bytes;
    int num_servers = M->get_num_servers();
    char buf;

    std::vector<int> fds;
    while (true) {
        M->GetServerFdSet(server_set, fds, fd_max);

        if (fd_max == INT_MIN) {
            usleep(kBusyWaitSleep);
            continue;
        }

        struct timeval timeout = kSelectTimeoutTimeval;
        int rv = select(fd_max + 1, &server_set, NULL, NULL, &timeout);

        if (rv == -1) { //error in select
            D(cout << "M  : ERROR in select() while peeking servers" << endl;)
        } else if (rv == 0) {
            // D(cout << "M  : ERROR: Unexpected timeout in select() while waiting for all clear" << endl;)
        } else {
            for (int i = 0; i < fds.size(); i++) {
                int serv_id = M->GetServerIdFromFd(fds[i]);

                if (M->get_server_status(serv_id) == DEAD)
                    continue;

                if (FD_ISSET(fds[i], &server_set)) { // we got one!!
                    num_bytes = recv(M->get_server_fd(serv_id), &buf, 1, MSG_DONTWAIT | MSG_PEEK);
                    if (num_bytes == 0) {  // connection closed by server
                        M->set_proceed(WAIT);
                        if (serv_id == M->get_primary_id()) {
                            // if a primary dies, it might have been because of timeBombLeader
                            // master might not have closed and reset its fd/pid/status yet
                            close(M->get_server_fd(serv_id));
                            M->set_server_pid(serv_id, -1);
                            M->set_server_fd(serv_id, -1);
                            M->set_server_status(serv_id, DEAD);

                            M->NewPrimaryElection();
                            M->WaitForGoAhead(M->get_primary_id());
                            M->InformClientsAboutNewPrimary();
                        } else {
                            // if a non-primary dies, master must have called crashServer on it
                            // no need to close and set fd/status/pid again.
                            close(M->get_server_fd(serv_id));
                            M->set_server_fd(serv_id, -1);
                            M->set_server_status(serv_id, DEAD);
                        }
                        M->set_proceed(NORMAL);
                        // M->set_proceed(true);
                    } else {
                        // some valid activity detected. Ignore
                    }
                }
            }
        }
    }
    return NULL;
}

int main() {
    signal(SIGPIPE, SIG_IGN);

    Master M;
    if (!M.InitializeLocks())
        return 1;
    M.ReadTest();

    usleep(2000 * 1000);
    D(cout << "M  : GOODBYE. Killing everyone..." << endl;)
    M.KillAllServers();
    M.KillAllClients();
    return 0;
}