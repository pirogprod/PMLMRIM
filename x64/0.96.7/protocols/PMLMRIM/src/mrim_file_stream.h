#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace pmlmrim {
struct TransferItem { std::string name; uint32_t size; };
// Pure wire helpers; no Miranda UI or socket ownership.
std::vector<TransferItem> parseFileList(const std::string& list);
std::string makeFileList(const std::vector<TransferItem>& items);
std::string makeFileHello(const std::string& email);
std::string makeFileGet(const std::string& filename);
bool matchesFileHello(const std::string& wire, const std::string& email);
bool matchesFileGet(const std::string& wire, const std::string& filename);
}
