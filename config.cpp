#include "config.h"

Config::Config() {
    // Port, default 9006
    PORT = 9006;

    // log写入方式, default 同步
    LOGWrite = 0;

    // 触发组合模式, default listened LT + connfd LT
    TRIGMode = 0;

    // listenfd触发模式, default LT
    LISTENTrigmode = 0;

    // connfd触发模式, default LT
    CONNTrigmode = 0;

    // 优雅关闭链接, default 不使用
    OPT_LINGER = 0;

    // database pool数量, default 8
    sql_num = 8;

    // thread pool数量, default 8
    thread_num = 8;

    // close log, default 不关
    close_log = 0;

    // 并发模型, default proactor
    actor_model = 0;
}

void Config::parse_arg(int argc, char *argv[]) {
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";
    while((opt = getopt(argc, argv, str)) != -1) {
        switch(opt) {
            case 'p': {
                PORT = atoi(optarg);
                break;
            }
            case 'l': {
                LOGWrite = atoi(optarg);
                break;
            }
            case 'm': {
                TRIGMode = atoi(optarg);
                break;
            }
            case 'o': {
                OPT_LINGER = atoi(optarg);
                break;
            }
            case 's': {
                sql_num = atoi(optarg);
                break;
            }
            case 't': {
                thread_num = atoi(optarg);
                break;
            }
            case 'c': {
                close_log = atoi(optarg);
                break;
            }
            case 'a': {
                actor_model = atoi(optarg);
                break;
            }
            default:
                break;
        }
    }
}