#include "webserver.h"
#include <cstring>

WebServer::WebServer() {
    printf("WebServer Constructor!\n");

    // http_conn类对象
    users = new http_conn[MAX_FD];

    // 定时器
    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer() {
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

void WebServer::init(int port, std::string user, std::string passwd, std::string databaseName,
                int log_write, int opt_linger, int trigmode, int sql_num,
                int thread_num, int close_log, int actor_model) {
    m_port = port;
    m_user = user;
    m_passWord = passwd;
    m_databaseName = databaseName;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
    m_actormodel = actor_model;

}

void WebServer::thread_pool()
{
    // 线程池
    m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

void WebServer::sql_pool()
{
    // 初始化数据库连接池
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);

    // 初始化数据库读取表
    users->initmysql_result(m_connPool);
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
    http_conn::m_epollfd = m_epollfd;

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

void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
    users[connfd].init(connfd, client_address, m_root, m_CONNTrigmodel, m_close_log, m_user, m_passWord, m_databaseName);

    // 初始化client_data数据
    // 创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);
}

// 若有数据传输，则将定时器往后延迟3个单位
// 并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    // LOG_INFO("%s", "adjust timer once");
}

void WebServer::deal_timer(util_timer *timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if(timer)
    {
        utils.m_timer_lst.del_timer(timer);
    }

    // LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

bool WebServer::dealclientdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (0 == m_LISTENTrigmode)
    {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0)
        {
            // LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (http_conn::m_user_count >= MAX_FD)
        {
            utils.show_error(connfd, "Internal server busy");
            // LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    }

    else
    {
        while (1)
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0)
            {
                // LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Internal server busy");
                // LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

bool WebServer::dealwithsignal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    // 从管道读端读出信号值，成功返回字节数，失败返回-1
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if(ret == -1) return false;
    else if(ret == 0) return false;
    else 
    {
        for(int i = 0; i < ret; i++)
        {
            switch(signals[i])
            {
                case SIGALRM: // 接收到SIGALRM信号，timeout设置为true
                {
                    timeout = true;
                    break;
                }
                case SIGTERM: // 接收到SIGTERM信号，终止进程运行，stop_server设置为true
                {
                    stop_server = true;
                    break;
                }
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;

    // reactor
    if(m_actormodel == 1)
    {
        if(timer) adjust_timer(timer);

        // 若监测到读事件，将该事件放入请求队列
        m_pool->append(users + sockfd, 0);

        while(true)
        {
            if(users[sockfd].improv == 1)
            {
                if(users[sockfd].timer_flag == 1)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else 
    {
        // proactor
        if (users[sockfd].read_once())
        {
            // LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            //若监测到读事件，将该事件放入请求队列
            m_pool->append_p(users + sockfd);

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;
    //reactor
    if (1 == m_actormodel)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        m_pool->append(users + sockfd, 1);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        //proactor
        if (users[sockfd].write())
        {
            // LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::eventLoop()
{
    bool timeout = false;
    bool stop_server = false;

    while(!stop_server)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number < 0 || errno != EINTR)
        {
            // LOG_ERROR("%s", "epoll failure");
            break;
        }

        for(int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            // 处理新到的客户连接
            if(sockfd == m_listenfd)
            {
                bool flag = dealclientdata();
                if(!flag) continue;
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 服务器端关闭连接，移除对应的定时器
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            else if((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                // 处理信号
                bool flag = dealwithsignal(timeout, stop_server);
                if(!flag)
                    int xxx = false;
                    // LOG_ERROR("%s", "dealclientdata failure");
            }
            else if(events[i].events & EPOLLIN)
            {
                // 处理客户连接上收到的数据
                dealwithread(sockfd);
            }
            else if(events[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        if(timeout)
        {
            utils.timer_handler();

            // LOG_ERROR("%s", "timer tick");

            timeout = false;
        }
    }
}