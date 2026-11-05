#pragma once
// MRIM 1.15+ extended status codec. Server capability negotiation is required.
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>
namespace pmlmrim_xstatus {
using Bytes=std::vector<uint8_t>;
struct State { uint32_t status=4; std::string uri,title,description; uint32_t features=0x02df; };
inline void u32(Bytes& b,uint32_t v){for(int i=0;i<4;i++)b.push_back(uint8_t(v>>(i*8)));}
inline void str(Bytes& b,const std::string& s){if(s.size()>1048576)throw std::length_error("MRIM string too large");u32(b,uint32_t(s.size()));b.insert(b.end(),s.begin(),s.end());}
inline uint32_t read32(const Bytes& b,size_t& p){if(p>b.size()||b.size()-p<4)throw std::runtime_error("truncated u32");uint32_t v=uint32_t(b[p])|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);p+=4;return v;}
inline std::string readstr(const Bytes& b,size_t& p){uint32_t n=read32(b,p);if(n>1048576||p>b.size()||n>b.size()-p)throw std::runtime_error("invalid string");std::string s(b.begin()+p,b.begin()+p+n);p+=n;return s;}
// Strings are pre-encoded according to negotiated capabilities. PMLMRIM
// currently sends CP1251 title/description (no UTF16CAPABLE negotiation).
inline Bytes makeChangeBody(const State& s){Bytes b;u32(b,s.status);str(b,s.uri);str(b,s.title);str(b,s.description);Bytes featureBytes;u32(featureBytes,s.features);u32(b,4);b.insert(b.end(),featureBytes.begin(),featureBytes.end());return b;}
// Wrap the extended body into a standard 44-byte MRIM packet with caller-selected
// protocol version. The caller must only use this with negotiated 1.15+ peers.
inline Bytes makeChangePacket(uint32_t sequence, const State& state,
                              uint16_t major=1, uint16_t minor=15) {
    const auto body = makeChangeBody(state);
    Bytes out;
    u32(out, 0xDEADBEEFu);
    out.push_back(uint8_t(minor)); out.push_back(uint8_t(minor>>8));
    out.push_back(uint8_t(major)); out.push_back(uint8_t(major>>8));
    u32(out, sequence);
    u32(out, 0x1022); // MRIM_CS_CHANGE_STATUS
    u32(out, uint32_t(body.size()));
    u32(out,0); u32(out,0);
    for (int i=0;i<16;++i) out.push_back(0);
    out.insert(out.end(),body.begin(),body.end());
    return out;
}
struct ContactUpdate {State state;std::string email,userAgent;};
inline ContactUpdate parseContactBody(const Bytes& b){size_t p=0;ContactUpdate v;v.state.status=read32(b,p);v.state.uri=readstr(b,p);v.state.title=readstr(b,p);v.state.description=readstr(b,p);v.email=readstr(b,p);const std::string capabilities=readstr(b,p);if(capabilities.size()!=4)throw std::runtime_error("invalid features LPS");size_t featurePos=0;v.state.features=read32(Bytes(capabilities.begin(),capabilities.end()),featurePos);v.userAgent=readstr(b,p);if(p!=b.size())throw std::runtime_error("trailing data");return v;}
} // namespace pmlmrim_xstatus
