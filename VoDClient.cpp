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
#include <math.h>
#include <vector>
#include <algorithm>
#include "easywsclient.hpp"
#define TIME_DELTA 800 // in microseconds
#define POP_NUM 1
#define MAX_QUEUE 10000
#define MEAN_NUM 2000

using namespace std;
using namespace easywsclient;
typedef pair<int, pair<int, uint64_t>> pp;
typedef queue<pair<int, uint64_t>> bq;

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

void PrintLevelLog(uint64_t msgTime, int &num_msg, double &mean_val, double &std_val, double &max_val, double &min_val, vector<uint64_t> &data)
{
    cout << "----------------------------------------------" << "\n";
    cout << "(msec) >>> " << micros() << "\n";
    cout << "(TimeStamp) >>> " << msgTime << "\n";
    uint64_t diffTime = micros() - msgTime;
    cout << "(time difference) >>> " << diffTime << "\n";
    num_msg ++;
    if (num_msg >= 2) std_val = ((num_msg - 2) / (double)(num_msg - 1)) * std_val + (diffTime - mean_val) * (diffTime - mean_val) / (double)num_msg;
    mean_val = (diffTime + (num_msg - 1) * mean_val) / num_msg;
    max_val = max(max_val, diffTime);
    min_val = min(min_val, diffTime);
    data.push_back(diffTime);
    cout << "----------------------------------------------" << "\n";
    cout << "\n\n";
}

template<typename T>
static inline double Lerp(T v0, T v1, T t)
{
    return (1 - t)*v0 + t*v1;
}

template<typename T>
static inline std::vector<T> Quantile(const std::vector<T>& inData, const std::vector<double>& probs)
{
    if (inData.empty())
    {
        return std::vector<T>();
    }

    if (1 == inData.size())
    {
        return std::vector<T>(1, inData[0]);
    }

    std::vector<T> data = inData;
    std::sort(data.begin(), data.end());
    std::vector<T> quantiles;

    for (size_t i = 0; i < probs.size(); ++i)
    {
        T poi = Lerp<double>(-0.5, data.size() - 0.5, probs[i]);

        size_t left = std::max(int64_t(std::floor(poi)), int64_t(0));
        size_t right = std::min(int64_t(std::ceil(poi)), int64_t(data.size() - 1));

        T datLeft = data.at(left);
        T datRight = data.at(right);

        T quantile = Lerp<double>(datLeft, datRight, poi - left);

        quantiles.push_back(quantile);
    }

    return quantiles;
}

void popNextElement(bq lv1, bq lv2, bq lv3, bq lv4, uint64_t t, int lv) 
{
    if (!lv1.empty()) {
        t = lv1.front().second;
        lv1.pop();
        lv = 1;
    } else if (!lv2.empty()){
        t = lv2.front().second;
        lv2.pop();
        lv = 2;
    } else if (!lv3.empty()){
        t = lv3.front().second;
        lv3.pop();
        lv = 3;
    } else if (!lv4.empty()){
        t = lv4.front().second;
        lv4.pop();
        lv = 4;
    } else {
        t = 0;
        lv = -1;
    }
}

