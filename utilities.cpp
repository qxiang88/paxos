#include "utilities.h"

#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG


bool Proposal::operator==(const Proposal &p2) const {
    return ((this->client_id == p2.client_id) && (this->chat_id == p2.chat_id) && (this->msg == p2.msg));
}

bool Triple::operator==(const Triple &t2) const {
    return ((this->b == t2.b) && (this->s == t2.s) && (this->p == t2.p));
}

bool Ballot::operator>(const Ballot &b2) const {
    if (this->seq_num > b2.seq_num)
        return true;
    else if (this->seq_num < b2.seq_num)
        return false;
    else if (this->seq_num == b2.seq_num) {
        if (this->id > b2.id)
            return true;
        else
            return false;
    }
}

bool Ballot::operator<(const Ballot &b2) const {
    return !((*this) >= b2);
}

bool Ballot::operator==(const Ballot &b2) const {
    return ((this->seq_num == b2.seq_num) && (this->id == b2.id));
}

bool Ballot::operator>=(const Ballot &b2) const {
    return ((*this) == b2 || (*this) > b2);
}

bool Ballot::operator<=(const Ballot &b2) const {
    return !((*this) > b2);
}

// bool Ballot::operator()(const Ballot &b) const {
//     return ((hash<int>() (b.id)
//             ^ ( hash<int>() (b.seq_num) <<1 )));
                                
// }

// bool Proposal::operator()(const Proposal &p) const {
//     return ((hash<string>()(p.client_id)
//                ^ (hash<string>()(p.chat_id) << 1)) >> 1)
//                ^ (hash<string>()(p.msg) << 1);
// }

// bool Triple::operator()(const Triple &t) const{
//     return ((hash<Ballot>()(t.b)
//                ^ (hash<int>()(t.s) << 1)) >> 1)
//                ^ (hash<Proposal>()(t.p) << 1);
// }


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
    if(s=="")
        return elems;
    
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
    s += t.p.client_id;
    s+=kInternalStructDelim;
    s += t.p.chat_id;
    s+=kInternalStructDelim;
    s += t.p.msg;

    return s;
}


void union_set(unordered_set<Triple>& s1, unordered_set<Triple>&s2)
{
    unordered_set <Triple> un; 
    unordered_set<Triple>::iterator got;
    for(auto it=s2.begin(); it!=s2.end(); it++)
    {
        got = s1.find(*it);
        if(got==s1.end())
            s1.insert(*it);
    }
}


string proposalToString(const Proposal& p)
{
    string s = p.client_id;
    s+=kInternalStructDelim;
    s += p.chat_id;
    s+=kInternalStructDelim;
    s += p.msg;
    return s;
}

string ballotToString(const Ballot& b)
{
    string s = to_string(b.id);
    s+=kInternalStructDelim;
    s += to_string(b.seq_num);
    return s;
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


Triple stringToTriple(const string& s)
{
    Triple t;
    Proposal p;
    Ballot b;
    vector<string> parts = split(s, kInternalStructDelim[0]);
    if(parts.size()==6)
    {
        b.id = stoi(parts[0]);
        b.seq_num = stoi(parts[1]);
        t.b = b;
        t.s = stoi(parts[2]);
        p.client_id = parts[3];
        p.chat_id = parts[4];
        p.msg = parts[5];
        t.p = p;
    }
    return t;
}

unordered_set<Triple> stringToTripleSet(const string& s)
{
    unordered_set<Triple> st;
    vector<string> parts = split(s, kInternalSetDelim[0]);
    for (auto it=parts.begin(); it!=parts.end(); it++)
    {
        Triple t = stringToTriple(*it);
        st.insert(t);
    }
}


Ballot stringToBallot(const string& s)
{
    Ballot b;
    vector<string> parts = split(s, kInternalStructDelim[0]);
    b.id = stoi(parts[0]);
    b.seq_num = stoi(parts[1]);
    return b;
}

Proposal stringToProposal(const string& s)
{
    Proposal p;
    vector<string> parts = split(s, kInternalStructDelim[0]);
    p.client_id = parts[0];
    p.chat_id = parts[1];
    p.msg = parts[2];
    return p;
}

map<int, Proposal> pmax(const unordered_set<Triple> &pvalues)
{
    map<int, Proposal> rval;
    // set<int> slot_added;
    for(auto it=pvalues.begin(); it!=pvalues.end(); it++)
    {
        if(rval.find(it->s)==rval.end())
        {
            Ballot b_max = it->b;
            Proposal p_max = it->p;
            for(auto it2 = pvalues.begin(); it2!=pvalues.end(); it2++)
            {
                if(it2->s != it->s)continue;

                if(it2->b > b_max)
                {
                    b_max = it2->b;
                    p_max = it2->p;
                }
            }
            rval[it->s] = p_max;
        }
    }
    return rval;
}

map<int, Proposal> pairxor(const map<int, Proposal> &x,const map<int, Proposal> &y)
{
    map<int, Proposal> rval = y; 
    for(auto itx = x.begin(); itx!=x.end(); itx++)
    {
        auto it = y.find(itx->first);
        if(it==y.end())
        {
            rval[itx->first] = itx->second;
        }
    }
    return rval;
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