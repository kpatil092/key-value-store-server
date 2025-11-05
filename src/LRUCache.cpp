#include "LRUCache.h"

LRUCache::LRUCache(int cap): capacity(cap) {}

pair<int, string> LRUCache::get(string &key) {
  lock_guard<mutex> lock(this->mtx);
  auto itr = this->map.find(key);
  if(itr == map.end()){ 
    return {0, ""};
  }
  this->cache.splice(cache.begin(), cache, itr->second);
  return {1, itr->second->second};
}

int LRUCache::set(string &key, string &value) {
  lock_guard<mutex> lock(this->mtx);
  auto itr = this->map.find(key);
  if(itr != map.end()){
    itr->second->second = value;
    this->cache.splice(cache.begin(), cache, itr->second);
    return 1;
  }
  if(cache.size() == this->capacity) {
    auto back = cache.back();
    map.erase(back.first);
    cache.pop_back();
  }
  cache.emplace_front(key, value);
  map[key] = cache.begin();
  return 1;
}

int LRUCache::delete_(string& key) {
  lock_guard<mutex> lock(this->mtx);
  auto itr = this->map.find(key);
  if(itr == map.end()) return 0;
  cache.erase(itr->second);
  map.erase(itr);
  return 1;
}
