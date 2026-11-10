#include "server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

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

    // 5. 循环 accept
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int conn_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (conn_fd == -1) {
            perror("accept");
            continue;
        }

        // 读取请求（先简单读一下，不解析）
        char buffer[1024];
        read(conn_fd, buffer, sizeof(buffer));

	//读取 www/index.html
	std::ifstream file("www/index.html");
	if(!file.is_open()){
		const char* not_found = 
		"HTTP/1.1 404 Not Found\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n"
                "\r\n";
            write(conn_fd, not_found, strlen(not_found));
            close(conn_fd);
            continue;
        }
        
	std::stringstream ss;
        ss << file.rdbuf();
        std::string body = ss.str();

        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "Connection: close\r\n"
            "\r\n" + body;

        write(conn_fd, response.c_str(), response.size());
        close(conn_fd);
    }
}
