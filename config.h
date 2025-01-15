#ifndef CONFIG_H
#define CONFIG_H

#include "webserver.h"

using namespace std;

class Config {
public:
    Config();
    ~Config(){};

    void parse_arg(int argc, char *argv[]);

    // Port
    int PORT;

    // log写入方式
    int LOGWrite;

    // 触发组合模式
    int TRIGMode;

    // listenfd触发模式
    int LISTENTrigmode;

    // connfd触发模式
    int CONNTrigmode;

    // 关闭连接
    int OPT_LINGER;

    // database连接池数量
    int sql_num;

    // threadpool数量
    int thread_num;

    // 是否关闭log
    int close_log;

    // 并发模式
    int actor_model;
};

#endif