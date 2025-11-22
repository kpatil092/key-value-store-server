#ifndef DB_CONNECTION_POOL_H
#define DB_CONNECTION_POOL_H

#include <iostream>
#include <string>
#include <queue>
#include <libpq-fe.h>
#include <condition_variable>
#include <mutex>
#include "Exceptions.h"

class DBConnectionPool {
private:
  std::queue<PGconn*> conn_queue;
  int size;
  std::mutex mtx;
  std::condition_variable cv;
  std::string conn_string;

  PGconn* acquire_conn();

public:

  DBConnectionPool() = default;
  explicit DBConnectionPool(const std::string& conn_string, int n=8);
  ~DBConnectionPool();

  void createPool();
  std::pair<bool, std::string> get(std::string key);
  bool set(std::string key, std::string value);
  bool remove(std::string key);
};
 

#endif