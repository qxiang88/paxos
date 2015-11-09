#include "server.h"
#include "replica.h"
#include "constants.h"
#include "iostream"
#include "unistd.h"
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
using namespace std;

// #define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

extern int return_port_no(struct sockaddr *sa);
extern void sigchld_handler(int s);

/**
 * function for replica's accept connections thread
 * @param _S Pointer to server class object
 */
void* AcceptConnectionsReplica(void* _R) {
    Replica *R = (Replica*)_R;

    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *l;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, std::to_string(R->S->get_replica_listen_port(R->S->get_pid())).c_str(),
                          &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit (1);
    }

    // loop through all the results and bind to the first we can
    for (l = servinfo; l != NULL; l = l->ai_next) {
        if ((sockfd = socket(l->ai_family, l->ai_socktype,
                             l->ai_protocol)) == -1) {
            perror("server: socket ERROR");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt ERROR");
            exit(1);
        }

        if (bind(sockfd, l->ai_addr, l->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind ERROR");
            continue;
        }

        break;
    }
    freeaddrinfo(servinfo); // all done with this structure

    if (l == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    // S->set_server_sockfd(sockfd);

    if (listen(sockfd, kBacklog) == -1) {
        perror("listen ERROR");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    while (1) {
        // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept ERROR");
            continue;
        }

        int incoming_port = ntohs(return_port_no((struct sockaddr *)&their_addr));

        int process_id = R->S->IsClientChatPort(incoming_port);
        if (process_id != -1) { //incoming connection from chat port of a client
            R->set_client_chat_fd(process_id, new_fd);
        } else {
            process_id = R->S->IsReplicaPort(incoming_port);
            if (process_id != -1) { //incoming connection from chat port of a client
                R->set_replica_fd(process_id, new_fd);
            }
            else {
                D(cout << "SR" << R->S->get_pid() << ": ERROR: Unexpected connect request from port "
                  << incoming_port << endl;)
            }
        }
    }
    pthread_exit(NULL);
}

/**
 * Connects to a commander port
 * @param server_id id of server whose commander to connect to
 * @return  true if connection was successfull or already connected
 */
bool Replica::ConnectToCommander(const int server_id) {
    // if (get_commander_fd(server_id) != -1) return true;

    int sockfd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *clientinfo, *l;
    struct sigaction sa;
    int yes = 1;
    int rv;

    // set up addrinfo for i.e. self
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, std::to_string(S->get_replica_port(S->get_pid())).c_str(),
                          &hints, &clientinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit (1);
    }

    // loop through all the results and bind to the first we can
    for (l = clientinfo; l != NULL; l = l->ai_next)
    {
        if ((sockfd = socket(l->ai_family, l->ai_socktype,
                             l->ai_protocol)) == -1) {
            perror("client: socket ERROR");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt ERROR");
            exit(1);
        }

        if (bind(sockfd, l->ai_addr, l->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: bind ERROR");
            continue;
        }

        break;
    }
    freeaddrinfo(clientinfo); // all done with this structure
    if (l == NULL)  {
        fprintf(stderr, "client: failed to bind\n");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }


    // set up addrinfo for server
    int numbytes;
    struct addrinfo *servinfo;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, std::to_string(S->get_commander_listen_port(server_id)).c_str(),
                          &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return false;
    }
    // loop through all the results and connect to the first we can
    for (l = servinfo; l != NULL; l = l->ai_next)
    {
        errno = 0;
        if (connect(sockfd, l->ai_addr, l->ai_addrlen) == -1) {
            close(sockfd);
            // if (errno == EBADF) cout << errno << endl;
            continue;
        }

        break;
    }
    if (l == NULL) {
        D(cout << S->get_pid() << "HH" << endl;)
        return false;
    }
    // int outgoing_port = ntohs(return_port_no((struct sockaddr *)l->ai_addr));
    freeaddrinfo(servinfo); // all done with this structure
    set_commander_fd(server_id, sockfd);
    return true;
}

/**
 * Connects to a scout port
 * @param server_id id of server whose scout to connect to
 * @return  true if connection was successfull or already connected
 */
// bool Replica::ConnectToScout(const int server_id) {
//     // if (get_scout_fd(server_id) != -1) return true;

//     int sockfd;  // listen on sock_fd, new connection on new_fd
//     struct addrinfo hints, *clientinfo, *l;
//     struct sigaction sa;
//     int yes = 1;
//     int rv;

//     // set up addrinfo for i.e. self
//     memset(&hints, 0, sizeof hints);
//     hints.ai_family = AF_UNSPEC;
//     hints.ai_socktype = SOCK_STREAM;
//     hints.ai_flags = AI_PASSIVE; // use my IP
//     if ((rv = getaddrinfo(NULL, std::to_string(S->get_replica_port(S->get_pid())).c_str(),
//                           &hints, &clientinfo)) != 0) {
//         fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
//         exit (1);
//     }

//     // loop through all the results and bind to the first we can
//     for (l = clientinfo; l != NULL; l = l->ai_next)
//     {
//         if ((sockfd = socket(l->ai_family, l->ai_socktype,
//                              l->ai_protocol)) == -1) {
//             perror("client: socket ERROR");
//             continue;
//         }

//         if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
//                        sizeof(int)) == -1) {
//             perror("setsockopt ERROR");
//             exit(1);
//         }

//         if (bind(sockfd, l->ai_addr, l->ai_addrlen) == -1) {
//             close(sockfd);
//             perror("client: bind ERROR");
//             continue;
//         }

