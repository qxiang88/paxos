#include "server.h"
#include "replica.h"
#include "constants.h"
#include "utilities.h"
#include "iostream"
#include "vector"
#include "string"
#include "fstream"
#include "sstream"
#include "unistd.h"
#include "signal.h"
#include "errno.h"
#include "sys/socket.h"
#include "limits.h"
using namespace std;

typedef pair<int, Proposal> SPtuple;
pthread_mutex_t decisions_lock;

#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

bool map_compare(std::map<int, Proposal> &lhs, std::map<int, Proposal> &rhs) {
    for (auto &l : lhs) {
        if (rhs.find(l.first) == rhs.end())
            return false;
        else if (!(l.second == rhs.find(l.first)->second))
            return false;
    }
    return true;
}

Replica::~Replica() {

}

Replica::Replica(Server* _S) {
    S = _S;
    set_slot_num(0);

    int num_servers = S->get_num_servers();
    int num_clients = S->get_num_clients();

    commander_fd_.resize(num_servers, -1);
    // scout_fd_.resize(num_servers, -1);
    leader_fd_.resize(num_servers, -1);
    client_chat_fd_.resize(num_clients, -1);
    replica_fd_.resize(num_servers, -1);

    if (pthread_mutex_init(&decisions_lock, NULL) != 0) {
        D(cout << "SR" << S->get_pid() << ": Mutex init failed" << endl;)
    }

}

int Replica::get_commander_fd(const int server_id) {
    return commander_fd_[server_id];
}

// int Replica::get_scout_fd(const int server_id) {
//     return scout_fd_[server_id];
// }

int Replica::get_leader_fd(const int server_id) {
    return leader_fd_[server_id];
}

int Replica::get_replica_fd(const int server_id) {
    return replica_fd_[server_id];
}

int Replica::get_client_chat_fd(const int client_id) {
    return client_chat_fd_[client_id];
}

int Replica::get_slot_num() {
    return slot_num_;
}

map<int, Proposal> Replica::get_decisions() {
    map<int, Proposal> d;
    pthread_mutex_lock(&decisions_lock);
    d = decisions_;
    pthread_mutex_unlock(&decisions_lock);
    return d;
}

void Replica::set_decisions(map<int, Proposal>& d) {
    pthread_mutex_lock(&decisions_lock);
    decisions_ = d;
    pthread_mutex_unlock(&decisions_lock);
}

void Replica::set_commander_fd(const int server_id, const int fd) {
    commander_fd_[server_id] = fd;
}

// void Replica::set_scout_fd(const int server_id, const int fd) {
//     scout_fd_[server_id] = fd;
// }

void Replica::set_leader_fd(const int server_id, const int fd) {
    leader_fd_[server_id] = fd;
}
void Replica::set_replica_fd(const int server_id, const int fd) {
    replica_fd_[server_id] = fd;
}

void Replica::set_client_chat_fd(const int client_id, const int fd) {
    client_chat_fd_[client_id] = fd;
}

void Replica::set_slot_num(const int slot_num) {
    slot_num_ = slot_num;
}

/*** increments the value of slot_num_*/
void Replica::IncrementSlotNum() {
    set_slot_num(get_slot_num() + 1);
}

void Replica::Unicast(const string &type, const string& msg, const int primary_id)
{
    if (send(get_leader_fd(primary_id), msg.c_str(), msg.size(), 0) == -1) {
        D(cout << "SR" << S->get_pid()
          << ": ERROR in sending" << type << " to leader S" << primary_id << endl;)
    }
    else {
        D(cout << "SR" << S->get_pid() << ": " << type
          << " message sent to primary's leader S" << primary_id << ": " << msg << endl;)
    }
}

/**
 * proposes the proposal to a leader
 * @param p Proposal to be proposed
 */
void Replica::Propose(const Proposal &p, const int primary_id) {
    for (auto it = decisions_.begin(); it != decisions_.end(); ++it ) {
        if (it->second == p)
            return;
    }

    int min_slot;
    if (proposals_.rbegin() == proposals_.rend())
        min_slot = 0;
    else {
        min_slot = proposals_.rbegin()->first + 1;
    }

    while (decisions_.find(min_slot) != decisions_.end())
        min_slot++;

    proposals_[min_slot] = p;
    SendProposal(min_slot, p, primary_id);
}

/**
 * sends proposal to a leader
 * @param s proposed slot num for this proposal
 * @param p proposal to be proposed
 */
