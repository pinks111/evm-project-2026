#include "net.hpp"

#include <iostream>

bool send_all(SOCKET socket, const std::string& reply) {
  const int reply_size = static_cast<int>(reply.size());
  int total_sent = 0;

  while (total_sent < reply_size) {
    const std::ptrdiff_t sent = send_socket(
        socket,
        reply.data() + total_sent,
        static_cast<std::size_t>(reply_size - total_sent)
    );

    if (sent < 0) {
      std::cerr << "Send failed: " << socket_last_error() << '\n';
      return false;
    }

    if (sent == 0) {
      std::cerr << "Could not send the remaining bytes\n";
      return false;
    }

    total_sent += sent;
  }
  return true;
}
