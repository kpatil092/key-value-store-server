#ifndef DB_CONNECTION_POOL_H
#define DB_CONNECTION_POOL_H

#include <iostream>
#include <string>
#include <queue>
#include <libpq-fe.h>
#include <condition_variable>
#include <mutex>
#include "Exceptions.h"

using namespace std;

class DBConnectionPool {
private:
  queue<PGconn*> conn_queue;
  int size;
  mutex mtx;
  condition_variable cv;
  string conn_string;

  PGconn* acquire_conn();

public:

  DBConnectionPool() = default;
  explicit DBConnectionPool(const string& conn_string, int n=8);
  ~DBConnectionPool();

  void createPool();
  pair<bool, string> get(string key);
  bool set(string key, string value);
  bool remove(string key);
};
 

#endif