void Replica::SendProposal(const int& s, const Proposal& p, const int primary_id)
{
    string msg = kPropose + kInternalDelim;
    msg += to_string(s) + kInternalDelim;
    msg += proposalToString(p) + kMessageDelim;
    Unicast(kPropose, msg, primary_id);
}

/**
 * performs the decision reached by Paxos by adding it to decisions,
 * incrementing slot num, followed by sending decision to client
 * @param slot decided slot num
 * @param p    decided proposal
 */
void Replica::Perform(const int& slot, const Proposal& p, const int primary_id)
{
    if (p.msg == kNoop) {
        IncrementSlotNum();
        return;
    }

    for (auto it = decisions_.begin(); it != decisions_.end(); it++)
    {
        if (it->second == p && it->first < get_slot_num())
        {
            IncrementSlotNum();
            return;
        }
    }

    IncrementSlotNum();
    SendResponseToAllClients(slot, p, primary_id);
}

/**
 * sends the decided proposal to all clients
 * @param s decided slot num
 * @param p proposal to be sent
 */
void Replica::SendResponseToAllClients(const int& s,
                                       const Proposal& p,
                                       const int primary_id)
{
    if (S->get_pid() != primary_id)
        return;

    string msg = kResponse + kInternalDelim;
    msg += to_string(s) + kInternalDelim;
    msg += proposalToString(p) + kMessageDelim;

    for (int i = 0; i < S->get_num_clients(); ++i) {
        if (get_client_chat_fd(i) == -1) {
            D(cout << "SR" << S->get_pid()
              << ": ERROR: Unexpected fd=-1 for client C" << i << endl;)
            continue;
        }
        if (send(get_client_chat_fd(i), msg.c_str(), msg.size(), 0) == -1) {
            D(cout << "SR" << S->get_pid() << ": ERROR: sending response to client C"
              << i << endl;)
            close(get_client_chat_fd(i));
            set_client_chat_fd(i, -1);
        }
        else {
            D(cout << "SR" << S->get_pid() << ": Message sent to client C"
              << i << ": " << msg << endl;)
        }
    }
}

void Replica::ProposeBuffered(const int primary_id)
{
    for (auto pit = buffered_proposals_.begin(); pit != buffered_proposals_.end(); pit++)
    {
        Propose(*pit, primary_id);
    }
    buffered_proposals_.clear();
}

void Replica::CheckReceivedAllDecisions(map<int, Proposal>& allDecisions)
{
    if (map_compare (allDecisions, decisions_))
    {
        D(cout << "SR" << S->get_pid()
          << ": Has received every decision in all decisions(" << allDecisions.size() << ")" << endl;)
        S->set_all_clear(kReplicaRole, kAllClearDone);
        allDecisions.clear();
        allDecisions[-1] = Proposal("", "", "");
    }
    else
    {
        D(cout << "SR" << S->get_pid() << ": Has not received every decision" << endl;)
    }
}

void Replica::CreateFdSet(fd_set& fromset, vector<int> &fds,
                          int& fd_max, const int primary_id)
{
    fd_max = INT_MIN;
    char buf;
    int fd_temp;
    FD_ZERO(&fromset);
    fds.clear();
    for (int i = 0; i < S->get_num_clients(); i++)
    {
        fd_temp = get_client_chat_fd(i);
        if (fd_temp == -1) {
            continue;
        }
        int rv = recv(fd_temp, &buf, 1, MSG_DONTWAIT | MSG_PEEK);
        if (rv == 0) {
            close(fd_temp);
            set_client_chat_fd(i, -1);
        } else {
            FD_SET(fd_temp, &fromset);
            fd_max = max(fd_max, fd_temp);
            fds.push_back(fd_temp);
        }
    }

    for (int i = 0; i < S->get_num_servers(); i++)
    {
        fd_temp = get_commander_fd(i);
        if (fd_temp == -1)
            continue;
        int rv = recv(fd_temp, &buf, 1, MSG_DONTWAIT | MSG_PEEK);
        if (rv == 0) {
            close(fd_temp);
            set_commander_fd(i, -1);
        } else {
            fd_max = max(fd_max, fd_temp);
            fds.push_back(fd_temp);
            FD_SET(fd_temp, &fromset);
        }

    }

    fd_temp = get_leader_fd(primary_id);
    if (fd_temp != -1) {
        int rv = recv(fd_temp, &buf, 1, MSG_DONTWAIT | MSG_PEEK);
        if (rv == 0) {
            close(fd_temp);
            set_leader_fd(primary_id, -1);
        } else {
            fd_max = max(fd_max, fd_temp);
            fds.push_back(fd_temp);
            FD_SET(fd_temp, &fromset);
        }
    }

    for (int i = 0; i < S->get_num_servers(); i++)
    {
        fd_temp = get_replica_fd(i);
        if (fd_temp == -1)
            continue;
        int rv = recv(fd_temp, &buf, 1, MSG_DONTWAIT | MSG_PEEK);
        if (rv == 0) {
            close(fd_temp);
            set_replica_fd(i, -1);
        } else {
            fd_max = max(fd_max, fd_temp);
            fds.push_back(fd_temp);
            FD_SET(fd_temp, &fromset);
        }

    }
}

