#include "mrim_session.h"
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif
namespace pmlmrim {
namespace {
uint32_t u32(const Bytes& b,size_t& i){ if(i>b.size()||b.size()-i<4) throw std::runtime_error("truncated MRIM uint32"); uint32_t v=uint32_t(b[i])|(uint32_t(b[i+1])<<8)|(uint32_t(b[i+2])<<16)|(uint32_t(b[i+3])<<24); i+=4; return v; }
std::string str(const Bytes& b,size_t& i){ auto n=u32(b,i); if(n>1024*1024||i>b.size()||n>b.size()-i) throw std::runtime_error("invalid MRIM string length"); std::string s(b.begin()+i,b.begin()+i+n); i+=n; return s; }
std::string utf16leToUtf8(const std::string& bytes){
 if(bytes.size()%2) throw std::runtime_error("odd UTF16LE byte length");
 std::string out;
 for(size_t i=0;i<bytes.size();i+=2){
  uint32_t c=(uint8_t)bytes[i]|(uint32_t(uint8_t(bytes[i+1]))<<8);
  if(c>=0xD800&&c<=0xDBFF){ if(i+3>=bytes.size()) throw std::runtime_error("truncated surrogate pair"); uint32_t low=(uint8_t)bytes[i+2]|(uint32_t(uint8_t(bytes[i+3]))<<8); if(low<0xDC00||low>0xDFFF) throw std::runtime_error("bad surrogate pair"); c=0x10000+((c-0xD800)<<10)+(low-0xDC00); i+=2; }
  else if(c>=0xDC00&&c<=0xDFFF) throw std::runtime_error("lone low surrogate");
  if(c<0x80) out.push_back(char(c)); else if(c<0x800){out.push_back(char(0xC0|(c>>6)));out.push_back(char(0x80|(c&63)));}
  else if(c<0x10000){out.push_back(char(0xE0|(c>>12)));out.push_back(char(0x80|((c>>6)&63)));out.push_back(char(0x80|(c&63)));}
  else {out.push_back(char(0xF0|(c>>18)));out.push_back(char(0x80|((c>>12)&63)));out.push_back(char(0x80|((c>>6)&63)));out.push_back(char(0x80|(c&63)));}
 }
 return out;
}
std::string utf8ToUtf16le(const std::string& s){
 std::string out; size_t i=0;
 auto emit=[&](uint16_t x){out.push_back(char(x));out.push_back(char(x>>8));};
 while(i<s.size()){
  unsigned char b=(unsigned char)s[i++]; uint32_t c; unsigned need;
  if(b<0x80){c=b;need=0;}else if((b&0xE0)==0xC0){c=b&31;need=1;}else if((b&0xF0)==0xE0){c=b&15;need=2;}else if((b&0xF8)==0xF0){c=b&7;need=3;}else throw std::runtime_error("bad UTF8 lead");
  for(unsigned k=0;k<need;k++){if(i>=s.size()||((unsigned char)s[i]&0xC0)!=0x80) throw std::runtime_error("bad UTF8 continuation"); c=(c<<6)|((unsigned char)s[i++]&63);}
  if((need==1&&c<0x80)||(need==2&&c<0x800)||(need==3&&c<0x10000)||c>0x10FFFF||(c>=0xD800&&c<=0xDFFF)) throw std::runtime_error("invalid UTF8 codepoint");
  if(c<0x10000)emit(uint16_t(c));else{c-=0x10000;emit(uint16_t(0xD800+(c>>10)));emit(uint16_t(0xDC00+(c&1023)));}
 }
 return out;
}
}
// MRIM 1.13 uses CP1251 for UNICODE_STRING unless UTF16CAPABLE was negotiated.
// This client does not negotiate UTF16CAPABLE, so UTF-16LE would corrupt Cyrillic.
std::string mrimTextToUtf8(const std::string& raw) {
#ifdef _WIN32
 if(raw.empty()) return {};
 int n=MultiByteToWideChar(1251,0,raw.data(),(int)raw.size(),nullptr,0);
 if(n<=0) throw std::runtime_error("CP1251 decode failed");
 std::wstring wide(n,L'\0'); MultiByteToWideChar(1251,0,raw.data(),(int)raw.size(),&wide[0],n);
 int len=WideCharToMultiByte(CP_UTF8,0,wide.data(),n,nullptr,0,nullptr,nullptr);
 std::string out(len,'\0'); WideCharToMultiByte(CP_UTF8,0,wide.data(),n,&out[0],len,nullptr,nullptr);
 return out;
#else
 return raw; // Windows Miranda build uses Win32 conversion above.
#endif
}
// Optional UTF-16LE conversion helper. Do not use for extended statuses
// unless UTF16CAPABLE was negotiated; this client currently uses CP1251.
std::string utf8ToMrimUnicode(const std::string& text) {
#ifdef _WIN32
 if (text.empty()) return {};
 const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),nullptr,0);
 if (n<=0) throw std::runtime_error("invalid X-status UTF-8");
 std::wstring wide(n,L'\0');
 MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),&wide[0],n);
 std::string result(size_t(n)*2,'\0');
 for (int i=0;i<n;++i) {result[size_t(i)*2]=char(wide[i]&0xff);result[size_t(i)*2+1]=char(wide[i]>>8);}
 return result;
