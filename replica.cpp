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

template <typename Map>
bool map_compare (Map const &lhs, Map const &rhs) {
    // No predicate needed because there is operator== for pairs already.
	return lhs.size() == rhs.size()
	&& std::equal(lhs.begin(), lhs.end(),
		rhs.begin());
}

#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG
#

Replica::Replica(Server* _S) {
	S = _S;
	set_slot_num(0);

	int num_servers = S->get_num_servers();
	int num_clients = S->get_num_clients();

	commander_fd_.resize(num_servers, -1);
	scout_fd_.resize(num_servers, -1);
	leader_fd_.resize(num_servers, -1);
	client_chat_fd_.resize(num_clients, -1);
}

int Replica::get_commander_fd(const int server_id) {
	return commander_fd_[server_id];
}

int Replica::get_scout_fd(const int server_id) {
	return scout_fd_[server_id];
}

int Replica::get_leader_fd(const int server_id) {
	return leader_fd_[server_id];
}

int Replica::get_client_chat_fd(const int client_id) {
	return client_chat_fd_[client_id];
}

int Replica::get_slot_num() {
	return slot_num_;
}

void Replica::set_commander_fd(const int server_id, const int fd) {
	commander_fd_[server_id] = fd;
}

void Replica::set_scout_fd(const int server_id, const int fd) {
	scout_fd_[server_id] = fd;
}