void Replica::ResetFD(const int fd, const int primary_id) {
    if (fd == get_leader_fd(primary_id)) {
        set_leader_fd(primary_id, -1);
        close(fd);
        return;
    }

    for (int i = 0; i < S->get_num_servers(); ++i) {
        if (fd == get_commander_fd(i)) {
            set_commander_fd(i, -1);
            close(fd);
            return;
        }
    }

    for (int i = 0; i < S->get_num_clients(); ++i) {
        if (fd == get_client_chat_fd(i)) {
            set_client_chat_fd(i, -1);
            close(fd);
            return;
        }
    }

    for (int i = 0; i < S->get_num_servers(); ++i) {
        if (fd == get_replica_fd(i)) {
            set_replica_fd(i, -1);
            close(fd);

            if (S->get_pid() != primary_id)
                continue;

            Scout* scout_obj = S->get_scout_object();
            close(scout_obj->get_acceptor_fd(i));
            scout_obj->set_acceptor_fd(i, -1);

            Commander* C = new Commander(S);
            close(C->get_replica_fd(i));
            C->set_replica_fd(i, -1);
            delete C;

            return;
        }
    }


}

void Replica::CheckAndDecrementWaitFor(vector<int>& waitfor, const int& s_fd)
{
    if (S->get_mode() != RECOVER) {
        return;
    }
    for (int i = 0; i < S->get_num_servers(); ++i) {
        if (s_fd == get_replica_fd(i)) {
            for (int j = 0; j < waitfor.size(); j++)
            {
                if (waitfor[j] == i)
                {
                    waitfor.erase(waitfor.begin() + j);
                    return;
                }
            }

        }
    }
}

/**
 * function for performing replica related job
 */

