#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <list>
#include <thread>
#include <string>
#include <unordered_map>

using namespace std;

class LRUCache {
private: 
  int capacity;
  mutex mtx;
  list<pair<string, string>> cache;
  unordered_map<string, list<pair<string,string>>::iterator> map;

public:
  explicit LRUCache(int cap);

  pair<int, string> get(string &key);
  int set(string &key, string &value);
  int delete_(string &key);
};


#endif