#include <iostream>
#include <libpq-fe.h>
#include <cstdlib>

#include "DBConnectionPool.h"
#include "Cache.h"

#include "httplib.h"

#define CACHE_BUCKETS 10

using namespace std;
using namespace httplib;

int main(int argc, char* argv[]) {
  int port = 8000;
  int threads = 8;
  int cachesize = 1000;

  try {
    if(argc >= 2) {
      port = stoi(argv[1]);
      if(port < 1023 || port > 56635) {
        cerr << "format : ./server [port] [threads] [cachesize]";
        return 1;
      }
    }
    if(argc >= 3) {
      threads = stoi(argv[2]);
      threads = max(1, threads);
      threads = min(threads, 64);
    }
    if(argc >= 4) {
      cachesize = stoi(argv[3]);
      cachesize = max(1, cachesize);
      cachesize = min(cachesize, 1000000000);
    }
  } catch(exception) {
    cerr << "format : ./server [port] [threads] [cachesize]\n";
    return 1;
  }

  const char* db_conn = getenv("DB_CONN");
  string connectionString;

  if(db_conn) connectionString = db_conn;
  else {
    cerr << "Database connection string is not provided!\n";
    return 1;
  }
  
  int bucket_size = cachesize/CACHE_BUCKETS;
  Cache cache(bucket_size, CACHE_BUCKETS);

  DBConnectionPool dbclient(connectionString, threads);

  try {
    dbclient.createPool(); 
  } catch(const Exception_& e) {
    cerr << e.what() << endl;
    return 1;
  }
  
  Server svr;
  svr.new_task_queue = [&] { return new ThreadPool(threads); };

  svr.Get("/hi", [](const Request &, Response &res) {
    res.set_content("Hello World!", "text/plain");
  });

  svr.Get(R"(/api/(.+))", [&](const Request &req, Response &res) {
    string key = req.matches[1];
    pair<int, string> result;
    // pool.enqueue([&]() {
    try{
      result = cache.get(key);
      if(!result.first) {
        result = dbclient.get(key);
        if(!result.first) {
          // res.status = 404;
          res.status = 200;
          res.set_content("NOT_FOUND", "text/plain");
          return;
        }
        cache.set(key, result.second);
        res.status = 200;
        res.set_content(result.second, "text/plain");
      } else {
        res.status = 200;
        res.set_content(result.second, "text/plain"); 
      }
    } catch(const Exception_& e) {
      res.status = 500;
      res.set_content("Server Error", "text/plain");
    }
    // });
  });

  svr.Put(R"(/api/(.+))", [&](const Request &req, Response &res) {
    string key = req.matches[1];
    string value = req.body;

    try{
    // pool.enqueue([&]() {
      dbclient.set(key, value);
      cache.set(key, value);
      res.status = 200;
      res.set_content("OK", "text/plain");
    } catch(const Exception_& e) {
      res.status = 500;
      res.set_content("Server Error", "text/plain");
    }
    // });
  });

  svr.Delete(R"(/api/(.+))", [&](const Request &req, Response &res) {
    string key = req.matches[1];
    // pool.enqueue([&](){
    try {
      dbclient.remove(key);
      cache.delete_(key);
      res.status = 200;
      res.set_content("OK", "text/plain");
    } catch(const Exception_& e) {
      res.status = 500;
      res.set_content("Server Error", "text/plain");
    }
    // });
  });

  cout << "server is running at http://localhost:" << port <<endl;
  cout << "Threads: " << threads << " Cache size: " << cachesize << endl; 
  svr.listen("localhost", port);

  // pool.shutdown();
  return 0;
}