#ifndef WEBSOCKET_SERVER_HPP
#define WEBSOCKET_SERVER_HPP

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
using SocketHandle = SOCKET;
#else
using SocketHandle = int;
#endif

class WebSocketServer {
public:
    explicit WebSocketServer(unsigned short port);
    ~WebSocketServer();

    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;

    void start();
    void publish(std::string message);
    void stop();

private:
    void run();

    unsigned short port;
    SocketHandle listener;
    std::atomic<bool> running;
    bool socketSubsystemInitialized;
    std::mutex messageMutex;
    std::string latestMessage;
    std::thread worker;
};

#endif
