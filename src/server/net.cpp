#include "net.hpp"

#include <iostream>

bool send_all(SOCKET socket, const std::string& reply) {
  const int reply_size = static_cast<int>(reply.size());
  int total_sent = 0;

  while (total_sent < reply_size) {
    int sent = send(socket,
                    reply.data() + total_sent,
                    reply_size - total_sent,
                    0);

    if (sent == SOCKET_ERROR) {
      std::cerr << "Send failed: " << WSAGetLastError() << '\n';
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
