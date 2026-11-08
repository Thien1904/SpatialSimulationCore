#include "WebSocketServer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

#ifdef _WIN32
constexpr SocketHandle InvalidSocket = INVALID_SOCKET;
using SocketLength = int;

void closeSocket(SocketHandle socket) {
    closesocket(socket);
}

void shutdownSocket(SocketHandle socket) {
    shutdown(socket, SD_BOTH);
}
#else
constexpr SocketHandle InvalidSocket = -1;
using SocketLength = socklen_t;

void closeSocket(SocketHandle socket) {
    close(socket);
}

void shutdownSocket(SocketHandle socket) {
    shutdown(socket, SHUT_RDWR);
}
#endif

bool waitForSocket(SocketHandle socket, bool readable, int timeoutMilliseconds) {
    fd_set sockets;
    FD_ZERO(&sockets);
    FD_SET(socket, &sockets);

    timeval timeout{};
    timeout.tv_sec = timeoutMilliseconds / 1000;
    timeout.tv_usec = (timeoutMilliseconds % 1000) * 1000;

#ifdef _WIN32
    const int result = select(0, readable ? &sockets : nullptr, readable ? nullptr : &sockets, nullptr, &timeout);
#else
    const int result = select(socket + 1, readable ? &sockets : nullptr, readable ? nullptr : &sockets, nullptr, &timeout);
#endif
    return result > 0;
}

bool sendAll(SocketHandle socket, const char* data, std::size_t length) {
    std::size_t sent = 0;
    while (sent < length) {
#ifdef _WIN32
        const int result = send(socket, data + sent, static_cast<int>(std::min<std::size_t>(length - sent, INT_MAX)), 0);
#else
        const ssize_t result = send(socket, data + sent, length - sent, 0);
#endif
        if (result <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

std::array<std::uint8_t, 20> sha1(std::string_view input) {
    std::vector<std::uint8_t> data(input.begin(), input.end());
    const std::uint64_t bitLength = static_cast<std::uint64_t>(data.size()) * 8;
    data.push_back(0x80);
    while (data.size() % 64 != 56) {
        data.push_back(0);
    }
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<std::uint8_t>(bitLength >> (i * 8)));
    }

    std::uint32_t h0 = 0x67452301;
    std::uint32_t h1 = 0xEFCDAB89;
    std::uint32_t h2 = 0x98BADCFE;
    std::uint32_t h3 = 0x10325476;
    std::uint32_t h4 = 0xC3D2E1F0;

    for (std::size_t offset = 0; offset < data.size(); offset += 64) {
        std::array<std::uint32_t, 80> words{};
        for (std::size_t i = 0; i < 16; ++i) {
            const std::size_t index = offset + i * 4;
            words[i] = (static_cast<std::uint32_t>(data[index]) << 24) |
                       (static_cast<std::uint32_t>(data[index + 1]) << 16) |
                       (static_cast<std::uint32_t>(data[index + 2]) << 8) |
                       static_cast<std::uint32_t>(data[index + 3]);
        }
        for (std::size_t i = 16; i < words.size(); ++i) {
            const std::uint32_t value = words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16];
            words[i] = (value << 1) | (value >> 31);
        }

        std::uint32_t a = h0;
        std::uint32_t b = h1;
        std::uint32_t c = h2;
        std::uint32_t d = h3;
        std::uint32_t e = h4;

        for (std::size_t i = 0; i < words.size(); ++i) {
            std::uint32_t function;
            std::uint32_t constant;
            if (i < 20) {
                function = (b & c) | ((~b) & d);
                constant = 0x5A827999;
            } else if (i < 40) {
                function = b ^ c ^ d;
                constant = 0x6ED9EBA1;
            } else if (i < 60) {
                function = (b & c) | (b & d) | (c & d);
                constant = 0x8F1BBCDC;
            } else {
                function = b ^ c ^ d;
                constant = 0xCA62C1D6;
            }

            const std::uint32_t rotatedA = (a << 5) | (a >> 27);
            const std::uint32_t next = rotatedA + function + e + constant + words[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = next;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    const std::array<std::uint32_t, 5> hash{h0, h1, h2, h3, h4};
    std::array<std::uint8_t, 20> result{};
    for (std::size_t i = 0; i < hash.size(); ++i) {
        result[i * 4] = static_cast<std::uint8_t>(hash[i] >> 24);
        result[i * 4 + 1] = static_cast<std::uint8_t>(hash[i] >> 16);
        result[i * 4 + 2] = static_cast<std::uint8_t>(hash[i] >> 8);
        result[i * 4 + 3] = static_cast<std::uint8_t>(hash[i]);
    }
    return result;
}

std::string base64(const std::uint8_t* data, std::size_t length) {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((length + 2) / 3) * 4);
    for (std::size_t i = 0; i < length; i += 3) {
        const std::uint32_t first = data[i];
        const std::uint32_t second = i + 1 < length ? data[i + 1] : 0;
        const std::uint32_t third = i + 2 < length ? data[i + 2] : 0;
        const std::uint32_t value = (first << 16) | (second << 8) | third;
        result.push_back(alphabet[(value >> 18) & 0x3F]);
        result.push_back(alphabet[(value >> 12) & 0x3F]);
        result.push_back(i + 1 < length ? alphabet[(value >> 6) & 0x3F] : '=');
        result.push_back(i + 2 < length ? alphabet[value & 0x3F] : '=');
    }
    return result;
}

std::string websocketAcceptKey(std::string_view key) {
    std::string input(key);
    input += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const auto hash = sha1(input);
    return base64(hash.data(), hash.size());
}

bool performHandshake(SocketHandle client) {
    std::string request;
    std::array<char, 2048> buffer{};

    while (request.find("\r\n\r\n") == std::string::npos && request.size() < 8192) {
        if (!waitForSocket(client, true, 2000)) {
            return false;
        }
#ifdef _WIN32
        const int received = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
#else
        const ssize_t received = recv(client, buffer.data(), buffer.size(), 0);
#endif
        if (received <= 0) {
            return false;
        }
        request.append(buffer.data(), static_cast<std::size_t>(received));
    }

    constexpr std::string_view headerName = "Sec-WebSocket-Key:";
    const std::size_t headerPosition = request.find(headerName);
    if (headerPosition == std::string::npos) {
        return false;
    }

    const std::size_t valueStart = request.find_first_not_of(" \t", headerPosition + headerName.size());
    if (valueStart == std::string::npos) {
        return false;
    }
    const std::size_t valueEnd = request.find("\r\n", valueStart);
    if (valueEnd == std::string::npos) {
        return false;
    }

    const std::string acceptKey = websocketAcceptKey(
        std::string_view(request).substr(valueStart, valueEnd - valueStart));
    const std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
    return sendAll(client, response.data(), response.size());
}

bool sendTextFrame(SocketHandle client, const std::string& message) {
    std::vector<char> frame;
    frame.reserve(message.size() + 10);
    frame.push_back(static_cast<char>(0x81));

    if (message.size() <= 125) {
        frame.push_back(static_cast<char>(message.size()));
    } else if (message.size() <= 65535) {
        frame.push_back(static_cast<char>(126));
        frame.push_back(static_cast<char>((message.size() >> 8) & 0xFF));
        frame.push_back(static_cast<char>(message.size() & 0xFF));
    } else {
        frame.push_back(static_cast<char>(127));
        const std::uint64_t length = message.size();
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<char>(length >> (i * 8)));
        }
    }

    frame.insert(frame.end(), message.begin(), message.end());
    return sendAll(client, frame.data(), frame.size());
}

}

