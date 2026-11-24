#pragma once

#include <winsock2.h>
#include <string>

bool send_all(SOCKET socket, const std::string& reply);
