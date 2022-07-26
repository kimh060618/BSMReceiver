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
#define TIME_DELTA 800 // in microseconds
#define POP_NUM 1
#define MAX_QUEUE 10000
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

void PrintLevelLog(int type, int level, uint64_t msgTime, uint64_t &sum, int &num_fail, int &num_msg)
{
    cout << "-------------- Message Level: " << -1 * level << " --------------" << "\n";
    cout << "(msec) >>> " << micros() << "\n";
    cout << "(TimeStamp) >>> " << msgTime << "\n";
    uint64_t diffTime = micros() - msgTime;
    cout << "(time difference) >>> " << diffTime << "\n";
    sum += diffTime;
    num_msg ++;
    switch (level)
    {
    case -1:
        if (diffTime >= 10 * 1000) num_fail ++;
        break;
    case -2:
        if (diffTime >= 30 * 1000) num_fail ++;
        break;
    case -3:
        if (diffTime >= 100 * 1000) num_fail ++;
        break;
    case -4:
        if (diffTime >= 1000 * 1000) num_fail ++;
    break;
    default:
        break;
    }
    cout << "----------------------------------------------" << "\n";
    cout << "\n\n";
}

int main(int argc, char **argv)
{
    ios::sync_with_stdio(0);
    cin.tie(0);
    cout.tie(0);

    unique_ptr<WebSocket> ws(WebSocket::from_url("ws://localhost:3000/bsm"));
    assert(ws);

    int enablePQ = argv[1][0] - '0';
    priority_queue<pp, vector<pp>, cmp> pq;
    queue<pp> q;
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

    bool flag = false;
    while (ws->getReadyState() != WebSocket::CLOSED) {
        WebSocket::pointer wsp = &*ws; // <-- because a unique_ptr cannot be copied into a lambda
        uint64_t _t = micros();
        ws->poll();
        ws->dispatch([&_t, &start_time, &flag, wsp, enablePQ, &num_msg_lv1, &num_msg_lv2, &num_msg_lv3, &num_msg_lv4, &num_fail_lv1, &num_fail_lv2, &num_fail_lv3, &num_fail_lv4, &sum_lv1, &sum_lv2, &sum_lv3, &sum_lv4, &pq, &q](const std::string & message) { // 
            int type, level;
            string time;
            // PrintLog(message);
            SimpleParser(message, type, level, time);
            uint64_t t = micros();
            uint64_t timeUnix = stol(time);
            if (num_msg_lv3 <= MEAN_NUM) {
                if (!enablePQ) {
                    if (q.size() < MAX_QUEUE) q.push({ -1 * level, { -1 * type, timeUnix } });
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
                    if (pq.size() < MAX_QUEUE) pq.push({ -1 * level, { -1 * type, timeUnix } }); 
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
                    cout << pq.size() << endl;
                    while (cnt < POP_NUM && !pq.empty()) 
                    {
                        int level = pq.top().first;
                        int type = pq.top().second.first;
                        uint64_t msgTime = pq.top().second.second;
                        pq.pop();
                        cnt ++;
                        if (level == -1) PrintLevelLog(type, level, msgTime, sum_lv1, num_fail_lv1, num_msg_lv1);
                        if (level == -2) PrintLevelLog(type, level, msgTime, sum_lv2, num_fail_lv2, num_msg_lv2);
                        if (level == -3) PrintLevelLog(type, level, msgTime, sum_lv3, num_fail_lv3, num_msg_lv3);
                        if (level == -4) PrintLevelLog(type, level, msgTime, sum_lv4, num_fail_lv4, num_msg_lv4);
                    }
                }
                else {
                    int cnt = 0;
                    cout << q.size() << endl;
                    while (cnt < POP_NUM && !q.empty()) 
                    {
                        int level = q.front().first;
                        int type = q.front().second.first;
                        uint64_t msgTime = q.front().second.second;
                        q.pop();
                        cnt ++;
                        if (level == -1) PrintLevelLog(type, level, msgTime, sum_lv1, num_fail_lv1, num_msg_lv1);
                        if (level == -2) PrintLevelLog(type, level, msgTime, sum_lv2, num_fail_lv2, num_msg_lv2);
                        if (level == -3) PrintLevelLog(type, level, msgTime, sum_lv3, num_fail_lv3, num_msg_lv3);
                        if (level == -4) PrintLevelLog(type, level, msgTime, sum_lv4, num_fail_lv4, num_msg_lv4);
                    }
                }
                start_time = _t;
            }
            if (num_msg_lv2 >= MEAN_NUM || ((enablePQ && pq.empty()) || (!enablePQ && q.empty()))) {

                cout << "---------------- Lv1 Mean Time & Failure Rate ----------------" << "\n";
                cout << sum_lv1 / (double)num_msg_lv1 << "\n";
                cout << num_fail_lv1 / (double)num_msg_lv1 << "\n";
                cout << "--------------------------------------------------------------" << "\n";

                cout << "---------------- Lv2 Mean Time & Failure Rate ----------------" << "\n";
                cout << sum_lv2 / (double)num_msg_lv2 << "\n";
                cout << num_fail_lv2 / (double)num_msg_lv2 << "\n";
                cout << "--------------------------------------------------------------" << "\n";

                cout << "---------------- Lv3 Mean Time & Failure Rate ----------------" << "\n";
                cout << sum_lv3 / (double)num_msg_lv3 << "\n";
                cout << num_fail_lv3 / (double)num_msg_lv3 << "\n";
                cout << "--------------------------------------------------------------" << "\n";

                cout << "---------------- Lv4 Mean Time & Failure Rate ----------------" << "\n";
                cout << sum_lv4 / (double)num_msg_lv4 << "\n";
                cout << num_fail_lv4 / (double)num_msg_lv4 << "\n";
                cout << "--------------------------------------------------------------" << "\n";
                flag = true;
            }
        });

        if (flag) break;
        
    }
    return 0;
}