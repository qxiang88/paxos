#ifndef UTILITIES_H_
#define UTILITIES_H_

#include "string"
#include "vector"
#include "sstream"
#include "iostream"
#include "constants.h"
using namespace std;

#define DEBUG

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif // DEBUG


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

#endif