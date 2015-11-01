#ifndef UTILITIES_H_
#define UTILITIES_H_

#include "string"
#include "vector"
#include "sstream"
#include "iostream"
#include "constants.h"
#include "unordered_set"

using namespace std;

#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG

struct Proposal;
struct Ballot;
struct Triple;

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
std::vector<std::string> split(const std::string &s, char delim);

string tripleToString(const Triple& t);
string proposalToString(const Proposal& p);
string ballotToString(const Ballot& b);
Triple stringToTriple(const string& s);
unordered_set<Triple> stringToTripleSet(const string& s);
string tripleSetToString(const unordered_set<Triple>& st);
Ballot stringToBallot(const string& s);

void union_set(unordered_set<Triple>& s1, unordered_set<Triple>&s2);

void CreateThread(void* (*f)(void* ), void* arg, pthread_t &thread);


struct Proposal {
    string client_id;
    string chat_id;
    string msg;

    Proposal() { }
    Proposal(const string &client_id,
            const string &chat_id,
            const string &msg)
        : client_id(client_id),
          chat_id(chat_id),
          msg(msg) { }

    bool operator==(const Proposal &p2) const;
};

struct Ballot {
    int id;
    int seq_num;

    Ballot() { }
    Ballot(int i, int s): id(i), seq_num(s) {}

    bool operator>(const Ballot &b2) const;
    bool operator<(const Ballot &b2) const;
    bool operator==(const Ballot &b2) const;
    bool operator>=(const Ballot &b2) const;
    bool operator<=(const Ballot &b2) const;

};

struct Triple {
    Ballot b;
    int s;
    Proposal p;

    bool operator==(const Triple &t2) const;
};

template <class T>
inline void hash_combine(std::size_t & seed, const T & v)
{
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std
{
  template <> struct hash<Ballot>
  {
    inline size_t operator()(const Ballot & b) const
    {
      size_t seed = 0;
      hash_combine(seed, b.id);
      hash_combine(seed, b.seq_num);
      return seed;
    }
  };

  template <> struct hash<Proposal>
  {
    inline size_t operator()(const Proposal & b) const
    {
      size_t seed = 0;
      hash_combine(seed, b.client_id);
      hash_combine(seed, b.chat_id);
      hash_combine(seed, b.msg);
      return seed;
    }
  };

  template <> struct hash<Triple>
  {
    inline size_t operator()(const Triple & t) const
    {
      size_t seed = 0;
      hash_combine(seed, t.b);
      hash_combine(seed, t.s);
      hash_combine(seed, t.p);
      return seed;
    }
  };
}




#endif