void Replica::set_leader_fd(const int server_id, const int fd) {
	leader_fd_[server_id] = fd;
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

void Replica::Unicast(const string &type, const string& msg)
{
	if (send(get_leader_fd(S->get_primary_id()), msg.c_str(), msg.size(), 0) == -1) {
		D(cout << "SR" << S->get_pid() << ": ERROR in sending " << type << endl;)
	}
	else {
        D(cout << "SR" << S->get_pid() << ": " << type << " message sent: " << msg << endl;)
    }
}

/**
 * proposes the proposal to a leader
 * @param p Proposal to be proposed
 */
 void Replica::Propose(const Proposal &p) {
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
 	SendProposal(min_slot, p);
 }

/**
 * sends proposal to a leader
 * @param s proposed slot num for this proposal
 * @param p proposal to be proposed
 */
 void Replica::SendProposal(const int& s, const Proposal& p)
 {
 	string msg = kPropose + kInternalDelim;
 	msg += to_string(s) + kInternalDelim;
 	msg += proposalToString(p) + kMessageDelim;
 	Unicast(kPropose, msg);
 }

/**
 * performs the decision reached by Paxos by adding it to decisions,
 * incrementing slot num, followed by sending decision to client
 * @param slot decided slot num
 * @param p    decided proposal
 */
 void Replica::Perform(const int& slot, const Proposal& p)
 {
 	for (auto it = decisions_.begin(); it != decisions_.end(); it++)
 	{
 		if (it->second == p && it->first < get_slot_num())
        {   //think why not increase in loop
        	IncrementSlotNum();
        	return;
        }
    }

    IncrementSlotNum();
    SendResponseToAllClients(slot, p);
}

/**
 * sends the decided proposal to all clients
 * @param s decided slot num
 * @param p proposal to be sent
 */
 void Replica::SendResponseToAllClients(const int& s, const Proposal& p)
 {
 	if(S->get_pid()!=S->get_primary_id())
 		return;

 	string msg = kResponse + kInternalDelim;
 	msg += to_string(s) + kInternalDelim;
 	msg += proposalToString(p) + kMessageDelim;

 	for (int i = 0; i < S->get_num_clients(); ++i) {
 		if (send(get_client_chat_fd(i), msg.c_str(), msg.size(), 0) == -1) {
 			D(cout << "SR" << S->get_pid() << ": ERROR: sending response to client C"
 				<< i << endl;)
 		}
 		else {
            D(cout << "SR" << S->get_pid() << ": Message sent to client C"
              << i << ": " << msg << endl;)
        }
    }
}

void Replica::ProposeBuffered()
{
  for(auto pit = buffered_proposals_.begin(); pit!=buffered_proposals_.end(); pit++)
  {
     Propose(*pit);
 }
 buffered_proposals_.clear();
}

void Replica::CheckReceivedAllDecisions(map<int, Proposal>& allDecisions)
{    
  if (map_compare (allDecisions,decisions_))
  {
 		// D(cout<<"SR"<<S->get_pid()<<": Has received every decision in all decisions("<<allDecisions.size()<<")"<<endl;)
     S->set_all_clear(kReplicaRole, kAllClearDone);
     allDecisions.clear();
     allDecisions[-1] = Proposal("","","");
 }
 else
 {
                // D(cout<<"SR"<<S->get_pid()<<": Has not received every decision"<<endl;)
 }
}

void Replica::CreateFdSet(fd_set& fromset, vector<int> &fds, int& fd_max)
{
  fd_max = INT_MIN;
  int fd_temp;
  FD_ZERO(&fromset);
  fds.clear();
  for (int i = 0; i < S->get_num_clients(); i++)
  {
     fd_temp = get_client_chat_fd(i);
     if(fd_temp == -1)
        continue;

    fd_max = max(fd_max, fd_temp);
    fds.push_back(fd_temp);
    FD_SET(fd_temp, &fromset);
}

for (int i = 0; i < S->get_num_servers(); i++)
{
 fd_temp = get_commander_fd(i);
 if(fd_temp == -1)
    continue;

fd_max = max(fd_max, fd_temp);
fds.push_back(fd_temp);
FD_SET(fd_temp, &fromset);
}

        //TODO: Use local primary_id from calling function or get_primary_id()?
fd_temp = get_leader_fd(S->get_primary_id());
fd_max = max(fd_max, fd_temp);
fds.push_back(fd_temp);
FD_SET(fd_temp, &fromset);

}


/**
 * function for performing replica related job
 */
 void Replica::ReplicaMode()
 {




 	char buf[kMaxDataSize];
 	int num_bytes;

 	fd_set fromset;
 	vector<int> fds;
 	int fd_max;

 	map<int, Proposal> allDecs;
 	allDecs[-1] = Proposal("","","");

    while (true) {  // always listen to messages from the acceptors

    	CreateFdSet(fromset, fds, fd_max);
        //if just become not set, then propose buffered
    	if((S->get_all_clear(kReplicaRole)==kAllClearNotSet)&&(!buffered_proposals_.empty()))
    		ProposeBuffered();

    	if((S->get_all_clear(kReplicaRole)==kAllClearSet)&&(allDecs.find(-1)==allDecs.end()) )
    	{
    		CheckReceivedAllDecisions(allDecs);
    	}

    	int rv = select(fd_max + 1, &fromset, NULL, NULL, (timeval*)&kSelectTimeoutTimeval);
        if (rv == -1) { //error in select
        	D(cout << "SR" << S->get_pid() << ": ERROR in select()" << endl;)
        } else if (rv == 0) {
            // D(cout << "SR" << S->get_pid() << ": ERROR Unexpected select timeout" << endl;)
        } else {
        	for (int i = 0; i < fds.size(); i++) {
                if (FD_ISSET(fds[i], &fromset)) { // we got one!!
                	char buf[kMaxDataSize];
                	if ((num_bytes = recv(fds[i], buf, kMaxDataSize - 1, 0)) == -1) {
                		D(cout << "SR" << S->get_pid()
                			<< ": ERROR in receiving from commander or clients" << endl;)
                    } else if (num_bytes == 0) {     //connection closed
                    	D(cout << "SR" << S->get_pid() << ": Connection closed by commander or client" << endl;)
                    } else {
                    	buf[num_bytes] = '\0';
                    	std::vector<string> message = split(string(buf), kMessageDelim[0]);
                    	for (const auto &msg : message) {
                    		std::vector<string> token = split(string(msg), kInternalDelim[0]);
                    		if (token[0] == kChat)
                    		{
                    			D(cout << "SR" << S->get_pid() << ": Received chat from client: " << msg <<  endl;)
                    			Proposal p = stringToProposal(token[1]);
                    			if(S->get_all_clear(kReplicaRole)!=kAllClearNotSet)
                    			{
                    				D(cout<<"SR"<<S->get_pid()<<": Buffering propose - "<<token[1]<<endl;)
                    				buffered_proposals_.push_back(p);
                    			}
                    			else
                    			{
                    				ProposeBuffered();
                    				Propose(p);
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
                    						Propose(proposals_[slot_num]);
                    				}
                    				Perform(slot_num, currdecision);
                    				slot_num = get_slot_num();
                                    //s has to slot_num. check if it is slotnum in recovery too.
                                    //if so can remove argument from perform, sendresponse functions
                    			}

                                if(allDecs.find(-1)==allDecs.end()) //means allDecs has been received
                                {
                                	CheckReceivedAllDecisions(allDecs);
                                }
                            }
                            else if(token[0]==kAllDecisions)
                            {
                            	D(cout << "SR" << S->get_pid() << ": Received allDecisions from leader: " << msg <<  endl;)
                            	if(token.size()!=1)
                            		stringToAllDecisions(token[1], allDecs);
                            	else
                                    allDecs.clear(); //alldecs should be empty as leader sent empty as all decs
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

/**
 * thread entry function for replica
 * @param  _S pointer to server class object
 * @return    NULL
 */
 void* ReplicaEntry(void *_S) {
 	Replica R((Server*)_S);

 	pthread_t accept_connections_thread;
 	CreateThread(AcceptConnectionsReplica, (void*)&R, accept_connections_thread);

    // sleep for some time to make sure accept threads of commanders and scouts are running
 	usleep(kGeneralSleep);
 	usleep(kGeneralSleep);
 	int primary_id = R.S->get_primary_id();
 	if (R.ConnectToCommander(primary_id)) {
        // D(cout << "SR" << R.S->get_pid() << ": Connected to commander of S"
        //   << primary_id << endl;)
 	} else {
 		D(cout << "SR" << R.S->get_pid() << ": ERROR in connecting to commander of S"
 			<< primary_id << endl;)
 		return NULL;
 	}

 	if (R.ConnectToScout(primary_id)) {
        // D(cout << "SR" << R.S->get_pid() << ": Connected to scout of S"
          // << primary_id << endl;)
 	} else {
 		D(cout << "SR" << R.S->get_pid() << ": ERROR in connecting to scout of S"
 			<< primary_id << endl;)
 		return NULL;
 	}

    // sleep for some time to make sure all connections are established
 	usleep(kGeneralSleep);
 	usleep(kGeneralSleep);
 	usleep(kGeneralSleep);

 	R.ReplicaMode();

 	void *status;
 	pthread_join(accept_connections_thread, &status);
 	return NULL;

 }