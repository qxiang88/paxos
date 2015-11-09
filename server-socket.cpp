#include "server.h"
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

#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

extern void* ReceiveMessagesFromMaster(void* _S );

/**
 * returns port number
 * @param  sa sockaddr structure
 * @return    port number contained in sa
 */
 int return_port_no(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return (((struct sockaddr_in*)sa)->sin_port);
    }

    return (((struct sockaddr_in6*)sa)->sin6_port);
}

void sigchld_handler(int s) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}


/**
 * function for server's accept connections thread
 * @param _S Pointer to server class object
 */
 void* AcceptConnectionsServer(void* _S) {
    Server *S = (Server *)_S;

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
    if ((rv = getaddrinfo(NULL, std::to_string(S->get_server_listen_port(S->get_pid())).c_str(),
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

        if (incoming_port == S->get_master_port()) { // incoming connection from master_port
            S->set_master_fd(new_fd);

            pthread_t receive_from_master_thread;
            CreateThread(ReceiveMessagesFromMaster, (void*)S, receive_from_master_thread);

        } else {
            D(cout << "S" << S->get_pid() << ": ERROR: Unexpected connect request from port "
              << incoming_port << endl;)
        }
    }
    pthread_exit(NULL);
}

