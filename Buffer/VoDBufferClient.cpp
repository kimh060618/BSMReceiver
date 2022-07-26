#include <iostream>
#include <assert.h>
#include <fstream>
#include <stdio.h>
#include <string>
#include <memory>
#include <ctime>
#include <chrono>
#include <sys/time.h>
#include <queue>
#include <tuple>
#include "easywsclient.hpp"
#include <math.h>
#define TIME_DELTA 100 * 1000 // in microseconds
#define POP_NUM 1
#define MAX_QUEUE 10000
#define BUFFER_NUM 10000
#define MEAN_NUM 2000

using namespace std;
using namespace easywsclient;
typedef pair<int, pair<int, uint64_t>> pp;

struct cmp{
    bool operator()(pp t, pp u){
        if (t.first == u.first) {
            if (t.second.first == u.second.first) return t.second.second > u.second.second;
            else return t.second.first < t.second.first;
        }
        return t.first < u.first;
    }
};


void PrintLog(string message) 
{
    cout << "(raw msg) >>> " << message.c_str();
    cout << "(Type) >>> " << message[8] - '0' << "\n";
    cout << "(TimeStamp) >>> " << message.substr(17, 16) << "\n";
    cout << "(level) >>> " << message[message.size() - 3] - '0' << "\n";
    cout << "(size) >>> " << message.size() << "\n";
}

void SimpleParser(string message, int &type, int &level, string &timestamp) 
{
    type = message[8] - '0';
    level = message[message.size() - 3] - '0';
    timestamp = message.substr(17, 16);
}

uint64_t micros()
{
    uint64_t us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    return us; 
}

double min(double a, uint64_t b) { return a < b ? a : b; }
double max(double a, uint64_t b) { return a > b ? a : b; }

void PrintLevelLog(int type, int level, uint64_t msgTime, int &num_msg, double &mean_val, double &std_val, double &max_val, double &min_val)
{
    cout << "-------------- Message Level: " << -1 * level << " --------------" << "\n";
    cout << "(msec) >>> " << micros() << "\n";
    cout << "(TimeStamp) >>> " << msgTime << "\n";
    uint64_t diffTime = micros() - msgTime;
    cout << "(time difference) >>> " << diffTime << "\n";
    num_msg ++;
    if (num_msg >= 2) std_val = ((num_msg - 2) / (double)(num_msg - 1)) * std_val + (diffTime - mean_val) * (diffTime - mean_val) / (double)num_msg;
    mean_val = (diffTime + (num_msg - 1) * mean_val) / num_msg;
    max_val = max(max_val, diffTime);
    min_val = min(min_val, diffTime);
    cout << "----------------------------------------------" << "\n";
    cout << "\n\n";
}

