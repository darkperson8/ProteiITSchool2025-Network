#include <fcntl.h>
#include <iostream>
#include <random>
#include <sstream>
#include <unistd.h>
#include <unordered_map>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#include "calculator.h"

// Установить файловый дескриптор в неблокирующий режим
int setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Генерация арифметического выражения из n чисел
std::string genExpression(int n) {
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> numDist(1, 10);
    std::vector<std::string> ops = {"+","-","*","/"};
    std::uniform_int_distribution<> opDist(0, ops.size() - 1);
    std::ostringstream oss;
    for (int i = 0; i < n; ++i) {
        oss << numDist(gen);
        if (i < n - 1)
            oss << ops[opDist(gen)];
    }
    return oss.str();
}

// Разбиение строки на случайные фрагменты
std::vector<std::string> sliceRandom(const std::string& s) {
    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> lenDist(1, s.size());
    std::vector<std::string> parts;
    size_t idx = 0;
    while (idx < s.size()) {
        size_t remain = s.size() - idx;
        size_t len = std::min(remain, (size_t)lenDist(gen));
        parts.push_back(s.substr(idx, len));
        idx += len;
    }
    return parts;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Использование: " << argv[0]
                  << " <n> <connections> <server_addr> <server_port>\n";
        return 1;
    }
    int n           = std::stoi(argv[1]);
    int connections = std::stoi(argv[2]);
    const char* addr= argv[3];
    int port        = std::stoi(argv[4]);

    // Генерируем выражение и ожидаемый результат
    std::string expr     = genExpression(n);
    double      expected = calc(infixToRPN(expr));
    expr += " ";
    auto parts = sliceRandom(expr);

    std::cout << "[client] expr=\"" << expr << "\" expected=" << expected
              << ", parts=" << parts.size() << "\n";

    // Создаём epoll
    int epfd = epoll_create1(0);
    if (epfd < 0) { perror("epoll_create1"); return 1; }

    struct epoll_event ev;
    std::vector<struct epoll_event> events(connections);

    // Открываем соединения
    for (int i = 0; i < connections; ++i) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        setNonBlocking(sock);
        std::cout << "[client] socket fd=" << sock << "\n";

        sockaddr_in srv{};
        srv.sin_family = AF_INET;
        srv.sin_port   = htons(port);
        inet_pton(AF_INET, addr, &srv.sin_addr);

        int res = connect(sock, (sockaddr*)&srv, sizeof(srv));
        if (res < 0 && errno != EINPROGRESS) {
            perror("connect");
            close(sock);
            continue;
        }

        ev.events  = EPOLLIN | EPOLLOUT | EPOLLET;  // edge-triggered
        ev.data.fd = sock;
        epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev);
    }

    std::unordered_map<int, size_t> sentIdx;
    std::unordered_map<int, std::string> recvBuf;
    int remaining = connections;

    // Цикл обработки epoll-событий
    while (remaining > 0) {
        int nfd = epoll_wait(epfd, events.data(), connections, -1);
        if (nfd < 0) { perror("epoll_wait"); break; }

        for (int i = 0; i < nfd; ++i) {
            int fd = events[i].data.fd;

            // EDGE-TRIGGERED: внутри одного EPOLLOUT шлём все части
            if (events[i].events & EPOLLOUT) {
                size_t &idx = sentIdx[fd];
                while (idx < parts.size()) {
                    const auto &chunk = parts[idx];
                    ssize_t sent = write(fd, chunk.c_str(), chunk.size());
                    if (sent < 0) {
                        if (errno == EAGAIN) break;  // нет места — ждём следующий EPOLLOUT
                        perror("write");
                        break;
                    }
                    std::cout << "[client] fd=" << fd
                              << " sent chunk[" << idx << "] size=" << sent << "\n";
                    ++idx;
                }
            }

            // Чтение ответа
            if (events[i].events & EPOLLIN) {
                char buf[256];
                ssize_t cnt = read(fd, buf, sizeof(buf));
                if (cnt > 0) {
                    recvBuf[fd].append(buf, cnt);
                    std::cout << "[client] fd=" << fd
                              << " recv " << cnt << " bytes\n";
                }
                // Как только видим пробел — считаем, что пришёл полный ответ
                if (recvBuf[fd].find(' ') != std::string::npos) {
                    double got = std::stod(recvBuf[fd]);
                    std::cout << "[client] fd=" << fd
                              << " response=" << got
                              << ", expected=" << expected << "\n";
                    if (std::fabs(got - expected) > 1e-6) {
                        std::cerr << "[client][ERROR] expr=\"" << expr
                                  << "\" got=" << got
                                  << " expected=" << expected << "\n";
                    }
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                    close(fd);
                    --remaining;
                }
            }
        }
    }

    close(epfd);
    return 0;
}
