#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>

namespace pmlmrim {
using Bytes = std::vector<uint8_t>;
constexpr uint32_t MAGIC = 0xDEADBEEF;
constexpr size_t HEADER_SIZE = 44;
enum Command : uint32_t { HELLO=0x1001, HELLO_ACK=0x1002, LOGIN=0x1003, LOGIN2=0x1038, LOGIN_ACK=0x1004, LOGIN_REJ=0x1005, PING=0x1006, MESSAGE=0x1008, MESSAGE_ACK=0x1009, MESSAGE_RECV=0x1011, CONTACT_LIST=0x100A, CONTACT_LIST_ACK=0x100B, CONTACT_LIST2=0x1037, ADD_CONTACT=0x1019, ADD_CONTACT_ACK=0x101A, AUTHORIZE=0x1020, AUTHORIZE_ACK=0x1021, FILE_TRANSFER=0x1026, FILE_TRANSFER_ACK=0x1027, ANKETA_INFO=0x1028, WP_REQUEST=0x1029, USER_STATUS=0x100F, MESSAGE_STATUS=0x1012, CHANGE_STATUS=0x1022 };
enum Status : uint32_t { OFFLINE=0, ONLINE=1, AWAY=2, INVISIBLE=0x80000001u };
struct Packet { uint16_t major=1, minor=15; uint32_t sequence=0, command=0; Bytes body; };
void put16(Bytes&, uint16_t);
void put32(Bytes&, uint32_t);
void putString(Bytes&, const std::string&);
Bytes encode(const Packet&);
bool decodeOne(Bytes& incoming, Packet& out); // false = incomplete; retains buffer
Bytes makeHello(uint32_t sequence, uint16_t minor=15);
Bytes makeLogin(uint32_t sequence, const std::string& email, const std::string& password, uint32_t status=ONLINE,
 const std::string& uri = {}, const std::string& title = {}, const std::string& description = {});
Bytes makeStatus(uint32_t sequence, uint32_t status);
Bytes makePing(uint32_t sequence);
}