#else
 return text;
#endif
}
std::string utf8ToMrimText(const std::string& text) {
#ifdef _WIN32
 if(text.empty()) return {};
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),nullptr,0);
 if(n<=0) throw std::runtime_error("invalid UTF-8");
 std::wstring wide(n,L'\0'); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),&wide[0],n);
 // Do not silently turn characters outside Windows-1251 into '?'.
 BOOL usedDefault=FALSE;
 int len=WideCharToMultiByte(1251,WC_NO_BEST_FIT_CHARS,wide.data(),n,nullptr,0,nullptr,&usedDefault);
 if(len<=0 || usedDefault) throw std::runtime_error("text not representable in CP1251");
 std::string out(len,'\0');
 usedDefault=FALSE;
 if(!WideCharToMultiByte(1251,WC_NO_BEST_FIT_CHARS,wide.data(),n,&out[0],len,nullptr,&usedDefault) || usedDefault)
  throw std::runtime_error("CP1251 conversion failed");
 return out;
#else
 return text;
#endif
}
HelloAck parseHelloAck(const Packet& p){if(p.command!=HELLO_ACK)throw std::runtime_error("expected HELLO_ACK");size_t i=0;return {u32(p.body,i)};}
StatusUpdate parseStatusUpdate(const Packet& p){
 if(p.command!=USER_STATUS)throw std::runtime_error("expected USER_STATUS");
 size_t i=0; StatusUpdate s; s.status=u32(p.body,i);
 // MRIM 1.14+: status, URI, title, description, email, UL features, LPS agent.
 // Older servers may still send only status + email even with a newer header.
 if(p.major==1 && p.minor>=14){
  size_t start=i;
  try {
   const std::string uri=str(p.body,i);
   const std::string title=str(p.body,i);
   const std::string description=str(p.body,i);
   std::string email=str(p.body,i);
   if(email.find('@')==std::string::npos)throw std::runtime_error("invalid extended presence address");
   s.email=std::move(email); s.xstatusUri=uri; s.xstatusTitle=mrimTextToUtf8(title); s.xstatusDescription=mrimTextToUtf8(description);
   // The capabilities field on known MRIM servers is a uint32, but
   // some implementations wrap those four bytes in an LPS. Accept both.
   if(i<p.body.size()) {
    size_t optional=i;
    try {
     if(p.body.size()-optional>=4) {
      const uint32_t capabilities=u32(p.body,optional);
      // Alternate wire format: LPS(4-byte capabilities), then LPS(User-Agent).
      if(capabilities==4 && p.body.size()-optional>=4) optional+=4;
      if(optional<p.body.size()) {
       const std::string agent=str(p.body,optional);
       if(!agent.empty() && agent.size()<=512 && optional==p.body.size())
        s.userAgent=mrimTextToUtf8(agent);
      }
     }
    } catch(const std::exception&) { /* No usable User-Agent field. */ }
   }
   return s;
  }catch(const std::exception&){i=start;}
 }
 s.email=str(p.body,i);
 if(s.email.find('@')==std::string::npos)throw std::runtime_error("invalid presence address");
 return s;
}
IncomingMessage parseIncomingMessage(const Packet& p){if(p.command!=MESSAGE_ACK)throw std::runtime_error("expected MESSAGE_ACK");size_t i=0;IncomingMessage m;m.id=u32(p.body,i);m.flags=u32(p.body,i);m.sender=str(p.body,i);
 std::string rawText=str(p.body,i);
 m.rawTextBytes=rawText.size();
 // MRIM v1.16 flag: some peers send UTF-16LE even if our own login uses CP1251.
 // The Unicode message flag applies to the message body, not to sender or RTF.
 m.text=(m.flags&0x00100000u)?utf16leToUtf8(rawText):mrimTextToUtf8(rawText);
 std::string rawRtf=str(p.body,i);
 m.rawRtfBytes=rawRtf.size();
 return m;}