WebSocketServer::WebSocketServer(unsigned short port)
    : port(port), listener(InvalidSocket), running(false), socketSubsystemInitialized(false) {}

WebSocketServer::~WebSocketServer() {
    stop();
}

void WebSocketServer::start() {
    if (running) {
        return;
    }

#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
    socketSubsystemInitialized = true;
#endif

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == InvalidSocket) {
#ifdef _WIN32
        WSACleanup();
        socketSubsystemInitialized = false;
#endif
        throw std::runtime_error("Unable to create WebSocket listener");
    }

    int reuseAddress = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuseAddress), sizeof(reuseAddress));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(listener, 1) != 0) {
        closeSocket(listener);
        listener = InvalidSocket;
#ifdef _WIN32
        WSACleanup();
        socketSubsystemInitialized = false;
#endif
        throw std::runtime_error("Unable to bind WebSocket server to 127.0.0.1:" + std::to_string(port));
    }

    running = true;
    worker = std::thread(&WebSocketServer::run, this);
}

void WebSocketServer::publish(std::string message) {
    std::lock_guard<std::mutex> lock(messageMutex);
    latestMessage = std::move(message);
}

void WebSocketServer::stop() {
    running = false;
    if (worker.joinable()) {
        worker.join();
    }
    if (listener != InvalidSocket) {
        closeSocket(listener);
        listener = InvalidSocket;
    }
    if (socketSubsystemInitialized) {
#ifdef _WIN32
        WSACleanup();
#endif
        socketSubsystemInitialized = false;
    }
}

void WebSocketServer::run() {
    while (running) {
        if (!waitForSocket(listener, true, 100)) {
            continue;
        }

        sockaddr_in clientAddress{};
        SocketLength addressLength = sizeof(clientAddress);
        const SocketHandle client = accept(listener, reinterpret_cast<sockaddr*>(&clientAddress), &addressLength);
        if (client == InvalidSocket) {
            continue;
        }

        if (performHandshake(client)) {
            while (running) {
                if (waitForSocket(client, true, 0)) {
                    std::array<char, 512> receivedData{};
#ifdef _WIN32
                    const int received = recv(client, receivedData.data(), static_cast<int>(receivedData.size()), 0);
#else
                    const ssize_t received = recv(client, receivedData.data(), receivedData.size(), 0);
#endif
                    if (received <= 0) {
                        break;
                    }
                    if ((static_cast<unsigned char>(receivedData[0]) & 0x0F) == 0x08) {
                        break;
                    }
                }

                std::string message;
                {
                    std::lock_guard<std::mutex> lock(messageMutex);
                    message = latestMessage;
                }

                if (!message.empty() && !sendTextFrame(client, message)) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }

        shutdownSocket(client);
        closeSocket(client);
    }
}
