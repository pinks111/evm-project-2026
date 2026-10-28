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
    std::string message;
    bool completed = false;

    while (true) {
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
        std::cout << "Client disconnected before completing the message\n";
        break;
      }

      message.append(buf, received);
      size_t end = message.find("\n");
      
      std::size_t msg_size = (end == std::string::npos) ? message.size() : end;

      if (msg_size > 1024) {
        std::cerr << "Message is too long\n";
      break;
      }

      if (end != std::string::npos) {
        message.resize(end);
        completed = true;
        break;
      }
    }

    if (completed) {
      std::cout << "Message from client: " << message << '\n';

      const char reply[] = "hello";
      constexpr int reply_size = sizeof(reply) - 1;
      int total_sent = 0;

      while (total_sent < reply_size) {
        int sent = send(client_socket,
                        reply + total_sent,
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

      if (total_sent == reply_size) {
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