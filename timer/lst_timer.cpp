#include "lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst()
{
    head = NULL;
    tail = NULL;
}

sort_timer_lst::~sort_timer_lst()
{
    util_timer *tmp = head;
    while(tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}
// 添加新的定时器
void sort_timer_lst::add_timer(util_timer *timer)
{
    if(!timer) return;
    if(!head) 
    {
        head = tail = timer;
        return;
    }
    // 如果新的定时器超时时间小于当前头部结点，则直接将当前定时器结点作为头部结点
    if(timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    // 则调用私有成员，调整内部结点
    add_timer(timer, head);
}

// 私有成员，被公有成员add_timer和adjust_time调用，主要用于调整链表内部结点
void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head)
{
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;
    while(tmp)
    {
        if(timer->expire < tmp->expire)
        {
            timer->prev = prev;
            timer->next = tmp;
            prev->next = timer;
            tmp->prev = timer;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if(!tmp)
    {
        timer->prev = prev;
        timer->next = NULL;
        prev->next = timer;
        tail = timer;
    }
}

// 调整定时器，任务发生变化时，调整定时器在链表中的位置
void sort_timer_lst::adjust_timer(util_timer *timer)
{
    if(!timer) return;
    util_timer *tmp = timer->next;
    if(!tmp || (timer->expire < tmp->expire)) return;
    // 被调整定时器是链表头结点，将定时器取出，重新插入 
    if(timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next - NULL;
        add_timer(timer, head);
    }
    else  // 被调整定时器在内部，将定时器取出，重新插入
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

// 删除定时器
void sort_timer_lst::del_timer(util_timer *timer)
{
    if(!timer) return;
    // 链表中只有一个定时器
    if((timer == head) &&(timer == tail))
    {
        delete timer;
        head = tail = NULL;
        return;
    }
    // 被删除的定时器为链表头
    if(timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    // 被删除的定时器为尾
    if(timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    // 被删除的定时器在链表中
    timer->next->prev = timer->prev;
    timer->prev->next = timer->next;
    delete timer;
}

void sort_timer_lst::tick()
{
    if(!head) return;
    
    // 获取当前时间
    time_t cur = time(NULL);
    util_timer *tmp = head;
    while(tmp)
    {
        // 链表容器为升序排列，当前时间小于定时器的超时时间，后面的定时器也没有到期
        if(cur < tmp->expire) break;
        // 当前定时器到期，则调用回调函数，执行定时事件
        tmp->cb_func(tmp->user_data);
        // 将处理后的定时器从链表容器中删除，并重置头结点
        head = tmp->next;
        if(head) head->prev = NULL;
        delete tmp;
        tmp = head;
    }
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

// 对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if(TRIGMode == 1) event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else event.events = EPOLLIN | EPOLLRDHUP;

    if(one_shot) event.events |= EPOLLONESHOT;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 信号处理函数中仅仅通过管道发送信号值，不处理信号对应的逻辑，缩短异步执行时间，减少对主程序的影响
void Utils::sig_handler(int sig)
{
    // 为保证函数的可重入性，保留原来的errno
    // 可重入性表示中断后再次进入该函数，环境变量与之前相同，不会丢失数据
    int save_errno = errno;
    int msg = sig;
    // 将信号值从管道写端写入，传输字符类型，而非整型
    send(u_pipefd[1], (char *)&msg, 1, 0);
    // 将原来的errno赋值为当前的errno
    errno = save_errno;
}

// 设置信号, 项目中设置信号函数，仅关注SIGTERM和SIGALRM两个信号
void Utils::addsig(int sig, void(handler)(int), bool restart)
{   
    // 创建sigaction结构体变量
    struct sigaction sa;        
    memset(&sa, '\0', sizeof(sa));
    // 信号处理函数中仅仅发送信号值，不做对应逻辑处理
    sa.sa_handler = handler;
    if(restart) sa.sa_flags |= SA_RESTART;
    // 将所有信号添加到信号集中
    sigfillset(&sa.sa_mask);
    // 执行sigaction函数
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

// 定时器回调函数
class Utils;
void cb_func(client_data *user_data)
{
    // 删除非活动连接在socket上的注册事件
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    // 关闭文件描述符
    close(user_data->sockfd);
    // 减少连接数
    http_conn::m_user_count--;
}
