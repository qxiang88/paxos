
void Server::Leader()
{
    leader_active_ = false;
    proposals_.clear();
    ballot_num_.id = get_pid();
    ballot_num_.seq_num = 0;

    pthread_t scout_thread;
    CreateThread(Scout, (void*)this, scout_thread);

    int num_bytes;
    int num_servers = S->get_num_servers();
    while (true) {  // always listen to messages from the acceptors
        if ((num_bytes = recv(S->get_leader_fd(), buf, kMaxDataSize - 1, 0)) == -1) 
        {
          D(cout << "Leader ERROR in receiving" << endl;)
          // pthread_exit(NULL); //TODO: think about whether it should be exit or not
        } else if (num_bytes == 0) {     //connection closed
          D(cout << "Leader connection closed by self??"<< endl;)
        } else {
            buf[num_bytes] = '\0';
            std::vector<string> message = split(string(buf), kMessageDelim[0]);
            for (const auto &msg : message) 
            {
                std::vector<string> token = split(string(msg), kInternalDelim[0]);
                if (token[0] == kPropose) 
                {
                    D(cout << "Leader receives PROPOSE" << "message"<<  endl;)
                    if(proposals_.find(token[1])==proposals_.end())
                    {
                      proposals_[token[1]]=stringToProposal(token[2]);
                      if(leader_active_)
                      {
                        pthread_t commander_thread;
                        CommanderThreadArgument* arg = new CommanderThreadArgument;
                        arg->S = this;
                        arg->toSend = Triple(ballot_num_, token[1], proposals_[token[1]]);
                        CreateThread(Commander, (void*)arg, commander_thread);
                      }
                    }
                } 
                else if(token[0] == kAdopted)
                {
                  unordered_set<Triple> pvalues = stringToTripleSet(token[2]);
                  proposals_ = pairxor(proposals_, pmax(pvalues));

                  pthread_t commander_thread[proposals_.size()];
                  int i=0;
                  for(auto it = proposals_.begin(); it!=proposals_.end(); it++)
                  {
                    CommanderThreadArgument* arg = new CommanderThreadArgument;
                    arg->S = this;
                    arg->toSend = Triple(ballot_num_, it->s, it->p);
                    CreateThread(Commander, (void*)arg, commander_thread[i]);
                    i++;
                  }
                  leader_active_ = true;
                }

                else if(token[0]==kPreEmpted)
                {
                  Ballot recvd_b = stringToBallot(token[1]);
                  if(recvd_b>ballot_num_)
                  {
                    leader_active_ = false;
                    IncrementBallotNum();
                  }
                }

                else {    //other messages
                    D(cout<<"Unexpected message in Leader "<<msg<<endl;)
                }
            }
        }
      }
    
}
//actually leader thread for each replica required. but here only one replica sends leader anything
void* LeaderThread(void* _rcv_thread_arg) {
    LeaderThreadArgument *rcv_thread_arg = (LeaderThreadArgument *)_rcv_thread_arg;
    Server *S = rcv_thread_arg->S;
    S->Leader();
    return NULL;
}