#include "limits.h"


void Server::Acceptor()
{
    
}
//actually leader thread for each replica required. but here only one replica sends leader anything
void* AcceptorThread(void* _rcv_thread_arg) {
    AcceptorThreadArgument *rcv_thread_arg = (AcceptorThreadArgument *)_rcv_thread_arg;
    Server *S = rcv_thread_arg->S;
    S->Acceptor();
    return NULL;
}