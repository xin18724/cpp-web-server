#ifndef SERVER_H
#define SERVER_H

class Server {
public:
    Server(int port);
    ~Server();
    void start();

private:
    int port_;
    int listen_fd_;
    void handle_new_connection(int epfd);
    void handle_client(int conn_fd);
};

#endif