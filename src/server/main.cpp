#include<winsock2.h>
#include <iostream>
#include <string>

#pragma comment(lib,"ws2_32.lib")
const int PORT = 2024;

void handleError(bool err, const char* msg) {
  if (!err) {
    return;
  }
  std::cerr << msg << WSAGetLastError() << std::endl;
  exit(EXIT_FAILURE);
}

int main(int argc, char* const argv[]) {
  WSADATA wsa;
  int res = WSAStartup(MAKEWORD(2, 2), &wsa);
  handleError(res != 0, "WSAStartup failed with error: ");

  SOCKET server_sock = socket(AF_INET, SOCK_STREAM, 0);
  handleError(server_sock == INVALID_SOCKET, "Socket creation failed with error: ");

  std::cout << "Socket created successfully" << std::endl;

  struct sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  res = bind(server_sock, (struct sockaddr*)&address, sizeof(address));
  handleError(res == SOCKET_ERROR, "Bind failed with error: ");
  std::cout << "Bind done" << std::endl;

  res = listen(server_sock, 3);
  if (res == SOCKET_ERROR) {
    std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
    closesocket(server_sock);
    WSACleanup();
    return 1;
  }

  while (true) {
    std::cout << "Waiting for a client...\n";

    SOCKET client_socket = accept(server_sock, nullptr, nullptr);

    if (client_socket == INVALID_SOCKET) {
      std::cerr << "Accept failed: " << WSAGetLastError() << '\n';
      break;
    }

    std::cout << "Client connected\n";

    std::string player_name;
    bool ready = false;
    std::string pending;
    bool connected = true;

    while (connected) {
      char buf[256];
      int received = recv(client_socket,
                          buf,
                          sizeof(buf),
                          0  
                        );
      if (received == SOCKET_ERROR) {
        std::cerr << "Receive failed: " << WSAGetLastError() << '\n';
        break;
      }
      if (received == 0) {
        if (pending.empty()) {
          std::cout << "Client disconnected\n";
        } else {
          std::cout << "Client disconnected before completing the message\n";
        }
        break;
      }

      pending.append(buf, received);
      
      while (true) {
        size_t pos = pending.find('\n');
        size_t line_size = (pos == std::string::npos) ? pending.size() : pos + 1;

        if (line_size > 1024) {
          std::cerr << "Message too long, closing connection\n";
          connected = false;
          break;
        }
        if (pos == std::string::npos) {
          break;
        }

        std::string line = pending.substr(0, pos);
        pending.erase(0, pos + 1);
        std::cout << "Received message: " << line << std::endl;

        std::string reply;

        if (line.starts_with("JOIN") && line.size() > 5) {
          if (player_name.empty()) {
            player_name = line.substr(5);
            reply = "OK JOIN\n";
          } else {
            reply = "ERR_ALREADY_JOINED\n";
          }
        } else if (line == "READY") {
          if (player_name.empty()) {
            reply = "ERR_JOIN_FIRST\n";
          } else {
            ready = true;
            reply = "OK READY\n";
          }
        } else {
          reply = "ERR_UNKNOWN_COMMAND\n";
        }

        const int reply_size = static_cast<int>(reply.size());
        int total_sent = 0;

        while (total_sent < reply_size) {
          int sent = send(client_socket,
                          reply.data() + total_sent,
                          reply_size - total_sent,
                          0);

          if (sent == SOCKET_ERROR) {
            std::cerr << "Send failed: " << WSAGetLastError() << '\n';
            break;
          }

          if (sent == 0) {
            std::cerr << "Could not send the remaining bytes\n";
            break;
          }

          total_sent += sent;
        }

        if (total_sent != reply_size) {
          connected = false;
          break;
        }
        std::cout << "Reply sent\n";
      }
    }

    closesocket(client_socket);
    std::cout << "Connection closed\n\n";
  }
  
  closesocket(server_sock);
  WSACleanup();
  return 0;
}