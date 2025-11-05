# KV Store Server

## Overview

It is lightweight key-value store server built with C++.
* Built with 3 layers
* HTTP interface built on cpp-httplib
* Integreted in memory LRU Cache for fast lookup
* Used PostgreSQL DB for persistant storage


## Execution Instructions

1. Compile the server.

```
g++ -std=c++17 -O2 -pthread -Iinclude ./src/*.cpp -o server -I/usr/include/postgresql -lpq
```

2. Set database string in shell (terminal).

```
export DB_CONN="host=localhost port=5432 dbname=<dbname> user=<username> password=<password>"
```

3. Run the program.
```
./server <port> <threads> <cachesize>
```


<!-- FLTO: 
g++ -std=c++17 -O3 -flto -pthread -Iinclude ./src/*.cpp -o server -I/usr/include/postgresql -lpq -->


## Testing Instruction

1. PUT request
```
curl -X PUT http://localhost:8000/api/<key> -d <value>
```

2. GET request
```
curl -X GET http://localhost:8000/api/<key>
```

3. DELETE request
```
curl -X DELETE http://localhost:8000/api/<key>
```