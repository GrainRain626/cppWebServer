#ifndef HTTPCONNECTION_h
#define HTTPCONNECTION_h
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <map>

// #include "../lock/locker.h"
// #include "../CGImysql/sql_connection_pool.h"
// #include "../timer/lst_timer.h"
// #include "../log/log.h"


class http_conn 
{
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE                 // 主状态机的状态（程序运行的阶段）
    {
        CHECK_STATE_REQUESTLINE = 0, // 正在分析请求行
        CHECK_STATE_HEADER,          // 正在分析头部字段
        CHECK_STATE_CONTENT          // 解析消息体，仅用于解析POST请求
    };
    enum LINE_STATUS                 // 从状态机的状态（读取行的结果）
    {
        LINE_OK = 0,                 // 完整读取一行
        LINE_BAD,                    // 报文语法有误
        LINE_OPEN                    // 读取的行不完整
    };
    enum HTTP_CODE
    {
        NO_REQUEST,                  // 请求不完整，需要继续读取请求报文数据 - 跳转主线程继续监测读事件
        GET_REQUEST,                 // 获得了完整的HTTP请求 - 调用do_request完成请求资源映射
        BAD_REQUEST,                 // HTTP请求报文有语法错误或请求资源为目录 - 跳转process_write完成响应报文
        NO_RESOURCE,                 // 请求资源不存在 - 跳转process_write完成响应报文
        FORBIDDEN_REQUEST,           // 请求资源禁止访问，没有读取权限 - 跳转process_write完成响应报文
        FILE_REQUEST,                // 请求资源可以正常访问 - 跳转process_write完成响应报文
        INTERNAL_ERROR,              // 服务器内部错误，该结果在主状态机逻辑switch的default下，一般不会触发
        CLOSED_CONNECTION            // 客户端关闭连接
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, std::string user, std::string passwd, std::string sqlname);
    void close_cnn(bool real_close = true);
    void process();
    bool read_once();
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    void initmysql_result();
    int timer_flag;
    int improv;

public:
    static int m_epollfd;
    static int m_user_count;
    // MYSQL *mysql;
    int m_state; // read为0, write为1

private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

private:
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];
    long m_read_idx;
    long m_checked_idx;
    int m_start_line;
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    CHECK_STATE m_check_state;
    METHOD m_method;
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    long m_content_length;
    bool m_linger;
    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;          // 是否启用的POST
    char *m_string;   // 存储请求头数据
    int bytes_to_send;
    int bytes_have_send;
    char *doc_root;

    std::map<std::string, std::string> m_users;
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif

