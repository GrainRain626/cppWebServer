#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T *request, int state);
    bool append_p(T *request);  //proactor模式下的任务请求入队

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        // 线程池中的线程数
    int m_max_requests;          //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理
    connection_pool *m_connPool;//数据库
    int m_actor_model;          //模型切换
};

template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool, int thread_number, int max_requests) : 
m_actor_model(actor_model), m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL), m_connPool(connPool)
{
    if(thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads)
        throw std::exception();
    for(int i = 0; i < thread_number; i++) 
    {
        if(thread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])) {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

template <typename T>
bool threadpool<T>::append(T *request, int state)
{
    m_queuelocker.lock();
    if(m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template <typename T>
bool threadpool<T>::append_p(T *request)
{
    m_queuelocker.lock();
    if(m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

//线程回调函数/工作函数，arg其实是this
template <typename T>
void* threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg; //将参数强行转化为线程池类，获取threadpool对象地址
    pool->run();                          //线程池中每个线程创建都会调用run()睡眠在队列中
    return pool;
}

//工作线程通过run函数不断等待任务队列有新任务，然后 加锁->取任务->解锁->执行任务
template <typename T>
void threadpool<T>::run()
{
    while(true)
    {
        m_queuestat.wait();               // 信号量等待
        m_queuelocker.lock();             // 工作线程被唤醒后先加互斥锁
        if(m_workqueue.empty) 
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front(); // 从请求队列中取出第一个任务
        m_workqueue.pop_front();          // 该任务从请求队列中弹出
        m_queuelocker.unlock();
        if(!request) continue;
        if(m_actor_model == 1)            //  proactor模式和reactor模式切换
        {
            /*
            reactor模式：只有reactor模式下，标志位improv和timer_flag才会发挥作用
            imporv：在read_once和write成功后会置1，对应request完成后置0，用于判断上一个请求是否已处理完毕
            timer_flag：当http的读写失败后置1，用于判断用户连接是否异常
            */
            if(request->m_state == 0)
            {
                if(request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else
                {
                    request->improve = 1;
                    request->timer_flag = 1;
                }
            }
            else 
            {
                if(request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else
        {
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }
}
#endif