std::string parseLoginRejection(const Packet& p){if(p.command!=LOGIN_REJ)throw std::runtime_error("expected LOGIN_REJ");size_t i=0;return str(p.body,i);}
Bytes makeTextMessage(uint32_t seq,const std::string& recipient,const std::string& text, bool preferUtf16){
 Packet p; p.sequence=seq; p.command=MESSAGE;
 // Mobile Agent (MRIM 1.21) often expects the Unicode message bit. Send
 // UTF-16LE end-to-end, rather than relying on an intermediary server to
 // convert the older CP1251 MESSAGE payload for a UTF-16-capable recipient.
 // MessageEncoding=0 provides a legacy CP1251 mode for older peers.
 uint32_t flags=0;
 std::string wireText;
 if (preferUtf16) {
  wireText=utf8ToUtf16le(text);
  flags=0x00100000u;
 } else {
  wireText=utf8ToMrimText(text);
 }
 put32(p.body,flags);
 putString(p.body,recipient);
 putString(p.body,wireText);
 putString(p.body,"");
 return encode(p);
}
}

namespace pmlmrim {
Bytes makeMessageReceived(uint32_t seq, uint32_t messageId, const std::string& sender) { Packet p; p.sequence=seq; p.command=MESSAGE_RECV; put32(p.body,messageId); putString(p.body,sender); return encode(p); }
}

