#pragma once
#include <winsock2.h>
#include <string>
#include <vector>
#include <cstdint>

namespace pmlmrim {
struct FileEndpoint {
 std::string ipv4;
 uint16_t port = 0;
};
// An address string from an untrusted MRIM offer; numeric IPv4 only.
std::vector<FileEndpoint> parseFileEndpoints(const std::string& addresses);
// Caller must initialize Winsock. Returned sockets belong to the caller.
SOCKET connectFileEndpoint(const FileEndpoint& address, unsigned timeoutMs = 6000);
SOCKET connectFileEndpoints(const std::string& addresses, unsigned timeoutMs = 6000);
// Bind an IPv4 TCP listener to any local interface, port 0 (system-assigned).
SOCKET listenForFiles(uint16_t& boundPort);
// Limit how long an accepted file socket can block on I/O.
bool setFileSocketTimeout(SOCKET socket, unsigned timeoutMs = 30000);
}
