#ifndef CONSTANSTS_H_
#define CONSTANSTS_H_
#include "string"
using namespace std;

// constants for socket connections
const int kMaxDataSize = 2000 ;          // max number of bytes we can get at once
const int kBacklog = 10;                // how many pending connections queue will hold

// filenames
const string kPortsFile = "./config/ports-file";

// testfile keywords
const string kStart = "start";
const string kSendMessage = "sendMessage";
const string kCrashServer = "crashServer";
const string kRestartServer = "restartServer";
const string kAllClear = "allClear";
const string kAllClearRemove = "allClearRemove";
const string kTimeBombLeader = "timeBombLeader";
const string kPrintChatLog = "printChatLog";

// file paths
const string kServerExecutable = "./server";
const string kClientExecutable = "./client";
const string kChatLogFile = "./chatlog/log";

// message templates
const string kMessageDelim = "$";
const string kInternalDelim = "-";
const string kInternalStructDelim = ".";
const string kInternalSetDelim = ",";
const string kChat = "CHAT";
const string kChatLog = "CHATLOG";

const string kP1a = "P1A";
const string kP2a = "P2A";
const string kP1b = "P1B";
const string kP2b = "P2B";
const string kDecision = "DECISION";
const string kAllDecisions = "ALLDECISIONS";
const string kPreEmpted = "PREEMPTED";
const string kAdopted = "ADOPTED";
const string kPropose = "PROPOSE";
const string kResponse = "RESPONSE";

const string kLeaderRole = "LEADER";
const string kReplicaRole = "REPLICA";

const string kAllClearSet = "SET";
const string kAllClearNotSet = "NOTSET";
const string kAllClearDone = "DONE";

// sleep values
const time_t kGeneralSleep = 1000 * 1000;
const time_t kBusyWaitSleep = 500 * 1000;
const time_t kAllClearSleep = 500 * 1000;

// timeout values
const timeval kReceiveTimeoutTimeval = {
    0, // tv_sec
    500 * 1000 //tv_usec (microsec)
};

const timeval kSelectTimeoutTimeval = {
    0, // tv_sec
    500 * 1000 //tv_usec (microsec)
};

#endif //CONSTANTS_H_
