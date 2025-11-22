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


class Cache {
private:
  struct Bucket {
    std::mutex mtx;
    std::list<std::pair<std::string, std::string>> lru;
    std::unordered_map<std::string, std::list<std::pair<std::string,std::string>>::iterator> idx_map;
    int capacity;

    Bucket(int capacity=0) : capacity(capacity) {}
  };

  Bucket* buckets;
  int buckets_count;

  int hash(const std::string &key);
public:
  Cache() = default;
  explicit Cache(int capacity, int buckets_count);
  ~Cache();

  std::pair<bool, std::string> get(const std::string &key);
  bool set(const std::string &key, const std::string &value);
  bool delete_(const std::string &key);
};

#endif