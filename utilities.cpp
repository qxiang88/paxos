

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

string tripleToString(const Triple& t)
{
    string s = to_string(t.b.id);
    s+=kInternalStructDelim;
    s += to_string(t.b.seq_num);
    s+=kInternalStructDelim;
    s += to_string(t.s);
    s+=kInternalStructDelim;
    s += to_string(t.p.client_id);
    s+=kInternalStructDelim;
    s += to_string(t.p.chat_id);
    s+=kInternalStructDelim;
    s += msg;

    return s;
}


string proposalToString(const Proposal& p)
{
    string s = to_string(t.p.client_id);
    s+=kInternalStructDelim;
    s += to_string(t.p.chat_id);
    return s;
}

string ballotToString(const Ballot& b)
{
    string s = to_string(b.id);
    s+=kInternalStructDelim;
    s += to_string(b.seq_num);
    return s;
}

Triple stringToTriple(const string& s)
{
    Triple t;
    Proposal p;
    Ballot b;
    vector<string> parts = split(s, kInternalStructDelim);
    if(parts.size()!=6)D(cout<<"Error not proper triple. Size is "<<parts.size())<<endl;)
    b.id = stoi(parts[0]);
    b.seq_num = stoi(parts[1]);
    t.b = stoi(b);
    t.s = stoi(parts[2]);
    p.client_id = stoi(parts[3]);
    p.chat_id = stoi(parts[4]);
    p.msg = parts[5];
    t.p = p;
    return t;
}

unordered_set<Triple> stringToTripleSet(const string& s)
{
    unordered_set<Triple> st;
    vector<string> parts = split(s, kInternalSetDelim);
    for (auto it=parts.begin(); it!=parts.end(); it++)
    {
        Triple t = stringToTriple(*it);
        st.insert(t);
    }
}

string tripleSetToString(const unordered_set<Triple>& st)
{
   string rval;
    for (auto it=st.begin(); it!=st.end(); it++)
    {
        if(it!=st.begin())
            rval += kInternalSetDelim;
        rval += tripleToString(*it);
    }
    return rval;
}

Ballot stringToBallot(const string& s)
{
    Ballot b;
    vector<string> parts = split(s, kInternalStructDelim);
    b.id = stoi(parts[0]);
    b.seq_num = stoi(parts[1]);
    return b;
}

void union_set(unordered_set<Triple>& s1, unordered_set<Triple>&s2)
{
    unordered_set <Triple> un; 
    for(auto it=s2.begin(); it!=s2.end(); it++)
    {
        if(s1.find(*it)==s1.end())
            s1.insert(*it);
    }
}

/**
 * creates a new thread
 * @param  f function pointer to be passed to the new thread
 * @param  arg arguments for the function f passed to the thread
 * @param  thread thread identifier for new thread
 */
void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread) {
    if (pthread_create(&thread, NULL, f, arg)) {
        D(cout << "U " << ": ERROR: Unable to create thread" << endl;)
        pthread_exit(NULL);
    }
}