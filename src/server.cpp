#include "server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
//新增响应
#include <fcntl.h>
#include <sys/epoll.h>
#include <cerrno>

Server::Server(int port) : port_(port), listen_fd_(-1) {}

Server::~Server() {
    if (listen_fd_ != -1) close(listen_fd_);
}

void Server::start() {
    // 1. 创建 socket
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ == -1) {
        perror("socket");
        exit(1);
    }

    // 2. 设置端口复用，避免重启时 bind 失败
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 3. bind
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(1);
    }

    // 4. listen
    if (listen(listen_fd_, 10) == -1) {
        perror("listen");
        exit(1);
    }

    printf("Server listening on port %d\n", port_);

     // 5. 设置 listen_fd_ 为非阻塞
    int flags = fcntl(listen_fd_, F_GETFL, 0);
    fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK);

    // 6. 创建 epoll 实例
    int epfd = epoll_create1(0);

    // 7. 把 listen_fd_ 注册到 epoll，监听可读事件，使用 ET 模式
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_fd_;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd_, &ev);

    // 8. 循环 accept
    struct epoll_event events[1024];
    while (true) {
        int n = epoll_wait(epfd, events, 1024, -1);
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            if (fd == listen_fd_) {
                // 处理新连接：循环 accept 直到 EAGAIN
                handle_new_connection(epfd);
            } else {
                // 处理已连接 socket 的读写
                handle_client(fd);
            }
        }
    }
}

void Server::handle_new_connection(int epfd) {
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int conn_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &len);

        if (conn_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;  // 没有更多连接了
            perror("accept");
            break;
        }

        // 设置 conn_fd 为非阻塞
        int flags = fcntl(conn_fd, F_GETFL, 0);
        fcntl(conn_fd, F_SETFL, flags | O_NONBLOCK);

        // 把 conn_fd 注册到 epoll（ET 模式）
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = conn_fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &ev);
    }
}

void Server::handle_client(int conn_fd) {
    // 1. 循环读，直到 EAGAIN
    char buffer[4096];
    std::string request;
    while (true) {
        ssize_t n = read(conn_fd, buffer, sizeof(buffer));
        if (n > 0) {
            request.append(buffer, n);
        } else if (n == 0) {
            // 客户端关闭连接
            close(conn_fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            close(conn_fd);
            return;
        }
    }

    // 2. 简单解析 HTTP 请求行（第一行）
    //    格式：GET /index.html HTTP/1.1
    //    你需要从中提取出请求路径
    std::string path= "/"; //默认路径
    size_t first_space = request.find(' ');
    if (first_space != std::string::npos){
        size_t second_space = request.find(' ', first_space + 1);
        if (second_space != std::string::npos){
            path = request.substr(first_space + 1, second_space - first_space - 1);
        }
    }


    // 3. 根据路径读取 www 下的文件，构造 HTTP 响应
    //    注意：默认路径 "/" 映射到 "www/index.html"
    if (path == "/") path = "/index.html";
    std::string file_path= "www" + path;
    printf("请求路径: %s, 文件路径: %s\n", path.c_str(), file_path.c_str());

    // 4. 写响应（ET 模式下也要注意写循环，但可以先简单 write）
    std::ifstream file(file_path);
	if(!file.is_open()){
		const char* not_found = 
		        "HTTP/1.1 404 Not Found\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n"
                "\r\n";
            write(conn_fd, not_found, strlen(not_found));
            close(conn_fd);
            return;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string body = ss.str();

    // 简单判断 Content-Type（先只处理 html 和 css）
    std::string content_type = "text/html";
    if (file_path.find(".css") != std::string::npos){
        content_type = "text/css";
    }else if(file_path.find(".js") != std::string::npos){
        content_type = "application/javascript";
    }

    std::string response = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: " + content_type + "\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n" + body;

    // 5. 关闭连接（或保留长连接，后续再加）
    write(conn_fd, response.c_str(), response.size());
    close(conn_fd);
}