//         break;
//     }
//     freeaddrinfo(clientinfo); // all done with this structure
//     if (l == NULL)  {
//         fprintf(stderr, "client: failed to bind\n");
//         exit(1);
//     }

//     sa.sa_handler = sigchld_handler; // reap all dead processes
//     sigemptyset(&sa.sa_mask);
//     sa.sa_flags = SA_RESTART;
//     if (sigaction(SIGCHLD, &sa, NULL) == -1) {
//         perror("sigaction");
//         exit(1);
//     }


//     // set up addrinfo for server
//     int numbytes;
//     struct addrinfo *servinfo;
//     char s[INET6_ADDRSTRLEN];

//     memset(&hints, 0, sizeof hints);
//     hints.ai_family = AF_UNSPEC;
//     hints.ai_socktype = SOCK_STREAM;
//     hints.ai_flags = AI_PASSIVE; // use my IP
//     if ((rv = getaddrinfo(NULL, std::to_string(S->get_scout_listen_port(server_id)).c_str(),
//                           &hints, &servinfo)) != 0) {
//         fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
//         return false;
//     }
//     // loop through all the results and connect to the first we can
//     for (l = servinfo; l != NULL; l = l->ai_next)
//     {
//         errno = 0;
//         if (connect(sockfd, l->ai_addr, l->ai_addrlen) == -1) {
//             close(sockfd);
//             // if (errno == EBADF) cout << errno << endl;
//             continue;
//         }

//         break;
//     }
//     if (l == NULL) {
//         return false;
//     }
//     // int outgoing_port = ntohs(return_port_no((struct sockaddr *)l->ai_addr));
//     freeaddrinfo(servinfo); // all done with this structure
//     set_scout_fd(server_id, sockfd);
//     return true;
// }

/**
 * Connects to a replica port
 * @param server_id id of server whose replica to connect to
 * @return  true if connection was successfull or already connected
 */
bool Replica::ConnectToReplica(const int server_id) {
    // if (get_commander_fd(server_id) != -1) return true;

    int sockfd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *clientinfo, *l;
    struct sigaction sa;
    int yes = 1;
    int rv;

    // set up addrinfo for i.e. self
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, std::to_string(S->get_replica_port(S->get_pid())).c_str(),
                          &hints, &clientinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit (1);
    }

    // loop through all the results and bind to the first we can
    for (l = clientinfo; l != NULL; l = l->ai_next)
    {
        if ((sockfd = socket(l->ai_family, l->ai_socktype,
                             l->ai_protocol)) == -1) {
            perror("client: socket ERROR");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt ERROR");
            exit(1);
        }

        if (bind(sockfd, l->ai_addr, l->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: bind ERROR");
            continue;
        }

        break;
    }
    freeaddrinfo(clientinfo); // all done with this structure
    if (l == NULL)  {
        fprintf(stderr, "client: failed to bind\n");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }


    // set up addrinfo for server
    int numbytes;
    struct addrinfo *servinfo;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, std::to_string(S->get_replica_listen_port(server_id)).c_str(),
                          &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return false;
    }
    // loop through all the results and connect to the first we can
    for (l = servinfo; l != NULL; l = l->ai_next)
    {
        errno = 0;
        if (connect(sockfd, l->ai_addr, l->ai_addrlen) == -1) {
            close(sockfd);
            // if (errno == EBADF) cout << errno << endl;
            continue;
        }

        break;
    }
    if (l == NULL) {
        return false;
    }
    // int outgoing_port = ntohs(return_port_no((struct sockaddr *)l->ai_addr));
    freeaddrinfo(servinfo); // all done with this structure
    set_replica_fd(server_id, sockfd);
    return true;
}

/**
 * Connects to a leader port
 * @param server_id id of server whose leader to connect to
 * @return  true if connection was successfull or already connected
 */
bool Replica::ConnectToLeader(const int server_id) {

    int sockfd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *clientinfo, *l;
    struct sigaction sa;
    int yes = 1;
    int rv;

    // set up addrinfo for i.e. self
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, std::to_string(S->get_replica_port(S->get_pid())).c_str(),
                          &hints, &clientinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit (1);
    }

    // loop through all the results and bind to the first we can
    for (l = clientinfo; l != NULL; l = l->ai_next)
    {
        if ((sockfd = socket(l->ai_family, l->ai_socktype,
                             l->ai_protocol)) == -1) {
            perror("client: socket ERROR");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt ERROR");
            exit(1);
        }

        if (bind(sockfd, l->ai_addr, l->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: bind ERROR");
            continue;
        }

        break;
    }
    freeaddrinfo(clientinfo); // all done with this structure
    if (l == NULL)  {
        fprintf(stderr, "client: failed to bind\n");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }


    // set up addrinfo for server
    int numbytes;
    struct addrinfo *servinfo;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, std::to_string(S->get_leader_listen_port(server_id)).c_str(),
                          &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return false;
    }
    // loop through all the results and connect to the first we can
    for (l = servinfo; l != NULL; l = l->ai_next)
    {
        errno = 0;
        if (connect(sockfd, l->ai_addr, l->ai_addrlen) == -1) {
            close(sockfd);
            // if (errno == EBADF) cout << errno << endl;
            continue;
        }

        break;
    }
    if (l == NULL) {
        D(cout << S->get_pid() << "HH" << endl;)
        return false;
    }
    // int outgoing_port = ntohs(return_port_no((struct sockaddr *)l->ai_addr));
    freeaddrinfo(servinfo); // all done with this structure
    set_leader_fd(server_id, sockfd);
    return true;
}
