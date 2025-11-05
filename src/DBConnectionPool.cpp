#include "DBConnectionPool.h"

inline PGconn* DBConnectionPool::acquire_conn() {
  unique_lock<mutex> lock(mtx);
  cv.wait(lock, [&](){return !conn_queue.empty();});
  PGconn* conn = conn_queue.front(); conn_queue.pop();
  return conn;
}

DBConnectionPool::DBConnectionPool(const std::string& conn_string, int n)
  : size(n), conn_string(conn_string) {}

void DBConnectionPool::createPool() {
  lock_guard<mutex> lock(mtx);
  PGconn* conn = PQconnectdb(conn_string.c_str());
  if(PQstatus(conn) != CONNECTION_OK) {
    string err = PQerrorMessage(conn);
    PQfinish(conn);
    throw Exception_("Postgres", "Fail to connect: " + err);
  }

  const char* create_query = "CREATE TABLE IF NOT EXISTS kvstore(key TEXT PRIMARY KEY, value TEXT)";
  PGresult *res = PQexec(conn, create_query); 
  if(PQresultStatus(res) !=  PGRES_COMMAND_OK) {
    string err = PQerrorMessage(conn);
    PQclear(res);
    PQfinish(conn);
    throw Exception_("Postgres", "Fail to create table: " + err);
    return;
  }
  PQclear(res);
  PQfinish(conn);

  for(int i=0; i<size; i++) {
    PGconn* conn = PQconnectdb(conn_string.c_str());
    if(PQstatus(conn) != CONNECTION_OK) {
      string err = PQerrorMessage(conn);
      PQfinish(conn);
      while(conn_queue.size()) {
        PGconn* conn = conn_queue.front();conn_queue.pop();
        PQfinish(conn);
      }
      throw Exception_("Postgres", "Connection failure:" + err);
    } else {
      conn_queue.push(conn);
    }
    
  }
  cv.notify_all();
}

DBConnectionPool::~DBConnectionPool() {
  lock_guard<mutex> lock(mtx);
  while(conn_queue.size()) {
    PGconn* conn = conn_queue.front(); conn_queue.pop();
    PQfinish(conn);
  }
}



pair<bool, string> DBConnectionPool::get(string key){
  PGconn* conn = acquire_conn();
  const char* param[1] = {key.c_str()};
  PGresult *res = PQexecParams(conn,
    "SELECT value from kvstore where key = $1;",
    1, nullptr, param, nullptr, nullptr, 0
  );

  pair<bool, string> result;
  if(PQresultStatus(res) != PGRES_TUPLES_OK){
    string err = PQerrorMessage(conn);
    PQclear(res);
    // cout << "DBPostgres:GET_request_success: (" << key << ", " << "val" << ")\n"; 
    throw Exception_("Postgres", "Fail to access: " + err);
  } else if (PQntuples(res) == 0){
    // cout << "DBPostgres:GET_request_failed: (" << key << ", " << "---" << ")\n";
    PQclear(res);
    result = {false, ""};
  } else {
    string val = PQgetvalue(res, 0, 0);
    result = {true, val};
    PQclear(res);
    // cout << "DBPostgres:GET_request_success: (" << key << ", " << "val" << ")\n";
  }

  {
    lock_guard<mutex> lock(mtx);
    conn_queue.push(conn);
  }
  cv.notify_one();
  return result;
}

bool DBConnectionPool::set(string key, string value) {
  PGconn* conn = acquire_conn();
  const char* param[2] = {key.c_str(), value.c_str()};
  PGresult *res = PQexecParams(conn,
    "INSERT INTO kvstore (key, value) VALUES ($1, $2) "
    "ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value;",
    2, nullptr, param, nullptr, nullptr, 0
  );

  bool result = true;
  if(PQresultStatus(res) != PGRES_COMMAND_OK){
    string err = PQerrorMessage(conn);
    // cout << "DBPostgres:SET_request_failed: (" << key << ", " << value << ")\n";
    PQclear(res);
    throw Exception_("Postgres", "Fail to access: " + err);
  }
  // cout << "DBPostgres:SET_request_success: (" << key << ", " << value << ")\n";
  PQclear(res);

  {
    lock_guard<mutex> lock(mtx);
    conn_queue.push(conn);
  }
  cv.notify_one();

  return result;
}

bool DBConnectionPool::remove(string key) {
  PGconn* conn = acquire_conn();
  const char* param[1] = {key.c_str()};
  PGresult *res = PQexecParams(conn,
    "DELETE FROM kvstore WHERE key = $1; ",
    1, nullptr, param, nullptr, nullptr, 0
  );

  bool result = true;
  if(PQresultStatus(res) != PGRES_COMMAND_OK){
    string err = PQerrorMessage(conn);
    // cout << "DBPostgres:DELETE_request_failed: (" << key << ", " << ")\n";
    PQclear(res);
    throw Exception_("Postgres", "Fail to access: " + err);
  }
  // cout << "DBPostgres:DELETE_request_success: (" << key << ", " << "---" << ")\n";
  PQclear(res);

  {
    lock_guard<mutex> lock(mtx);
    conn_queue.push(conn);
  }
  cv.notify_one();

  return result;
}