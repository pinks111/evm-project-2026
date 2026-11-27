#pragma once

#include "../common/socket_compat.hpp"

#include <string>

bool send_all(SOCKET socket, const std::string& reply);
