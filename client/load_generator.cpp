#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <climits>
#include <algorithm>
#include <unistd.h>

#include "httplib.h"

#define POOL_SIZE 10000
#define PRELOAD_PERCENT_MODE_CACHE_HEAVY 10
#define PRELOAD_PERCENT_MODE_MIXED 30
#define PROB_MODE_CACHE_HEAVY 0.9
#define PROB_MODE_MIXED 0.5

using namespace std;

atomic<bool> done(false);
const int HOTSET_SIZE = POOL_SIZE * ((PRELOAD_PERCENT_MODE_CACHE_HEAVY)/100.0);
const int MIXED_MODE_PRELOAD_SIZE = POOL_SIZE * ((PRELOAD_PERCENT_MODE_MIXED)/100.0);
vector<string> key_pool;
vector<string> value_pool;

struct ModeConfig {
    double get_ops;
    double put_ops;
    double delete_ops;
};

vector<ModeConfig> config = {
    {0.95, 0.05, 0.0}, {0.1, 0.8, 0.1}, {0.70, 0.2, 0.1}
};

struct Info {
    size_t client_id;
    string hostname;
    int port;
    size_t duration;
    size_t think_time;
    int mode;
    double get_ops;
    double put_ops;
    double delete_ops;

    long long success = 0;
    long long failures = 0;
    long long total_latency_ns = 0;
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

void preload_operation(string &host, int port, int mode) {
    httplib::Client client(host, port);
    client.set_read_timeout(5, 0);
    int st_idx = 0, en_idx = mode==0 ? HOTSET_SIZE : MIXED_MODE_PRELOAD_SIZE;
    long success = 0, failures = 0;
    cout << "Preload data for mode " << mode << " testing...\n";
    for(int i=st_idx; i<en_idx; i++) {
        const string& k = key_pool[i];
        const string& v = value_pool[i];
        string path = "/api/" + k;
        httplib::Result res;
        res = client.Put(path.c_str(), v, "text/plain");
        if(res && res->status!=500) success++;
        else failures++;
    } 
    double success_rate = success / (double)(success + failures);
    cout << "Preload complete with success rate " << success_rate*100 << " % ...\n";
}

// for choosing key from intended region
int choose_index(int mode, unsigned int* seed, double r) {
    switch(mode) {
        case 0:
            if(r < PROB_MODE_CACHE_HEAVY) return rand_r(seed) % HOTSET_SIZE;
            else return HOTSET_SIZE + (rand_r(seed) % (POOL_SIZE - HOTSET_SIZE));
        case 1:
            return rand_r(seed) % POOL_SIZE;
        default:
            if(r < PROB_MODE_MIXED) return rand_r(seed) % HOTSET_SIZE;
            else return rand_r(seed) % POOL_SIZE;
    }
    return rand_r(seed) % (POOL_SIZE);
}

void worker_routine(Info *info) {
    httplib::Client client(info->hostname, info->port);
    client.set_read_timeout(5, 0); 
    unsigned int seed = time(NULL) ^ (unsigned long)info;

    while (true) {
        if (done.load()) {
            break;
        }

        double r_key = rand_r(&seed)/(double)RAND_MAX;

        double r_ops = rand_r(&seed)/(double)RAND_MAX;
        size_t idx = choose_index(info->mode, &seed, r_ops);

        string key = key_pool[idx];
        string value = value_pool[idx];

        string path = "/api/" + key;

        auto start = chrono::high_resolution_clock::now();

        httplib::Result res;
        if(r_key < info->get_ops) {
            res = client.Get(path.c_str());
        } else if(r_key < info->get_ops + info->put_ops) {
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

    // 0: CACHE_HEAVY
    // 1: DB_HEAVY
    // 2: MIXED

    double get_ratio = 0.7;
    double put_ratio = 0.2;
    double del_ratio = 0.1;


    // Building string pool
    int total_pool_size = POOL_SIZE;
    key_pool.reserve(total_pool_size);
    value_pool.reserve(total_pool_size);

    cout << "Building string pool...\n";
    unsigned int seed = (unsigned int)time(NULL);
    
    for (size_t i = 0; i < total_pool_size; ++i) {
        key_pool.push_back(generate_string(14, &seed, i));
        value_pool.push_back(generate_string(44, &seed, i));
    }

    if(mode == 0 || mode == 2) {
        preload_operation(host, port, mode);
    }

    // sleep(1);

    cout << "Starting load testing... \n";
    cout << "  Host:       " << host << "\n";
    cout << "  Port:       " << port << "\n";
    cout << "  Clients:    " << clients << "\n";
    cout << "  Duration:   " << duration << " sec\n";
    cout << "  Think time: " << think_time << " ms\n";
    cout << "  Mode:       " << (mode==0 ? "Cache Heavy\n" : mode == 1 ? "DB Heavy\n" : "Mixed\n");

    vector<Info> info(clients);
    for(int i=0; i<clients; i++) {
        info[i].client_id = i;
        info[i].hostname = host;
        info[i].port = port;
        info[i].duration = duration;
        info[i].think_time = think_time;
        info[i].mode = mode;
        info[i].get_ops = config[mode].get_ops;
        info[i].put_ops = config[mode].put_ops;
        info[i].delete_ops = config[mode].delete_ops;
    }

    vector<thread> workers;
    workers.reserve(clients);

    auto master_start = chrono::steady_clock::now();

    for (int i = 0; i < clients; ++i) {
        workers.emplace_back(
            worker_routine,
            &info[i]
        );
    }

    sleep(duration);
    done.store(true);

    for (int i = 0; i < clients; ++i) {
        workers[i].join();
    }

    long long success = 0, failure = 0;
    long long total_latency_ns = 0;

    for (const auto &s : info) {
        success += s.success;
        failure += s.failures;
        total_latency_ns += s.total_latency_ns;
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
    }

    return 0;
}


// preload kv in case 0 and 2