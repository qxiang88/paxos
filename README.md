Paxos
==============
### Description:
This program is a (rather unoptimized) implementation of the Paxos algorithm for achieving consensus in an asynchronous environment with crash failures. The implementation is based on the model discussed in [Paxos Made Moderately Complex](https://people.csail.mit.edu/matei/courses/2015/6.S897/readings/paxos-moderately-complex.pdf). The protocol has been implemented in a chat-room setting, where there are multiple clients sending messages, and multiple servers (which are replicas of each other) running consensus on the order of the messages to be fixed upon in the chat-room, sending the decided order to the clients so that every client has the same view of the chatlog. The system can tolerate the failure of *f* servers. A failed server can recover and fail multiple times.

In this particular implementation, every server can perform the role of replica, leader, commander, scout, and acceptor. At any given instant, there is only one leader in the system. Leader election is done by the Master in a round-robin fashion. Master program is external to the chat-room environment, and only functions as an medium of interaction between the clients/servers and the test files, and as a controller of the order/sequence of instructions, such as **restartServer**, **crashServer** etc. to be executed. If there are *n* servers, then *n = 2f + 1*, i.e., a majority of servers must always be alive to guarantee progress. In case of the failure of a majority of servers, progress cannot be guaranteed; however, correctness will never be violated. We assume that at any instant, there is at least one server which has the log of all the decisions made upto that point, so that any recovering server can query this server and get back on track without any explicit logging requirements.

### Build instructions:
```cd``` to the project directory, and type ```make```. It should create 3 executables named ``master, client, server``. Type the following command to delete all executables and logfiles
```sh
make clean
``` 
Create a folder named ``chatlog`` in the project directory where the chatlog file of each client will be created during execution. 

### Sample Tests:
Sample test cases are included in `tests` folder. The first line in each test consists of `start s c` where *s* is the number of servers and *c* is the number of clients. The number of ports given in `config/ports-file` must correspond to the *s* and *c* values specified in tests file as mentioned in the Config section below. To run `test5`, type `./master <test5`. The description of commands that can be specified in the tests are discussed in the `handout.pdf`

### Client Chatlogs:
Clients' chatlogs are dumped in `chatlogs` folder. The logs of only those clients will be dumped in the `chatlog/log*` files for whom `printChatLog client_id` instruction was given in the test file. Expected output for the sample test cases are provided in `archive` folder. Each file in the `archive` folder represents a client's view of the chatlog for the corresponding test case in `tests` folder.
### Config:
If the test file has *s* servers and *c* clients, then you need to provide appropriate number of ports in the file `config/ports-file`. See `config/ports-description` for a description of the number/type of ports required. The `config/ports-file` should have *(2*c + 9*s + 1)* unique (free) ports, each on a separate line. The file should not have a spare new-line at the end. We have provided two example port files:
1. `config/ports-file3` for the case when *s = c = 3*
2. `config/ports-file5` for the case when *s = c = 5*

### Running instructions:
Type `./master` to run the program

### Debugging:
Printing of debug statements can be turned off for each `.cpp` file by commenting the `#define DEBUG` statement at the beginning of that file.
### Note:
1. Test file **must not** have a new line at the end
2. Some long tests might take quite a lot of time to finish (~100 sec), especially those that have multiple crashes and restarts, because every restart operation requires 6-7 seconds for a server to start.