void Replica::ReplicaMode(const int primary_id)
{
    char buf[kMaxDataSize];
    int num_bytes;

    fd_set fromset;
    vector<int> fds;
    int fd_max;
    vector<int> waitfor;

    if (S->get_mode() == RECOVER) {
        waitfor = SendDecisionsRequest();
    }
    map<int, Proposal> allDecs;
    allDecs[-1] = Proposal("", "", "");

    while (true) {  // always listen to messages from the acceptors

        if (primary_id != S->get_primary_id()) {   // new primary elected
            set_commander_fd(primary_id, -1);
            // set_scout_fd(primary_id, -1);
            set_leader_fd(primary_id, -1);
            return;
        }
        CreateFdSet(fromset, fds, fd_max, primary_id);

        if (fd_max == INT_MIN) {
            usleep(kBusyWaitSleep);
            continue;
        }

        //if just become not set, then propose buffered
        if ((S->get_all_clear(kReplicaRole) == kAllClearNotSet) && (!buffered_proposals_.empty()))
            ProposeBuffered(primary_id);
        if ((S->get_all_clear(kReplicaRole) == kAllClearSet) && (allDecs.find(-1) == allDecs.end()) )
            CheckReceivedAllDecisions(allDecs);

        struct timeval timeout = kSelectTimeoutTimeval;
        int rv = select(fd_max + 1, &fromset, NULL, NULL, &timeout);
        if (rv == -1) { //error in select
            D(cout << "SR" << S->get_pid() << ": ERROR in select() errno=" << errno << " fdmax=" << fd_max << endl;)
        } else if (rv == 0) {
            // D(cout << "SR" << S->get_pid() << ": ERROR Unexpected select timeout" << endl;)
        } else {
            for (int i = 0; i < fds.size(); i++) {
                if (FD_ISSET(fds[i], &fromset)) { // we got one!!
                    char buf[kMaxDataSize];
                    if ((num_bytes = recv(fds[i], buf, kMaxDataSize - 1, 0)) == -1) {
                        D(cout << "SR" << S->get_pid()
                          << ": ERROR in receiving" << endl;)
                        ResetFD(fds[i], primary_id);
                        CheckAndDecrementWaitFor(waitfor, fds[i]);
                    } else if (num_bytes == 0) {     //connection closed
                        D(cout << "SR" << S->get_pid() << ": Connection closed" << endl;)
                        ResetFD(fds[i], primary_id);
                        CheckAndDecrementWaitFor(waitfor, fds[i]);
                    } else {
                        buf[num_bytes] = '\0';
                        std::vector<string> message = split(string(buf), kMessageDelim[0]);
                        for (const auto &msg : message) {
                            std::vector<string> token = split(string(msg), kInternalDelim[0]);
                            if (token[0] == kChat)
                            {
                                D(cout << "SR" << S->get_pid() << ": Received chat from client: " << msg <<  endl;)
                                Proposal p = stringToProposal(token[1]);
                                if (S->get_all_clear(kReplicaRole) != kAllClearNotSet)
                                {
                                    D(cout << "SR" << S->get_pid() << ": Buffering propose - " << token[1] << endl;)
                                    buffered_proposals_.push_back(p);
                                }
                                else
                                {
                                    ProposeBuffered(primary_id);
                                    Propose(p, primary_id);
                                }
                            }
                            else if (token[0] == kDecision)
                            {
                                D(cout << "SR" << S->get_pid() << ": Received decision from commander: " << msg <<  endl;)
                                int s = stoi(token[1]);
                                Proposal p = stringToProposal(token[2]);
                                decisions_[s] = p;

                                Proposal currdecision;
                                int slot_num = get_slot_num();
                                while (decisions_.find(slot_num) != decisions_.end())
                                {
                                    currdecision = decisions_[slot_num];
                                    if (proposals_.find(slot_num) != proposals_.end())
                                    {
                                        if (!(proposals_[slot_num] == currdecision))
                                        {
                                            if (S->get_all_clear(kReplicaRole) != kAllClearNotSet)
                                            {
                                                D(cout << "SR" << S->get_pid() << ": Buffering propose - " << token[1] << endl;)
                                                buffered_proposals_.push_back(proposals_[slot_num]);
                                            }
                                            else
                                            {
                                                ProposeBuffered(primary_id);
                                                Propose(proposals_[slot_num], primary_id);
                                            }
                                        }
                                    }
                                    Perform(slot_num, currdecision, primary_id);
                                    slot_num = get_slot_num();
                                    //s has to slot_num. check if it is slotnum in recovery too.
                                    //if so can remove argument from perform, sendresponse functions
                                }

                                if (allDecs.find(-1) == allDecs.end()) //means allDecs has been received
                                {
                                    CheckReceivedAllDecisions(allDecs);
                                }
                            }
                            else if (token[0] == kAllDecisions)
                            {
                                if (S->get_mode() == RECOVER)
                                {
                                    D(cout << "SR" << S->get_pid() << ": All decisions response message received: " << msg <<  endl;)
                                    CheckAndDecrementWaitFor(waitfor, fds[i]);
                                    map<int, Proposal> receivedAllDecisions;
                                    if (token.size() != 1)
                                        stringToAllDecisions(token[1], receivedAllDecisions);

                                    MergeDecisions(receivedAllDecisions);
                                    if (waitfor.empty()) {
                                        S->set_mode(RUNNING);
                                        S->SendGoAheadToMaster();
                                        D(cout << "SR" << S->get_pid() << ": Recovered. My decisions are now " << allDecisionsToString(decisions_) << endl;)
                                    }
                                }
                                else
                                {
                                    D(cout << "SR" << S->get_pid() << ": Received allDecisions from leader: " << msg <<  endl;)
                                    if (token.size() != 1)
                                        stringToAllDecisions(token[1], allDecs);
                                    else
                                        allDecs.clear(); //alldecs should be empty as leader sent empty as all decs
                                }
                            }
                            else if (token[0] == kReqAllDecs)
                            {
                                D(cout << "SR" << S->get_pid() << ": Request for all decisions message received: " << msg <<  endl;)
                                SendDecisionsResponse(fds[i], primary_id);
                                //ResendProposals(primary_id);
                            }
                            else {    //other messages
                                D(cout << "SR" << S->get_pid() << ": ERROR Unexpected message received: " << msg << endl;)
                            }
                        }
                    }
                }
            }
        }
    }
}

void Replica::ResendProposals(const int primary_id) {
    //
    if (S->get_pid() != primary_id)
        return;

    std::map<int, Proposal> proposals_copy = proposals_;
    for (auto &p : proposals_copy) {
        if (decisions_.find(p.first) == decisions_.end()) {
            Propose(p.second, primary_id);
        }
    }
}

