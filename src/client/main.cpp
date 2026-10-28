#include<winsock2.h>
#include <iostream>
#include <WS2tcpip.h>
#include <cstring>
#include <cstdlib>

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
  std::cout << "Initialising Winsock...\n";
  WSADATA wsa;
  int res = WSAStartup(MAKEWORD(2, 2), &wsa);
  if (res != 0) {
    std::cerr << "WSAStartup failed: " << res << '\n';
    return 1;
  }

  char buffer[1024] = {0};
  SOCKET client_sock = socket(AF_INET, SOCK_STREAM, 0);
	handleError(client_sock == INVALID_SOCKET, "Could not create socket : ");

  struct sockaddr_in servaddr{};
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(PORT);

  int address_result = inet_pton(
    AF_INET,
    "127.0.0.1",
    &servaddr.sin_addr
  );

  if (address_result != 1) {
    std::cerr << "Invalid address\n";
    closesocket(client_sock);
    WSACleanup();
    return 1;
  }

  int status = connect(
    client_sock,
    (struct sockaddr*)&servaddr,
    sizeof(servaddr)
  );
  handleError(status == SOCKET_ERROR, "Connection failed with error: ");

  const char message[] = "Hello from client\n";
  send(client_sock, message, sizeof(message) - 1, 0);
  std::cout << "Message sent to server\n";
  int valread = recv(client_sock, buffer, sizeof(buffer), 0);

  if (valread == SOCKET_ERROR) {
    std::cerr << "Receive failed: " << WSAGetLastError() << '\n';
  } else if (valread == 0) {
    std::cout << "Server closed the connection\n";
  } else {
    std::cout << "Message from server: ";
    std::cout.write(buffer, valread);
    std::cout << '\n';
  }
  closesocket(client_sock);
	WSACleanup();
	return 0;
}