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
const string kTagDelim = "-";
const string kChat = "CHAT";

// sleep values
const time_t kGeneralSleep = 1000 * 1000;

#endif //CONSTANTS_H_