vector<int> Replica::SendDecisionsRequest()
{
    vector<int> sent_to;
    string msg = kReqAllDecs + kInternalDelim + kMessageDelim;

    for (int i = 0; i < S->get_num_servers(); i++)
    {
        int send_to = get_replica_fd(i);
        if (send_to == -1)
            continue;

        // cout<<send_to<<" sending"<<endl;
        if (send(send_to, msg.c_str(), msg.size(), 0) == -1) {
            D(cout << "SR" << S->get_pid() << ": ERROR: sending allDecs request to replica R"
              << i << endl;)
            close(send_to);
            set_replica_fd(i, -1);
        }
        else {
            D(cout << "SR" << S->get_pid() << ": Message sent to replica R"
              << i << ": " << msg << endl;)
            sent_to.push_back(i);
        }
    }
    return sent_to;
}
void Replica::SendDecisionsResponse(int fd, int primary_id)
{
    string msg = kAllDecisions + kInternalDelim;
    msg += allDecisionsToString(get_decisions());
    msg += kMessageDelim;

    if (send(fd, msg.c_str(), msg.size(), 0) == -1) {
        D(cout << "SR" << S->get_pid() << ": ERROR: sending allDecs response to replica" << endl;)
        ResetFD(fd, primary_id);
    }
    else {
        D(cout << "SR" << S->get_pid() << ": AllDecs response message sent to replica " << ": " << msg << endl;)
    }

}

void Replica::MergeDecisions(map<int, Proposal> receivedAllDecisions)
{
    map<int, Proposal> local = get_decisions();
    for (auto it = receivedAllDecisions.begin(); it != receivedAllDecisions.end(); it++)
    {
        //if recvd not in mine
        if (local.find(it->first) == local.end())
        {
            local[it->first] = it->second;
        }
    }
    set_decisions(local);
}

/**
 * thread entry function for replica
 * @param  _S pointer to server class object
 * @return    NULL
 */
void* ReplicaEntry(void *_S) {
    signal(SIGPIPE, SIG_IGN);
    Replica R((Server*)_S);

    pthread_t accept_connections_thread;
    CreateThread(AcceptConnectionsReplica, (void*)&R, accept_connections_thread);

    while (1)
    {

        // sleep for some time to make sure accept threads of commanders and scouts are running
        usleep(kGeneralSleep);
        usleep(kGeneralSleep);
        int primary_id = R.S->get_primary_id();
        if (R.ConnectToCommander(primary_id)) {
            D(cout << "SR" << R.S->get_pid() << ": Connected to commander of S"
              << primary_id << endl;)
        } else {
            D(cout << "SR" << R.S->get_pid() << ": ERROR in connecting to commander of S"
              << primary_id << endl;)
            return NULL;
        }

        // if (R.ConnectToScout(primary_id)) {
        //     D(cout << "SR" << R.S->get_pid() << ": Connected to scout of S"
        //       << primary_id << endl;)
        // } else {
        //     D(cout << "SR" << R.S->get_pid() << ": ERROR in connecting to scout of S"
        //       << primary_id << endl;)
        //     return NULL;
        // }

        int upper_bound;
        if (R.S->get_mode() == RECOVER)
            upper_bound = R.S->get_num_servers();
        else
            upper_bound = R.S->get_pid();

        for (int i = 0; i < upper_bound; i++)
        {
            if (R.S->get_pid() == i)
                continue;
            if (R.get_replica_fd(i) != -1) //if not already connected
                continue;
            if (R.ConnectToReplica(i)) {
                D(cout << "SR" << R.S->get_pid() << ": Connected to replica of S"
                  << i << endl;)
            } else {
                D(cout << "SR" << R.S->get_pid() << ": ERROR in connecting to replica of S"
                  << i << endl;)
                //return NULL; removed because replica may not be there sometimes
            }
        }

        if (R.ConnectToLeader(primary_id)) {
            D(cout << "SR" << R.S->get_pid() << ": Connected to leader of S"
              << primary_id << endl;)
        } else {
            D(cout << "SR" << R.S->get_pid() << ": ERROR in connecting to leader of S"
              << primary_id << endl;)
            return NULL;
        }

        // sleep for some time to make sure all connections are established
        usleep(kGeneralSleep);
        usleep(kGeneralSleep);
        usleep(kGeneralSleep);

        // pthread_t receive_from_replicas_thread;
        // CreateThread(ReceiveMessagesFromReplicas, (void*)&R, receive_from_replicas_thread);
        // while(R.S->get_mode() != RUNNING)
        //     usleep(kRecoveryWaitSleep);

        R.S->set_replica_ready(true);
        R.ReplicaMode(primary_id);
    }
    void *status;
    pthread_join(accept_connections_thread, &status);
    return NULL;
}