int main(int argc, char **argv)
{
    ios::sync_with_stdio(0);
    cin.tie(0);
    cout.tie(0);

    unique_ptr<WebSocket> ws(WebSocket::from_url("ws://localhost:3000/bsm"));
    assert(ws);

    bq q_lv1, q_lv2, q_lv3, q_lv4;
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
    double mean_lv1 = 0, std_lv1 = 0, max_lv1 = 0, min_lv1 = 9876543210;
    double mean_lv2 = 0, std_lv2 = 0, max_lv2 = 0, min_lv2 = 9876543210;
    double mean_lv3 = 0, std_lv3 = 0, max_lv3 = 0, min_lv3 = 9876543210;
    double mean_lv4 = 0, std_lv4 = 0, max_lv4 = 0, min_lv4 = 9876543210;
    vector<uint64_t> data_lv1;
    vector<uint64_t> data_lv2;
    vector<uint64_t> data_lv3;
    vector<uint64_t> data_lv4;

    bool flag = false;
    while (ws->getReadyState() != WebSocket::CLOSED) {
        WebSocket::pointer wsp = &*ws; // <-- because a unique_ptr cannot be copied into a lambda
        uint64_t _t = micros();
        ws->poll();
        ws->dispatch([&, wsp](const std::string & message) {
            int type, level;
            string time;

            SimpleParser(message, type, level, time);
            uint64_t t = micros();
            uint64_t timeUnix = stol(time);
            if (num_msg_lv3 <= MEAN_NUM) {
                switch (level)
                {
                case 1:
                    q_lv1.push({level, timeUnix});
                    break;
                case 2:
                    q_lv2.push({level, timeUnix});
                    break;
                case 3:
                    q_lv3.push({level, timeUnix});
                    break;
                case 4:
                    q_lv4.push({level, timeUnix});
                    break;
                default:
                    break;
                }
            }

            if  (_t - start_time > TIME_DELTA) {
                int cnt = 0;
                uint64_t timestamp;
                int lv;
                popNextElement(q_lv1, q_lv2, q_lv3, q_lv4, timestamp, lv);
                while (cnt < POP_NUM && timestamp != 0) 
                {
                    cnt ++;
                    if (lv == 1) PrintLevelLog(timestamp, num_msg_lv1, mean_lv1, std_lv1, max_lv1, min_lv1, data_lv1);
                    if (lv == 2) PrintLevelLog(timestamp, num_msg_lv2, mean_lv2, std_lv2, max_lv2, min_lv2, data_lv2);
                    if (lv == 3) PrintLevelLog(timestamp, num_msg_lv3, mean_lv3, std_lv3, max_lv3, min_lv3, data_lv3);
                    if (lv == 4) PrintLevelLog(timestamp, num_msg_lv4, mean_lv4, std_lv4, max_lv4, min_lv4, data_lv4);
                    popNextElement(q_lv1, q_lv2, q_lv3, q_lv4, timestamp, lv);
                }
                start_time = _t;
            }

            if (num_msg_lv2 >= MEAN_NUM) {

                auto quartiles_lv1 = Quantile<uint64_t>(data_lv1, { 0.25, 0.5, 0.75 });
                cout << "---------------- Lv1 Mean Time & Failure Rate ----------------" << "\n";
                cout << "Number of Message: " << num_msg_lv1 << "\n";
                cout << "Mean Time: " << mean_lv1 << "\n";
                cout << "std of Time: " << sqrt(std_lv1) << "\n";
                cout << "Max Time: " << max_lv1 << "\n";
                cout << "Min Time: " << min_lv1 << "\n";
                cout << "First Quantile: " << quartiles_lv1[0] << endl;
                cout << "Median: " << quartiles_lv1[1] << endl;
                cout << "Third Quantile: " << quartiles_lv1[2] << endl;
                cout << "--------------------------------------------------------------" << "\n";

                auto quartiles_lv2 = Quantile<uint64_t>(data_lv2, { 0.25, 0.5, 0.75 });
                cout << "---------------- Lv2 Mean Time & Failure Rate ----------------" << "\n";
                cout << "Number of Message: " << num_msg_lv2 << "\n";
                cout << "Mean Time: " << mean_lv2 << "\n";
                cout << "std of Time: " << sqrt(std_lv2) << "\n";
                cout << "Max Time: " << max_lv2 << "\n";
                cout << "Min Time: " << min_lv2 << "\n";
                cout << "First Quantile: " << quartiles_lv2[0] << endl;
                cout << "Median: " << quartiles_lv2[1] << endl;
                cout << "Third Quantile: " << quartiles_lv2[2] << endl;
                cout << "--------------------------------------------------------------" << "\n";
                
                auto quartiles_lv3 = Quantile<uint64_t>(data_lv3, { 0.25, 0.5, 0.75 });
                cout << "---------------- Lv3 Mean Time & Failure Rate ----------------" << "\n";
                cout << "Number of Message: " << num_msg_lv3 << "\n";
                cout << "Mean Time: " << mean_lv3 << "\n";
                cout << "std of Time: " << sqrt(std_lv3) << "\n";
                cout << "Max Time: " << max_lv3 << "\n";
                cout << "Min Time: " << min_lv3 << "\n";
                cout << "First Quantile: " << quartiles_lv3[0] << endl;
                cout << "Median: " << quartiles_lv3[1] << endl;
                cout << "Third Quantile: " << quartiles_lv3[2] << endl;
                cout << "--------------------------------------------------------------" << "\n";

                auto quartiles_lv4 = Quantile<uint64_t>(data_lv4, { 0.25, 0.5, 0.75 });
                cout << "---------------- Lv4 Mean Time & Failure Rate ----------------" << "\n";
                cout << "Number of Message: " << num_msg_lv4 << "\n";
                cout << "Mean Time: " << mean_lv4 << "\n";
                cout << "std of Time: " << sqrt(std_lv4) << "\n";
                cout << "Max Time: " << max_lv4 << "\n";
                cout << "Min Time: " << min_lv4 << "\n";
                cout << "First Quantile: " << quartiles_lv4[0] << endl;
                cout << "Median: " << quartiles_lv4[1] << endl;
                cout << "Third Quantile: " << quartiles_lv4[2] << endl;
                cout << "--------------------------------------------------------------" << "\n";
                flag = true;
            }
        });

        if (flag) break;
        
    }
    return 0;
}