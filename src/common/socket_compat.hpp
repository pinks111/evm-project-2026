#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

using SOCKET = int;
inline constexpr SOCKET INVALID_SOCKET = -1;
inline constexpr int SOCKET_ERROR = -1;
#endif

#include <cstddef>

inline int socket_last_error()
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

inline int initialize_socket_runtime()
{
#ifdef _WIN32
    WSADATA data{};
    return WSAStartup(MAKEWORD(2, 2), &data);
#else
    return 0;
#endif
}

inline void cleanup_socket_runtime()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

inline void close_socket(SOCKET socket)
{
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

inline int wait_for_socket(
    SOCKET highestSocket,
    fd_set* readSet,
    timeval* timeout = nullptr
)
{
#ifdef _WIN32
    (void)highestSocket;
    return select(0, readSet, nullptr, nullptr, timeout);
#else
    return select(highestSocket + 1, readSet, nullptr, nullptr, timeout);
#endif
}

inline bool configure_socket_no_sigpipe(SOCKET socket)
{
#if !defined(_WIN32) && defined(SO_NOSIGPIPE)
    const int enabled = 1;
    return setsockopt(
        socket,
        SOL_SOCKET,
        SO_NOSIGPIPE,
        &enabled,
        sizeof(enabled)
    ) == 0;
#else
    (void)socket;
    return true;
#endif
}

inline std::ptrdiff_t send_socket(
    SOCKET socket,
    const char* data,
    std::size_t length
)
{
#ifdef _WIN32
    return send(socket, data, static_cast<int>(length), 0);
#else
    int flags = 0;
#ifdef MSG_NOSIGNAL
    flags |= MSG_NOSIGNAL;
#endif
    return send(socket, data, length, flags);
#endif
}

inline std::ptrdiff_t receive_socket(
    SOCKET socket,
    char* data,
    std::size_t length
)
{
#ifdef _WIN32
    return recv(socket, data, static_cast<int>(length), 0);
#else
    return recv(socket, data, length, 0);
#endif
}
