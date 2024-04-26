#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>

class RingBuffer {
private:
    std::vector<std::string> buffer_;
    size_t capacity_;
    size_t front_;
    size_t rear_;
    size_t count_;

public:
    explicit RingBuffer(size_t size) : capacity_(size), front_(0), rear_(0), count_(0) {
        buffer_.resize(capacity_);
    }

    bool Push(std::string&& item) {
        if (count_ >= capacity_) return false;
        buffer_[rear_] = std::move(item);
        rear_ = (rear_ + 1) % capacity_;
        count_++;
        return true;
    }

    bool Pop(std::string& item) {
        if (count_ == 0) return false;
        item = buffer_[front_];
        front_ = (front_ + 1) % capacity_;
        count_--;
        return true;
    }
};

constexpr int kBacklog = 1024;
constexpr int kRingBufferSize = kBacklog;
constexpr int kBufferSize = 512;
const std::string g_received_message = "ok";

RingBuffer g_buffer(kBufferSize);
std::mutex g_lock;

int MakeServer(uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in host = {0};

    host.sin_port = htons(port);
    host.sin_family = AF_INET;
    host.sin_addr.s_addr = INADDR_ANY;

    int reuse_enable = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_enable, sizeof(int)) != 0) {
        perror("setsockopt");
        exit(0);
    }

    if (bind(fd, (struct sockaddr*)&host, sizeof(host)) != 0) {
        perror("bind");
        exit(0);
    }

    if (listen(fd, kBacklog) != 0) {
        perror("listen");
        exit(0);
    }

    return fd;
}

void ConnectionHandler(int fd)
{
    char buffer[kBufferSize];

    int result = recv(fd, buffer, kBufferSize, 0);

    if (result < 0) {
        perror("recv");
        close(fd);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_lock);

        g_buffer.Push({ buffer, (size_t) result });
    }

    result = send(fd, g_received_message.data(), (int) g_received_message.size(), 0);

    if (result < 0) {
        perror("send");
        close(fd);
        return;
    }

    std::cout << "Got new message!" << std::endl;
    close(fd);
}

int main() {
    int server_fd = MakeServer(7777);

    while (true) {
        int client_fd = accept(server_fd, (struct sockaddr*)nullptr, 0);

        if (client_fd < 0) continue;

        std::cout << "Got new client!" << std::endl;

        std::thread thread(ConnectionHandler, client_fd);
        thread.detach();
    }
}
