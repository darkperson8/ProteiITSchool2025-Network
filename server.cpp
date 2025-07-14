#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <unordered_map>
#include <netinet/in.h>
#include <sys/epoll.h>
#include "calculator.h"

static const int MAX_EVENTS = 64;

int setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0] << " <порт>\n";
        return 1;
    }
    int port = std::stoi(argv[1]);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(listen_fd, SOMAXCONN) < 0) {
        perror("listen"); return 1;
    }
    setNonBlocking(listen_fd);

    int epfd = epoll_create1(0);
    if (epfd < 0) { perror("epoll_create1"); return 1; }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    std::unordered_map<int, std::string> buffers;
    epoll_event events[MAX_EVENTS];

    std::cout << "[server] listening on port " << port << "\n";

    while (true) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (n < 0) { perror("epoll_wait"); break; }
        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            if (fd == listen_fd) {
                // Accept new
                while (true) {
                    int client = accept(listen_fd, nullptr, nullptr);
                    if (client < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept"); break;
                    }
                    setNonBlocking(client);
                    epoll_event cev{};
                    cev.events = EPOLLIN | EPOLLET;
                    cev.data.fd = client;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, client, &cev);
                    buffers[client] = "";
                    std::cout << "[server] new connection fd=" << client << "\n";
                }
            }
            else if (events[i].events & EPOLLIN) {
                // Read
                char buf[512];
                bool any = false;
                while (true) {
                    ssize_t cnt = read(fd, buf, sizeof(buf));
                    if (cnt <= 0) break;
                    any = true;
                    buffers[fd].append(buf, cnt);
                    std::cout << "[server] fd=" << fd << " read " << cnt << " bytes\n";
                }
                if (!any) continue;

                // Process complete expressions
                auto &data = buffers[fd];
                size_t pos;
                while ((pos = data.find(' ')) != std::string::npos) {
                    std::string exp = data.substr(0, pos);
                    data.erase(0, pos + 1);
                    double result = calc(infixToRPN(exp));
                    std::string out = std::to_string(result) + " ";
                    write(fd, out.c_str(), out.size());
                    std::cout << "[server] fd=" << fd
                              << " expr=\"" << exp
                              << "\" -> " << result << "\n";
                }
            }
            else {
                // Close
                std::cout << "[server] closing fd=" << fd << "\n";
                close(fd);
                buffers.erase(fd);
            }
        }
    }
    close(listen_fd);
    return 0;
}