#include "Cache.h"

Cache::Cache(int capacity, int buckets_count): buckets_count(max(1, buckets_count)) {
  int buc_capacity = max(1, capacity/buckets_count);

  buckets = new Bucket[buckets_count];
  for(int i=0; i<buckets_count; i++) {
    buckets[i].capacity = (buc_capacity);
    buckets[i].idx_map.reserve(buc_capacity);
  }
}

Cache::~Cache() {
  delete [] buckets;
  buckets = nullptr;
}

int Cache::hash(const string &key) {
  // djb2 hash
  unsigned long hash = 5381;
  int maxStretch = min((int)key.size(), 64);
  for(int i=0; i< maxStretch; i++) {
    char c = key[i];
    hash = ((hash << 5) + hash) + c;
  }
  return (int)hash % buckets_count;
}

pair<bool, string> Cache::get(const string &key) {
  int b = hash(key);
  Bucket& bucket = buckets[b];
  lock_guard<mutex> lock(bucket.mtx);
  auto itr = bucket.idx_map.find(key);
  if(itr == bucket.idx_map.end()){ 
    return {false, ""};
  }
  bucket.lru.splice(bucket.lru.begin(), bucket.lru, itr->second);
  return {true, itr->second->second};
}

bool Cache::set(const string &key, const string &value) {
  int b = hash(key);
  Bucket& bucket = buckets[b];
  lock_guard<mutex> lock(bucket.mtx);
  auto itr = bucket.idx_map.find(key);
  if(itr != bucket.idx_map.end()){
    itr->second->second = value;
    bucket.lru.splice(bucket.lru.begin(), bucket.lru, itr->second);
    return 1;
  }
  if(bucket.lru.size() == bucket.capacity) {
    auto back = bucket.lru.back();
    bucket.idx_map.erase(back.first);
    bucket.lru.pop_back();
  }
  bucket.lru.emplace_front(key, value);
  bucket.idx_map[key] = bucket.lru.begin();
  return 1;
}

bool Cache::delete_(const string& key) {
  int b = hash(key);
  Bucket& bucket = buckets[b];
  lock_guard<mutex> lock(bucket.mtx);
  auto itr = bucket.idx_map.find(key);
  if(itr == bucket.idx_map.end()) return 0;
  bucket.lru.erase(itr->second);
  bucket.idx_map.erase(itr);
  return 1;
}