#pragma once
#include "mrim_wire.h"
#include <string>
#include <map>
namespace pmlmrim {
struct RosterContact { std::string email, nickname, groupName; uint32_t status=0, flags=0, group=0; std::string xstatusUri, xstatusTitle, xstatusDescription; };
std::vector<RosterContact> parseRoster(const Packet&);
Bytes makeAddContact(uint32_t seq, const std::string& email);
Bytes makeAuthorizationGrant(uint32_t seq, const std::string& email);
Bytes makeAuthorizationRequest(uint32_t seq, const std::string& email, const std::string& reason);
struct HelloAck { uint32_t pingSeconds=0; };
struct StatusUpdate { uint32_t status=0; std::string email, xstatusUri, xstatusTitle, xstatusDescription, userAgent; };
struct IncomingMessage { uint32_t id=0, flags=0; std::string sender, text; size_t rawTextBytes=0, rawRtfBytes=0; };
HelloAck parseHelloAck(const Packet&);
StatusUpdate parseStatusUpdate(const Packet&);
IncomingMessage parseIncomingMessage(const Packet&);
std::string parseLoginRejection(const Packet&);
std::string mrimTextToUtf8(const std::string&);
std::string utf8ToMrimText(const std::string&);
std::string utf8ToMrimUnicode(const std::string&);
Bytes makeMessageReceived(uint32_t sequence, uint32_t messageId, const std::string& sender);
Bytes makeTextMessage(uint32_t sequence, const std::string& recipient, const std::string& utf8Text, bool preferUtf16 = true);
}

namespace pmlmrim {
struct ProfileResult { uint32_t status=0; std::map<std::string,std::string> fields; };
Bytes makeProfileRequest(uint32_t sequence, const std::string& email);
ProfileResult parseProfileResponse(const Packet& packet);
}

// MRIM file-transfer control packets (transport and Miranda UI are separate).
namespace pmlmrim {
struct FileOffer { std::string peer, files, extra, addresses; uint32_t id=0, totalSize=0; };
struct FileOfferAck { uint32_t status=0, id=0; std::string peer, addresses; };
Bytes makeFileOffer(uint32_t seq, const FileOffer& offer);
Bytes makeFileOfferAck(uint32_t seq, const FileOfferAck& ack);
FileOffer parseFileOffer(const Packet& packet);
FileOfferAck parseFileOfferAck(const Packet& packet);
}
