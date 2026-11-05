#include "mrim_wire.h"
#include <algorithm>
namespace pmlmrim {
void put16(Bytes& b, uint16_t x) { b.push_back(uint8_t(x)); b.push_back(uint8_t(x>>8)); }
void put32(Bytes& b, uint32_t x) { for(int i=0;i<4;++i) b.push_back(uint8_t(x>>(i*8))); }
void putString(Bytes& b, const std::string& s) { if(s.size()>0x100000) throw std::length_error("MRIM string too long"); put32(b,uint32_t(s.size())); b.insert(b.end(),s.begin(),s.end()); }
static uint16_t rd16(const uint8_t* p) { return uint16_t(p[0]) | (uint16_t(p[1])<<8); }
static uint32_t rd32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1])<<8) | (uint32_t(p[2])<<16) | (uint32_t(p[3])<<24); }
Bytes encode(const Packet& p) {
 if(p.body.size()>4*1024*1024) throw std::length_error("MRIM packet too long");
 Bytes b; b.reserve(HEADER_SIZE+p.body.size()); put32(b,MAGIC); put16(b,p.minor); put16(b,p.major);
 put32(b,p.sequence); put32(b,p.command); put32(b,uint32_t(p.body.size()));
 put32(b,0); put32(b,0); b.insert(b.end(),16,0); b.insert(b.end(),p.body.begin(),p.body.end()); return b;
}
bool decodeOne(Bytes& in, Packet& p) {
 if(in.size()<HEADER_SIZE) return false;
 if(rd32(in.data())!=MAGIC) throw std::runtime_error("invalid MRIM magic");
 const uint32_t len=rd32(in.data()+16);
 if(len>4*1024*1024) throw std::runtime_error("MRIM packet exceeds limit");
 if(in.size()<HEADER_SIZE+size_t(len)) return false;
 Packet q; q.minor=rd16(in.data()+4); q.major=rd16(in.data()+6);
 q.sequence=rd32(in.data()+8); q.command=rd32(in.data()+12);
 q.body.assign(in.begin()+HEADER_SIZE,in.begin()+HEADER_SIZE+len);
 in.erase(in.begin(),in.begin()+HEADER_SIZE+len); p=std::move(q); return true;
}
Bytes makeHello(uint32_t seq, uint16_t minor) { Packet p; p.minor=minor; p.sequence=seq; p.command=HELLO; return encode(p); }
Bytes makeLogin(uint32_t seq, const std::string& email, const std::string& password, uint32_t status,
 const std::string& uri, const std::string& title, const std::string& description) {
 // MRIM 1.15 (MRA 5.0) requires LOGIN2 with X-status and capabilities.
 Packet p; p.sequence=seq; p.command=LOGIN2;
 putString(p.body,email); putString(p.body,password); put32(p.body,status);
 putString(p.body,uri.empty() ? (status==AWAY ? "STATUS_AWAY" : "STATUS_ONLINE") : uri);
 putString(p.body,title); putString(p.body,description);
 put32(p.body,0x02DFu); // capabilities are UL in LOGIN2, unlike CHANGE_STATUS
 putString(p.body,"client=\"pmlmrim\" version=\"5.0\" build=\"2000\"");
 putString(p.body,"MRA 5.0 (build 2000);");
 return encode(p);
}
Bytes makeStatus(uint32_t seq,uint32_t status) { Packet p; p.sequence=seq; p.command=CHANGE_STATUS; put32(p.body,status); return encode(p); }
Bytes makePing(uint32_t seq) { Packet p; p.sequence=seq; p.command=PING; return encode(p); }
}
