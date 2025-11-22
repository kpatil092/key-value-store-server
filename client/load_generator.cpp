#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>

#include "httplib.h"

#define POOL_SIZE 10000

using namespace std;

atomic<bool> done(false);
vector<string> key_pool;
vector<string> value_pool;

struct Info {
    size_t client_id;
    string hostname;
    int port;
    size_t duration;
    size_t think_time;

    long long success = 0;
    long long failures = 0;
    long long total_latency_ns = 0;
    long long min_latency_ns = LLONG_MAX;
    long long max_latency_ns = 0;
};

string generate_string(int len, unsigned int *seed, int num) {
    static const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int N = sizeof(chars) -1;
    string s;
    s.reserve(len + 6);

    for (int i = 0; i < len; ++i) {
        int idx = rand_r(seed) % N;
        s.push_back(chars[idx]);
    }
    s += "_";
    s += to_string(num);

    return s;
}

void worker_routine(Info *info, double get_ratio, double put_ratio) {
    httplib::Client client(info->hostname, info->port);
    client.set_read_timeout(5, 0); 
    unsigned int seed = time(NULL) ^ (unsigned long)info;

    while (true) {
        if (done.load()) {
            break;
        }

        double r = rand_r(&seed)/(double)RAND_MAX;
        size_t idx = rand_r(&seed) % (POOL_SIZE);

        const string &key = key_pool[idx];
        const string &value = value_pool[idx];


        string path = "/api/" + key;

        auto start = chrono::high_resolution_clock::now();

        httplib::Result res;
        if(r < get_ratio) {
            res = client.Get(path.c_str());
        } else if(r < get_ratio+put_ratio) {
            res = client.Put(path.c_str(), value, "text/plain");
        } else {
            res = client.Delete(path.c_str());
        }

        auto end = chrono::high_resolution_clock::now();

        long long latency = chrono::duration_cast<chrono::nanoseconds>(end - start).count(); // in nanosecond

        if (res && res->status < 500) {
            info->success++;
        } else {
            info->failures++;
        }

        info->total_latency_ns += latency;
        info->min_latency_ns = min(info->min_latency_ns, latency);
        info->max_latency_ns = max(info->max_latency_ns, latency);

        // Think time
        if (info->think_time > 0) {
            usleep((useconds_t)info->think_time * 1000);
        }
    }
}

int main(int argc, char *argv[]) {

    if(argc < 5) {
        cerr << "format : ./load_generator <host> <port> <clients> <test_duration> [think_time] [test_mode:0/1/2]\n";
        return 1;
    }

    string host = argv[1];
    int port = atoi(argv[2]);
    int clients = atoi(argv[3]);
    double duration = atof(argv[4]);
    double think_time = argc >= 6 ? atof(argv[5]) : 0;
    int mode = argc >= 7 && atoi(argv[6])<3 ? atoi(argv[6]) : 2;

    double get_ratio = 0.7;
    double put_ratio = 0.2;
    double del_ratio = 0.1;


    // Building string pool
    key_pool.reserve(POOL_SIZE);
    value_pool.reserve(POOL_SIZE);

    cout << "Building string pool...\n";
    unsigned int seed = (unsigned int)time(NULL);
    for (size_t i = 0; i < POOL_SIZE; ++i) {
        key_pool.push_back(generate_string(14, &seed, i));
        value_pool.push_back(generate_string(44, &seed, i));
    }

    // sleep(1);

    cout << "Starting load testing... \n";
    cout << "  Host:       " << host << "\n";
    cout << "  Port:       " << port << "\n";
    cout << "  Clients:    " << clients << "\n";
    cout << "  Duration:   " << duration << " sec\n";
    cout << "  Think time: " << think_time << " ms\n";

    vector<Info> info(clients);
    for(int i=0; i<clients; i++) {
        info[i].client_id = i;
        info[i].hostname = host;
        info[i].port = port;
        info[i].duration = duration;
        info[i].think_time = think_time;
    }

    vector<thread> workers;
    workers.reserve(clients);

    auto master_start = chrono::steady_clock::now();

    for (int i = 0; i < clients; ++i) {
        workers.emplace_back(
            worker_routine,
            &info[i],
            get_ratio,
            put_ratio
        );
    }

    sleep(duration);
    done.store(true);

    for (int i = 0; i < clients; ++i) {
        workers[i].join();
    }

    long long success = 0, failure = 0;
    long long total_latency_ns = 0;
    long long min_latency_ns = LLONG_MAX;
    long long max_latency_ns = 0;

    for (const auto &s : info) {
        success += s.success;
        failure += s.failures;
        total_latency_ns += s.total_latency_ns;
        min_latency_ns = std::min(min_latency_ns, s.min_latency_ns);
        max_latency_ns = std::max(max_latency_ns, s.max_latency_ns);
    }

    long long total = success + failure;

    cout << "---- RESULTS ----\n";
    cout << "Total requests: " << total << "\n";
    cout << "Success:        " << success << "\n";
    cout << "Failures:       " << failure << "\n";
    cout << "Duration:       " << duration << " sec\n";

    if (duration > 0.0) {
        double throughput = (double)total / duration;
        cout << "Throughput:    " << throughput << " req/s\n";
    }

    if (total > 0 && total_latency_ns > 0) {
        double avg_latency_ns = total_latency_ns / (double)total;

        cout << "Avg latency:   " << (avg_latency_ns / 1e6) << " ms\n";
        cout << "Min latency:   " << (min_latency_ns / 1e6) << " ms\n";
        cout << "Max latency:   " << (max_latency_ns / 1e6) << " ms\n";
    }

    return 0;
}
