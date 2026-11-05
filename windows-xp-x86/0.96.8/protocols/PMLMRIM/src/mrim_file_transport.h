#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
namespace pmlmrim {
// The MRIM FT data channel is a byte stream: read exactly the advertised
// number of bytes, never rely on EOF to delimit a file.
using ReadBytes = std::function<int(char*, size_t)>;
using WriteBytes = std::function<int(const char*, size_t)>;
using ProgressBytes = std::function<void(uint64_t)>;
using IsCancelled = std::function<bool()>;
void sendAll(const WriteBytes& write, const char* data, size_t size,
             const IsCancelled& cancelled = {});
std::string receiveCommand(const ReadBytes& read, size_t maxSize=1024,
                           const IsCancelled& cancelled = {});
void sendFileBody(const ReadBytes& source, const WriteBytes& socket,
                  uint64_t size, const ProgressBytes& progress = {},
                  const IsCancelled& cancelled = {});
void receiveFileBody(const ReadBytes& socket, const WriteBytes& destination,
                     uint64_t size, const ProgressBytes& progress = {},
                     const IsCancelled& cancelled = {});
}
