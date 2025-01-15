#include "webserver.h"
#include <cstring>

WebServer::WebServer() {
    printf("WebServer Constructor!\n");
}

WebServer::~WebServer() {
    printf("WebServer Destructor!\n");
}

void WebServer::init(int port, std::string user, std::string passwd, std::string databaseName,
                int log_write, int opt_linger, int trigmode, int sql_num,
                int thread_num, int close_log, int actor_model) {
    m_port = port;
    printf("WebServer Init!\nPort=%d\n", port);

    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
    m_actormodel = actor_model;

}

void WebServer::trig_mode() {
    // LT + LT
    if(0 == m_TRIGMode) {
        m_LISTENTrigmode = 0;
        m_CONNTrigmodel = 0;
    } // LT + ET
    else if(1 == m_TRIGMode) {
        m_LISTENTrigmode = 0;
        m_CONNTrigmodel = 1;
    } // ET + LT
    else if(2 == m_TRIGMode) {
        m_LISTENTrigmode = 1;
        m_CONNTrigmodel = 0;
    } // ET + ET
    else if(3 == m_TRIGMode) {
        m_LISTENTrigmode = 1 ;
        m_CONNTrigmodel = 1;
    }
}

void WebServer::eventListen() {
    // 网络编程基本步骤
    // 1. 创建监听套接字
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    // 2. 优雅关闭链接(Linger 选项)
    if(m_OPT_LINGER == 0) {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    } else if(m_OPT_LINGER == 1) {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    // 3. 准备服务器端地址结构
    int ret = 0;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    // 4. 设置地址重用选项（SO_REUSEADDR）
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    // 5. 绑定和监听
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    // 6. 初始化定时器
    // utils.init(TIMESLOT);

    // 7. 创建epoll内核时间表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    // 8. 将监听套接字添加到epoll中
    // utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    // http_conn::m_epollfd = m_epollfd;

    // 9. 创建一对 UNIX 套接字（socketpair），并将其中一端加入 epoll
    ret = socketpair(PF_INET, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    /*utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);*/

    /* 10. 设置信号处理
    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);*/

    // 11. 启动定时器
    // alarm(TIMESLOT);

    /* 12. 将管道和 epoll 的文件描述符传给工具类
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;*/

}