namespace pmlmrim {
// CONTACT_LIST2 is a status + group count + two field masks, followed by
// exactly groupCount groups and then contact records up to packet end.
std::vector<RosterContact> parseRoster(const Packet& p) {
 // MRIM <= 1.7 uses CONTACT_LIST_ACK (0x100B), not CONTACT_LIST2.
 // mrim-server encodes an 8-byte prefix (count, length), a 0xA00-byte
 // group block and fixed-size 0x100-byte contact records. Each record is
 // CP1251 text: "00000000 <8-hex-digit-group> email@domain <hexlen>nickname".
 if(p.command==CONTACT_LIST_ACK){
  size_t offset=0;
  const uint32_t declaredCount=u32(p.body,offset);
  const uint32_t declaredLength=u32(p.body,offset);
  (void)declaredCount; // mrim-server currently sets this field to zero.
  if(declaredLength>p.body.size()-offset)throw std::runtime_error("legacy roster length exceeds packet");
  if(declaredLength<0xA00)throw std::runtime_error("legacy roster missing group block");
  const size_t contactBytes=declaredLength-0xA00;
  if(contactBytes%0x100)throw std::runtime_error("legacy roster contact records misaligned");
  if(contactBytes/0x100>100000)throw std::runtime_error("legacy roster too large");
  offset+=0xA00;
  std::vector<RosterContact> contacts;
  for(size_t rec=0;rec<contactBytes/0x100;++rec,offset+=0x100){
   const char* start=reinterpret_cast<const char*>(p.body.data()+offset);
   std::string raw(start,0x100);
   const size_t terminator=raw.find('\0');
   if(terminator!=std::string::npos)raw.resize(terminator);
   // First two whitespace-delimited tokens are flags and group index.
   size_t pos=0;
   auto token=[&]()->std::string{
    while(pos<raw.size()&&(raw[pos]==' '||raw[pos]=='\t'))++pos;
    size_t begin=pos;
    while(pos<raw.size()&&raw[pos]!=' '&&raw[pos]!='\t')++pos;
    return raw.substr(begin,pos-begin);
   };
   const std::string flags=token(),group=token(),address=token();
   if(address.empty()||address.find('@')==std::string::npos)continue;
   RosterContact c;
   c.email=mrimTextToUtf8(address);
   try{c.flags=static_cast<uint32_t>(std::stoul(flags,nullptr,16));
       c.group=static_cast<uint32_t>(std::stoul(group,nullptr,16));}
   catch(...){continue;}
   while(pos<raw.size()&&(raw[pos]==' '||raw[pos]=='\t'))++pos;
   // Legacy nickname starts with a two-character hex length, followed by CP1251 bytes.
   if(pos+2<=raw.size()){
    try{
     size_t count=0;
     const unsigned long len=std::stoul(raw.substr(pos,2),&count,16);
     if(count==2&&len<=raw.size()-(pos+2))
      c.nickname=mrimTextToUtf8(raw.substr(pos+2,len));
    }catch(...){}
   }
   c.status=OFFLINE; // Online status updates arrive separately as USER_STATUS.
   contacts.push_back(std::move(c));
  }
  return contacts;
 }
 if(p.command!=CONTACT_LIST2) throw std::runtime_error("not a supported MRIM contact list");
 size_t i=0; const uint32_t status=u32(p.body,i);
 if(status!=0) throw std::runtime_error("MRIM roster status nonzero");
 const uint32_t groupCount=u32(p.body,i);
 if(groupCount>10000) throw std::runtime_error("MRIM group count too large");
 const std::string groupMask=str(p.body,i), contactMask=str(p.body,i);
 if(groupMask.size()>64||contactMask.size()>64||contactMask.empty()) throw std::runtime_error("invalid MRIM roster mask");
 // The masks are positional. 'u' means uint32, 's' means a length-prefixed
 // CP1251 string. The server's 1.13 mask is uussuus (the final phone field
 // must also be consumed to keep the next contact aligned).
 auto readFields=[&](const std::string& mask,RosterContact* contact) {
  for(size_t k=0;k<mask.size();++k){
   if(mask[k]=='u'){
    const uint32_t value=u32(p.body,i);
    if(contact){if(k==0)contact->flags=value;else if(k==1)contact->group=value;else if(k==5)contact->status=value;}
   }else if(mask[k]=='s'){
    const std::string value=str(p.body,i);
    if(contact){if(k==2)contact->email=mrimTextToUtf8(value);else if(k==3)contact->nickname=mrimTextToUtf8(value);else if(k==7)contact->xstatusUri=value;else if(k==8)contact->xstatusTitle=mrimTextToUtf8(value);else if(k==9)contact->xstatusDescription=mrimTextToUtf8(value);}
   }else throw std::runtime_error("unsupported MRIM roster mask character");
  }
 };
 for(uint32_t j=0;j<groupCount;++j)readFields(groupMask,nullptr);
 std::vector<RosterContact> contacts;
 while(i<p.body.size()){
  const size_t before=i; RosterContact c;readFields(contactMask,&c);
  if(i==before)throw std::runtime_error("zero-length MRIM roster record");
  // Keep every valid address, including contacts with server-side flags.
  // Hiding or filtering them is a UI policy, not a wire parser decision.
  if(!c.email.empty()&&c.email.find('@')!=std::string::npos)contacts.push_back(std::move(c));
  if(contacts.size()>100000)throw std::runtime_error("MRIM roster too large");
 }
 return contacts;
}
Bytes makeAddContact(uint32_t seq,const std::string& email){
 Packet p;p.sequence=seq;p.command=ADD_CONTACT;
 put32(p.body,0);put32(p.body,0);putString(p.body,email);
 putString(p.body,utf8ToMrimText(email));put32(p.body,0);putString(p.body,"");
 return encode(p);
}
}


namespace pmlmrim {
Bytes makeProfileRequest(uint32_t seq, const std::string& email) {
 auto at=email.find('@');
 if(at==std::string::npos || at==0 || at+1>=email.size()) throw std::runtime_error("invalid MRIM email");
 Packet p; p.sequence=seq; p.command=WP_REQUEST;
 put32(p.body,0); putString(p.body,email.substr(0,at));
 put32(p.body,1); putString(p.body,email.substr(at+1));
 return encode(p);
}
ProfileResult parseProfileResponse(const Packet& p) {
 if(p.command!=ANKETA_INFO)throw std::runtime_error("not ANKETA_INFO");
 size_t i=0; ProfileResult result;
 result.status=u32(p.body,i);
 // Some server errors consist only of the 4-byte status.
 if(p.body.size()==4)return result;
 uint32_t fieldCount=u32(p.body,i), rowCount=u32(p.body,i);
 (void)u32(p.body,i); // server timestamp
 if(fieldCount>128 || rowCount>1000)throw std::runtime_error("MRIM profile bounds exceeded");
 std::vector<std::string> names;names.reserve(fieldCount);
 for(uint32_t j=0;j<fieldCount;++j)names.push_back(str(p.body,i));
 // One user requested by login+domain. Consume other rows safely.
 for(uint32_t row=0;row<rowCount;++row)
  for(uint32_t col=0;col<fieldCount;++col){
   std::string value=str(p.body,i);
   if(row==0)result.fields[names[col]]=mrimTextToUtf8(value);
  }
 return result;
}
}

