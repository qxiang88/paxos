#ifndef CONSTANSTS_H_
#define CONSTANSTS_H_
#include "string"
using namespace std;

// constants for socket connections
const int kMaxDataSize = 200 ;          // max number of bytes we can get at once
const int kBacklog = 10;                // how many pending connections queue will hold

// filenames
const string kPortsFile = "./config/ports-file";

// testfile keywords
const string kStart = "start";
const string kSendMessage = "sendMessage";
const string kCrashServer = "crashServer";
const string kRestartServer = "restartServer";
const string kAllClear = "allClear";
const string kTimeBombLeader = "timeBombLeader";
const string kPrintChatLog = "printChatLog";

// cpp file paths
const string kServerExecutable = "./server";
const string kClientExecutable = "./client";

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
const string kPreEmpted = "PREEMPTED";
const string kAdopted = "ADOPTED";

// sleep values
const time_t kGeneralSleep = 1000 * 1000;

// timeout values
const timeval kReceiveTimeoutTimeval = {
    0, // tv_sec
    500 * 1000 //tv_usec (microsec)
};

#endif //CONSTANTS_H_
