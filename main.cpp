#include "config.h"

int main(int argc, char* argv[]) 
{
    // mysql相关信息
    string user = "root";
    string passwd = "root";
    string databasename = "mydb";

    // 命令行解析
    Config config;
    config.parse_arg(argc, argv);

    // webserver
    WebServer server;

    server.init(config.PORT, user, passwd, databasename, config.LOGWrite, 
                config.OPT_LINGER, config.TRIGMode, config.sql_num, config.thread_num, 
                config.close_log, config.actor_model);

    // log
    // server.log_write();

    // database
    // server.sql_pool();

    // threadpool
    // server.thread_pool();

    // 触发模式
    server.trig_mode();

    // listen
    server.eventListen();

    // run
    // server.eventLoop();

    return 0;
}