int main(int argc, char **argv)
{
    unique_ptr<WebSocket> ws(WebSocket::from_url("ws://localhost:3000/bsm"));
    assert(ws);

    int enablePQ = argv[1][0] - '0';
    priority_queue<pp, vector<pp>, cmp> pq;
    queue<pp> q;
    queue<pp> buffer;
    uint64_t start_time = micros();

    int num_msg_lv1 = 0;
    int num_msg_lv2 = 0;
    int num_msg_lv3 = 0;
    int num_msg_lv4 = 0;
    uint64_t sum_lv1 = 0;
    uint64_t sum_lv2 = 0;
    uint64_t sum_lv3 = 0;
    uint64_t sum_lv4 = 0;
    int num_fail_lv1 = 0;
    int num_fail_lv2 = 0;
    int num_fail_lv3 = 0;
    int num_fail_lv4 = 0;
    double mean_lv1, std_lv1, max_lv1, min_lv1 = 0;
    double mean_lv2, std_lv2, max_lv2, min_lv2 = 0;
    double mean_lv3, std_lv3, max_lv3, min_lv3 = 0;
    double mean_lv4, std_lv4, max_lv4, min_lv4 = 0;


    bool flag = false;
    while (ws->getReadyState() != WebSocket::CLOSED) {
        WebSocket::pointer wsp = &*ws; // <-- because a unique_ptr cannot be copied into a lambda
        uint64_t _t = micros();
        ws->poll();
        ws->dispatch([&, wsp, enablePQ](const std::string & message) { // 
            int type, level;
            string time;
            // PrintLog(message);
            SimpleParser(message, type, level, time);
            uint64_t t = micros();
            uint64_t timeUnix = stol(time);
            if (num_msg_lv3 <= MEAN_NUM) { 
                if (!enablePQ) {
                    if (q.size() < MAX_QUEUE) q.push({-1 * level, { -1 * type, timeUnix }});
                    else if (buffer.size() < BUFFER_NUM) {
                        buffer.push({-1 * level, { -1 * type, timeUnix }});
                    }
                    else {
                        switch (level)
                        {
                        case 1:
                            num_msg_lv1++;
                            num_fail_lv1 ++;
                        break;
                        case 2:
                            num_msg_lv2++;
                            num_fail_lv2 ++;
                        break;
                        case 3:
                            num_msg_lv3++;
                            num_fail_lv3 ++;
                        break;
                        case 4:
                            num_msg_lv4++;
                            num_fail_lv4 ++;
                        break;
                        }
                    }
                }
                else { 
                    if (pq.size() < MAX_QUEUE) pq.push({-1 * level, { -1 * type, timeUnix }}); 
                    else if (buffer.size() < BUFFER_NUM) {
                        buffer.push({-1 * level, { -1 * type, timeUnix }});
                    }
                    else {
                        switch (level)
                        {
                        case 1:
                            num_msg_lv1++;
                            num_fail_lv1 ++;
                        break;
                        case 2:
                            num_msg_lv2++;
                            num_fail_lv2 ++;
                        break;
                        case 3:
                            num_msg_lv3++;
                            num_fail_lv3 ++;
                        break;
                        case 4:
                            num_msg_lv4++;
                            num_fail_lv4 ++;
                        break;
                        }
                    }
                }
            }

            if  (_t - start_time > TIME_DELTA) {
                if (enablePQ) {
                    int cnt = 0;
                    while (cnt < POP_NUM && !pq.empty()) 
                    {
                        int level = pq.top().first;
                        int type = pq.top().second.first;
                        uint64_t msgTime = pq.top().second.second;
                        pq.pop();
                        cnt ++;
                        if (level == -1) PrintLevelLog(type, level, msgTime, num_msg_lv1, mean_lv1, std_lv1, max_lv1, min_lv1);
                        if (level == -2) PrintLevelLog(type, level, msgTime, num_msg_lv2, mean_lv2, std_lv2, max_lv2, min_lv2);
                        if (level == -3) PrintLevelLog(type, level, msgTime, num_msg_lv3, mean_lv3, std_lv3, max_lv3, min_lv3);
                        if (level == -4) PrintLevelLog(type, level, msgTime, num_msg_lv4, mean_lv4, std_lv4, max_lv4, min_lv4);
                        if (pq.size() < MAX_QUEUE) {
                            pq.push(buffer.front());
                            buffer.pop();
                        }
                    }
                }
                else {
                    int cnt = 0;
                    while (cnt < POP_NUM && !q.empty()) 
                    {
                        int level = pq.top().first;
                        int type = pq.top().second.first;
                        uint64_t msgTime = pq.top().second.second;
                        q.pop();
                        cnt ++;
                        if (level == -1) PrintLevelLog(type, level, msgTime, num_msg_lv1, mean_lv1, std_lv1, max_lv1, min_lv1);
                        if (level == -2) PrintLevelLog(type, level, msgTime, num_msg_lv2, mean_lv2, std_lv2, max_lv2, min_lv2);
                        if (level == -3) PrintLevelLog(type, level, msgTime, num_msg_lv3, mean_lv3, std_lv3, max_lv3, min_lv3);
                        if (level == -4) PrintLevelLog(type, level, msgTime, num_msg_lv4, mean_lv4, std_lv4, max_lv4, min_lv4);
                        if (pq.size() < MAX_QUEUE) {
                            pq.push(buffer.front());
                            buffer.pop();
                        }
                    }
                    
                }
                start_time = _t;
            }
            if (num_msg_lv2 >= MEAN_NUM || ((enablePQ && pq.empty()) || (!enablePQ && q.empty()))) {
                
                cout << "---------------- Lv1 Mean Time & Failure Rate ----------------" << "\n";
                cout << "Number of Message: " << num_msg_lv1 << "\n";
                cout << "Mean Time: " << mean_lv1 << "\n";
                cout << "std of Time: " << sqrt(std_lv1) << "\n";
                cout << "Max Time: " << max_lv1 << "\n";
                cout << "Min Time: " << min_lv1 << "\n";
                cout << "--------------------------------------------------------------" << "\n";

                cout << "---------------- Lv2 Mean Time & Failure Rate ----------------" << "\n";
                cout << "Number of Message: " << num_msg_lv2 << "\n";
                cout << "Mean Time: " << mean_lv2 << "\n";
                cout << "std of Time: " << sqrt(std_lv2) << "\n";
                cout << "Max Time: " << max_lv2 << "\n";
                cout << "Min Time: " << min_lv2 << "\n";
                cout << "--------------------------------------------------------------" << "\n";

                cout << "---------------- Lv3 Mean Time & Failure Rate ----------------" << "\n";
                cout << "Number of Message: " << num_msg_lv3 << "\n";
                cout << "Mean Time: " << mean_lv3 << "\n";
                cout << "std of Time: " << sqrt(std_lv3) << "\n";
                cout << "Max Time: " << max_lv3 << "\n";
                cout << "Min Time: " << min_lv3 << "\n";
                cout << "--------------------------------------------------------------" << "\n";

                cout << "---------------- Lv4 Mean Time & Failure Rate ----------------" << "\n";
                cout << "Number of Message: " << num_msg_lv4 << "\n";
                cout << "Mean Time: " << mean_lv4 << "\n";
                cout << "std of Time: " << sqrt(std_lv4) << "\n";
                cout << "Max Time: " << max_lv4 << "\n";
                cout << "Min Time: " << min_lv4 << "\n";
                cout << "--------------------------------------------------------------" << "\n";
                flag = true;
            }
        });

        if (flag) break;
        
    }
    return 0;
}