namespace pmlmrim {
// MRIM_CS_AUTHORIZE is the explicit approval packet (LP-string user address).
Bytes makeAuthorizationGrant(uint32_t seq, const std::string& email) {
 Packet p; p.sequence=seq; p.command=AUTHORIZE;
 putString(p.body,email);
 return encode(p);
}
// Request permission to be added: ordinary MESSAGE with the MRIM authorization flag.
Bytes makeAuthorizationRequest(uint32_t seq, const std::string& email, const std::string& reason) {
 Packet p; p.sequence=seq; p.command=MESSAGE;
 put32(p.body,0x00000008u);
 putString(p.body,email);
 putString(p.body,utf8ToMrimText(reason));
 putString(p.body,"");
 return encode(p);
}
}


// File-transfer control-plane codec. Intentionally no contact/UI changes here.
namespace pmlmrim {
namespace {
struct FileReader {
 const Bytes& b; size_t pos=0;
 uint32_t number() {
  if (pos>b.size() || b.size()-pos<4) throw std::runtime_error("short file transfer packet");
  uint32_t n=uint32_t(b[pos]) | (uint32_t(b[pos+1])<<8) |
     (uint32_t(b[pos+2])<<16) | (uint32_t(b[pos+3])<<24);
  pos+=4; return n;
 }
 std::string str() {
  uint32_t n=number();
  if (n>1048576 || pos>b.size() || n>b.size()-pos) throw std::runtime_error("invalid file transfer string");
  std::string r(reinterpret_cast<const char*>(b.data()+pos), n); pos+=n; return r;
 }
 void done() const {if(pos!=b.size()) throw std::runtime_error("trailing file transfer data");}
};
}
Bytes makeFileOffer(uint32_t seq,const FileOffer& f) {
 Packet p; p.command=FILE_TRANSFER; p.sequence=seq;
 putString(p.body,f.peer); put32(p.body,f.id); put32(p.body,f.totalSize);
 Bytes details; putString(details,f.files); putString(details,f.extra); putString(details,f.addresses);
 if(details.size()>1048576) throw std::length_error("file transfer details too long");
 put32(p.body,static_cast<uint32_t>(details.size()));
 p.body.insert(p.body.end(),details.begin(),details.end()); return encode(p);
}
Bytes makeFileOfferAck(uint32_t seq,const FileOfferAck& a) {
 Packet p; p.command=FILE_TRANSFER_ACK; p.sequence=seq;
 put32(p.body,a.status); putString(p.body,a.peer); put32(p.body,a.id); putString(p.body,a.addresses);
 return encode(p);
}
FileOffer parseFileOffer(const Packet& p) {
 if(p.command!=FILE_TRANSFER) throw std::invalid_argument("not a file offer");
 FileReader r{p.body}; FileOffer f; f.peer=r.str(); f.id=r.number(); f.totalSize=r.number();
 std::string details=r.str(); r.done(); Bytes nested(details.begin(),details.end());
 FileReader n{nested}; f.files=n.str(); f.extra=n.str(); f.addresses=n.str(); n.done();
 return f;
}
FileOfferAck parseFileOfferAck(const Packet& p) {
 if(p.command!=FILE_TRANSFER_ACK) throw std::invalid_argument("not a file offer acknowledgement");
 FileReader r{p.body}; FileOfferAck a;
 a.status=r.number();a.peer=r.str();a.id=r.number();a.addresses=r.str();r.done();
 if(a.status>4) throw std::runtime_error("unknown file transfer acknowledgement");
 return a;
}
}
