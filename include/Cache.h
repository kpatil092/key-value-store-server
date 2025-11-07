#ifndef CACHE_H
#define CACHE_H

#include <iostream>
#include <mutex>
#include <condition_variable>
#include <list>
#include <vector>
#include <thread>
#include <string>
#include <unordered_map>

using namespace std;

class Cache {
private:
  struct Bucket {
    mutex mtx;
    list<pair<string, string>> lru;
    unordered_map<string, list<pair<string,string>>::iterator> idx_map;
    int capacity;

    Bucket(int capacity=0) : capacity(capacity) {}
  };

  Bucket* buckets;
  int buckets_count;

  int hash(const string &key);
public:
  Cache() = default;
  explicit Cache(int capacity, int buckets_count);
  ~Cache();

  pair<bool, string> get(const string &key);
  bool set(const string &key, const string &value);
  bool delete_(const string &key);
};

#endif