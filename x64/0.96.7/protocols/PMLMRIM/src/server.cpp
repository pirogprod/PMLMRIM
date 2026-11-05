#include "stdafx.h"
#include <cstdio>
#include <windows.h>

// Local roster diagnostics without requiring the optional Netlib logger plugin.
// Stores counts only: no passwords, emails or chat messages.
static void WriteRosterDiagnostics(unsigned parsed, unsigned imported, unsigned dbTotal, unsigned bodyBytes)
{
 char folder[MAX_PATH] = {};
 DWORD n = GetTempPathA(MAX_PATH, folder);
 if (n == 0 || n >= MAX_PATH) return;
 std::string path(folder); path += "PMLMRIM-roster.txt";
 FILE* fp = nullptr;
 if (fopen_s(&fp, path.c_str(), "a") != 0 || !fp) return;
 SYSTEMTIME t; GetLocalTime(&t);
 fprintf(fp, "%04u-%02u-%02u %02u:%02u:%02u  ROSTER: parsed=%u imported=%u database=%u bodyBytes=%u\n",
  t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, parsed, imported, dbTotal, bodyBytes);
 fclose(fp);
}

void CMrimProto::HandlePacket(const pmlmrim::Packet& p){
 if(p.command==pmlmrim::LOGIN_ACK||p.command==pmlmrim::CONTACT_LIST2||p.command==pmlmrim::CONTACT_LIST_ACK){
  char folder[MAX_PATH]={};if(GetTempPathA(MAX_PATH,folder)){
   std::string path(folder);path+="PMLMRIM-roster.txt";
   FILE* fp=nullptr;if(fopen_s(&fp,path.c_str(),"a")==0&&fp){
    fprintf(fp,"PACKET: command=0x%04X bytes=%u\n",p.command,(unsigned)p.body.size());fclose(fp);
   }
  }
 }
 try{
 if(p.command==pmlmrim::FILE_TRANSFER){
  HandleIncomingFileOffer(pmlmrim::parseFileOffer(p));
  return;
 }
 if(p.command==pmlmrim::FILE_TRANSFER_ACK){
  HandleFileOfferAck(pmlmrim::parseFileOfferAck(p));
  return;
 }
 if(p.command==pmlmrim::HELLO_ACK){auto ack=pmlmrim::parseHelloAck(p);m_pingInterval = 5u; if (ack.pingSeconds > 0) { unsigned interval = ack.pingSeconds / 2u; if (interval < 3u) interval = 3u; if (interval > 10u) interval = 10u; m_pingInterval = interval; }debugLogA("MRIM server ping interval=%u, sending every %u seconds",ack.pingSeconds,m_pingInterval);auto email=getMStringA("Email");auto password=getMStringA("Password");const auto uri=getMStringA("XStatusRequestedUri");
 const auto title=getMStringA("XStatusRequestedTitle");
 const auto message=getMStringA("XStatusRequestedMessage");
 const bool extended = !uri.IsEmpty() && MrimIsSupportedXStatus(uri.c_str());
 SendPacket(pmlmrim::makeLogin(++m_seq,email.c_str(),password.c_str(),
  extended ? 4u : ToMrimStatus(m_iDesiredStatus),
  extended ? uri.c_str() : "",
  extended ? pmlmrim::utf8ToMrimText(title.c_str()) : std::string(),
  extended ? pmlmrim::utf8ToMrimText(message.c_str()) : std::string()));return;}
 if(p.command==pmlmrim::LOGIN_ACK){m_online=true;int old=m_iStatus;m_iStatus=m_iDesiredStatus;ProtoBroadcastAck(0,ACKTYPE_STATUS,ACKRESULT_SUCCESS,(HANDLE)old,m_iStatus);return;}
 if(p.command==pmlmrim::LOGIN_REJ){debugLogA("MRIM login rejected: %s",pmlmrim::parseLoginRejection(p).c_str());ProtoBroadcastAck(0,ACKTYPE_LOGIN,ACKRESULT_FAILED,nullptr,LOGINERR_WRONGPASSWORD);m_stop=true;return;}
 if(p.command==pmlmrim::CONTACT_LIST2 || p.command==pmlmrim::CONTACT_LIST_ACK){
  auto contacts=pmlmrim::parseRoster(p);unsigned count=0;debugLogA("MRIM roster: parsed=%u body=%u command=0x%04X",(unsigned)contacts.size(),(unsigned)p.body.size(),p.command);
  for(const auto& entry:contacts){
   MCONTACT c=EnsureContact(entry.email.c_str());if(!c)continue;
   // Server roster contacts are permanent; older versions created hidden/temporary contacts.
   Contact::PutOnList(c);
   Contact::Hide(c,false);
   db_unset(c,"CList","NotOnList");db_unset(c,"CList","Hidden");
   debugLogA("MRIM roster contact: %s flags=0x%08X status=%u",entry.email.c_str(),entry.flags,entry.status);
   // Legacy roster has no reliable status field. Do not overwrite an online
   // USER_STATUS notification with OFFLINE during initial roster import.
   // Do not replace an already received online presence with a stale OFFLINE
   // snapshot from the roster (delivery ordering differs between servers).
   const int incomingStatus=ToMirandaStatus(entry.status);
   if(incomingStatus!=ID_STATUS_OFFLINE || getWord(c,"Status",ID_STATUS_OFFLINE)==ID_STATUS_OFFLINE)
    setWord(c,"Status",incomingStatus);
   const bool validX = entry.status == 4u && MrimIsSupportedXStatus(entry.xstatusUri);
   setByte(c, "XStatusActive", validX ? 1 : 0);
   if (validX) {
    setString(c,"XStatusUri",entry.xstatusUri.c_str());
    auto title = ptrW(mir_utf8decodeW(entry.xstatusTitle.c_str()));
    auto detail = ptrW(mir_utf8decodeW(entry.xstatusDescription.c_str()));
    const wchar_t *visible = detail && *detail ? detail.get() : title.get();
    if (title && *title) setWString(c,"XStatusName",title);
    else db_unset(c,m_szModuleName,"XStatusName");
    if (visible && *visible) {
     setWString(c,"StatusMsg",visible);
     setWString(c,"XStatusMsg",visible);
     db_set_ws(c,"CList","StatusMsg",visible);
    } else {
     db_unset(c,m_szModuleName,"StatusMsg");
     db_unset(c,m_szModuleName,"XStatusMsg");
     db_unset(c,"CList","StatusMsg");
    }
   } else {
    db_unset(c,m_szModuleName,"XStatusUri");
    db_unset(c,m_szModuleName,"XStatusName");
    db_unset(c,m_szModuleName,"StatusMsg");
     db_unset(c,m_szModuleName,"XStatusMsg");
    db_unset(c,"CList","StatusMsg");
   }
   ApplyXStatusIcon(c);
   if(!entry.nickname.empty())setWString(c,"Nick",ptrW(mir_utf8decodeW(entry.nickname.c_str())));
   // These values are locally available from CONTACT_LIST2. The server does
   // not supply birthday, gender, first name or surname in this packet.
   setString(c,"e-mail",entry.email.c_str());
   setString(c,"Email",entry.email.c_str());
   ++count;
  }
  unsigned totalDb=0; for (auto& existing : AccContacts()) { (void)existing; ++totalDb; }
  debugLogA("MRIM roster imported %u/%u; account database contains %u contacts",count,(unsigned)contacts.size(),totalDb);
  WriteRosterDiagnostics((unsigned)contacts.size(),count,totalDb,(unsigned)p.body.size());return;
 }
 if(p.command==pmlmrim::ANKETA_INFO){
  PendingSearch search = {};
  bool isSearch = false;
  { mir_cslock lock(m_csPending);
    auto it = m_pendingSearches.find(p.sequence);
    if (it != m_pendingSearches.end()) {
     search = it->second; m_pendingSearches.erase(it); isSearch = true;
    }
  }
  if (isSearch) {
   try {
    const auto result = pmlmrim::parseProfileResponse(p);
    debugLogA("MRIM SEARCH: ANKETA_INFO seq=%u status=%u fields=%u",p.sequence,result.status,(unsigned)result.fields.size());
    if (result.status == 1 && !result.fields.empty()) {
     // The requested address is the search identity. The response provides
     // optional profile fields; retain them until the synchronous ACK returns.
     std::wstring nick, first, last;
     auto value = [&](const char* key) -> std::wstring {
      auto it = result.fields.find(key);
      if (it == result.fields.end() || it->second.empty()) return {};
      auto wide = ptrW(mir_utf8decodeW(it->second.c_str()));
      return wide ? std::wstring(wide.get()) : std::wstring();
     };
     nick = value("Nickname"); first = value("FirstName"); last = value("LastName");
     PROTOSEARCHRESULT psr = {};
     psr.cbSize = sizeof(psr); psr.flags = PSR_UNICODE;
     psr.id.w = const_cast<wchar_t*>(search.email.c_str());
     psr.email.w = const_cast<wchar_t*>(search.email.c_str());
     if (!nick.empty()) psr.nick.w = const_cast<wchar_t*>(nick.c_str());
     if (!first.empty()) psr.firstName.w = const_cast<wchar_t*>(first.c_str());
     if (!last.empty()) psr.lastName.w = const_cast<wchar_t*>(last.c_str());
     ProtoBroadcastAck(0,ACKTYPE_SEARCH,ACKRESULT_DATA,search.handle,(LPARAM)&psr);
     ProtoBroadcastAck(0,ACKTYPE_SEARCH,ACKRESULT_SUCCESS,search.handle,0);
    }
    else if (result.status == 0 || (result.status == 1 && result.fields.empty())) {
     // A valid empty result: finish normally but do not claim a match.
     ProtoBroadcastAck(0,ACKTYPE_SEARCH,ACKRESULT_SUCCESS,search.handle,0);
    }
    else ProtoBroadcastAck(0,ACKTYPE_SEARCH,ACKRESULT_FAILED,search.handle,0);
   }
   catch (const std::exception& e) {
    debugLogA("MRIM SEARCH: malformed reply seq=%u: %s",p.sequence,e.what());
    ProtoBroadcastAck(0,ACKTYPE_SEARCH,ACKRESULT_FAILED,search.handle,0);
   }
   return;
  }

  MCONTACT c=0;
  {mir_cslock lock(m_csPending);auto it=m_pendingProfiles.find(p.sequence);if(it!=m_pendingProfiles.end()){c=it->second;m_pendingProfiles.erase(it);}}
  if(!c){debugLogA("MRIM ANKETA_INFO unmatched seq=%u",p.sequence);return;}
  auto result=pmlmrim::parseProfileResponse(p);
  if(result.status!=1 || result.fields.empty()){debugLogA("MRIM profile: status=%u fields=%u",result.status,(unsigned)result.fields.size());ProtoBroadcastAsync(c,ACKTYPE_GETINFO,ACKRESULT_FAILED,(HANDLE)1,0);return;}
  const struct {const char* remote;const char* local;} mappings[]={
   {"Username","UserName"},{"Nickname","Nick"},{"FirstName","FirstName"},
   {"LastName","LastName"},{"Sex","Gender"},{"Birthday","BirthDate"},
   {"Location","Location"},{"Phone","Phone"},{"city_id","City"},
   {"country_id","Country"}
  };
  for(const auto& m:mappings){auto it=result.fields.find(m.remote);if(it!=result.fields.end()&&!it->second.empty()){
   auto wide=ptrW(mir_utf8decodeW(it->second.c_str()));if(wide)setWString(c,m.local,wide);
  }}
  debugLogA("MRIM profile loaded: %u fields",(unsigned)result.fields.size());
  ProtoBroadcastAsync(c,ACKTYPE_GETINFO,ACKRESULT_SUCCESS,(HANDLE)1,0);
  return;
 }
 if(p.command==pmlmrim::ADD_CONTACT_ACK){debugLogA("MRIM ADD_CONTACT_ACK seq=%u",p.sequence);return;}
 if(p.command==pmlmrim::USER_STATUS){
  const auto s=pmlmrim::parseStatusUpdate(p);
  auto c=EnsureContact(s.email.c_str());
  if(c){
   setWord(c,"Status",ToMirandaStatus(s.status));
   // Tipper reads the protocol's MirVer string (also available as
   // %raw:/MirVer%). Only advertise a client when MRIM actually supplied it.
   // Remove stale versions when a peer disconnects or sends no User-Agent.
   if(s.status != 0 && !s.userAgent.empty()) {
    // parseStatusUpdate has already decoded MRIM CP1251 to UTF-8.
    std::string agent;
    for(unsigned char ch : s.userAgent) {
     if(ch < 0x20 || ch == 0x7f) continue;
     agent.push_back(static_cast<char>(ch));
     if(agent.size() >= 160) break;
    }
    if(!agent.empty()) {
     auto wide=ptrW(mir_utf8decodeW(agent.c_str()));
     if(wide && *wide) setWString(c,"MirVer",wide);
     else db_unset(c,m_szModuleName,"MirVer");
    } else db_unset(c,m_szModuleName,"MirVer");
   } else db_unset(c,m_szModuleName,"MirVer");
   debugLogA("MRIM client: peer=%s agent=%s", s.email.c_str(), s.userAgent.empty()?"(not sent)":s.userAgent.c_str());
   const bool validX = s.status == 4u && MrimIsSupportedXStatus(s.xstatusUri);
   setByte(c, "XStatusActive", validX ? 1 : 0);
   if (validX) {
    setString(c,"XStatusUri",s.xstatusUri.c_str());
    auto title = ptrW(mir_utf8decodeW(s.xstatusTitle.c_str()));
    auto detail = ptrW(mir_utf8decodeW(s.xstatusDescription.c_str()));
    const wchar_t *visible = detail && *detail ? detail.get() : title.get();
    if (title && *title) setWString(c,"XStatusName",title);
    else db_unset(c,m_szModuleName,"XStatusName");
    if (visible && *visible) {
     setWString(c,"StatusMsg",visible);
     setWString(c,"XStatusMsg",visible);
     db_set_ws(c,"CList","StatusMsg",visible);
    } else {
     db_unset(c,m_szModuleName,"StatusMsg");
     db_unset(c,m_szModuleName,"XStatusMsg");
     db_unset(c,"CList","StatusMsg");
    }
   } else {
    db_unset(c,m_szModuleName,"XStatusUri");
    db_unset(c,m_szModuleName,"XStatusName");
    db_unset(c,m_szModuleName,"StatusMsg");
     db_unset(c,m_szModuleName,"XStatusMsg");
    db_unset(c,"CList","StatusMsg");
   }
   ApplyXStatusIcon(c);
  }
  debugLogA("MRIM presence applied: status=0x%X, xstatus=%s",s.status,s.xstatusUri.c_str());
  return;
 }
 if(p.command==pmlmrim::MESSAGE_STATUS){
  if(p.body.size()<4){debugLogA("MRIM MESSAGE_STATUS missing status code");return;}
  uint32_t status=uint32_t(p.body[0])|(uint32_t(p.body[1])<<8)|(uint32_t(p.body[2])<<16)|(uint32_t(p.body[3])<<24);
  MCONTACT recipient=0;bool found=false;
  {mir_cslock l(m_csPending);auto it=m_pendingMessages.find(p.sequence);if(it!=m_pendingMessages.end()){recipient=it->second;m_pendingMessages.erase(it);found=true;}}
  debugLogA("MRIM MESSAGE_STATUS seq=%u code=0x%08X matched=%d",p.sequence,status,found?1:0);
  if(found)ProtoBroadcastAsync(recipient,ACKTYPE_MESSAGE,status==0?ACKRESULT_SUCCESS:ACKRESULT_FAILED,(HANDLE)(INT_PTR)p.sequence,status==0?0:(LPARAM)TranslateT("MRIM server rejected message"));
  return;
 }
 if(p.command==pmlmrim::AUTHORIZE_ACK){
  // Server acknowledged a grant from a peer. Keep the contact's auth state in sync.
  if(p.body.size()>=4){
   size_t len=(size_t)p.body[0]|((size_t)p.body[1]<<8)|((size_t)p.body[2]<<16)|((size_t)p.body[3]<<24);
   if(len>0 && len<=p.body.size()-4){
    std::string email((const char*)p.body.data()+4,len);
    auto c=FindContact(email.c_str());
    if(c) {setByte(c,"Auth",0);debugLogA("MRIM authorized by %s",email.c_str());}
   }
  }
  return;
 }
 if(p.command==pmlmrim::MESSAGE_ACK){
  auto m=pmlmrim::parseIncomingMessage(p);
  debugLogA("MRIM incoming: id=%u flags=0x%08X sender=%s textBytes=%u rtfBytes=%u utf8Bytes=%u",m.id,m.flags,m.sender.c_str(),(unsigned)m.rawTextBytes,(unsigned)m.rawRtfBytes,(unsigned)m.text.size());
  // MRIM NOTIFY (0x400) is typing activity, never a chat message.
  if(m.flags&0x400u){
   auto c=FindContact(m.sender.c_str());
   if(c)CallService(MS_PROTO_CONTACTISTYPING,c,10);
   SendPacket(pmlmrim::makeMessageReceived(++m_seq,m.id,m.sender));
   return;
  }
  // MRIM MESSAGE_FLAG_AUTHORIZE (0x08): a real authorization request,
  // not a chat message. Emit Miranda's native event so the user can approve.
  if (m.flags & 0x00000008u) {
   if (!m.sender.empty() && m.sender.find('@') != std::string::npos) {
    MCONTACT c=FindContact(m.sender.c_str());
    if (!c) {
     c=db_add_contact(); if(c) Proto_AddToContact(c,m_szModuleName);
     if (c) {
      setString(c,"Email",m.sender.c_str());
      setString(c,"e-mail",m.sender.c_str());
      setWord(c,"Status",ID_STATUS_OFFLINE);
      db_set_b(c,"CList","NotOnList",1);
     }
    }
    if (c) {
     setByte(c,"Grant",1);
     DB::AUTH_BLOB blob(c,m.sender.c_str(),"","",m.sender.c_str(),m.text.c_str());
     DB::EventInfo dbei;
     dbei.iTimestamp=(uint32_t)time(nullptr);
     dbei.eventType=EVENTTYPE_AUTHREQUEST;
     dbei.flags=DBEF_UTF;
     dbei.szModule=m_szModuleName;
     dbei.cbBlob=blob.size();
     dbei.pBlob=blob;
     Proto_AuthRecv(m_szModuleName,dbei);
     debugLogA("MRIM incoming authorization request from %s",m.sender.c_str());
    }
   }
   SendPacket(pmlmrim::makeMessageReceived(++m_seq,m.id,m.sender));
   return;
  }
  // MRIM AUTHORIZE_ACK (0x1021) is handled as a separate packet below.
  // Service notifications and empty bodies must not create blank chat entries.
  // Still acknowledge them, so the server does not resend them.
  if(!m.text.empty()){
   auto c=EnsureContact(m.sender.c_str());
   if(c){DB::EventInfo dbei;dbei.iTimestamp=(uint32_t)time(nullptr);dbei.eventType=EVENTTYPE_MESSAGE;dbei.flags=DBEF_UTF;dbei.szModule=m_szModuleName;dbei.cbBlob=(uint32_t)m.text.size()+1;dbei.pBlob=const_cast<char*>(m.text.c_str());db_event_add(c,&dbei);}
  }else debugLogA("MRIM ignored empty incoming body (flags=0x%08X, rawTextBytes=%u)",m.flags,(unsigned)m.rawTextBytes);
  SendPacket(pmlmrim::makeMessageReceived(++m_seq,m.id,m.sender));return;
 }
 }catch(const std::exception& e){
  debugLogA("MRIM packet processing error: cmd=0x%08X bytes=%u error=%s",p.command,(unsigned)p.body.size(),e.what());
  if(p.command==pmlmrim::CONTACT_LIST2 || p.command==pmlmrim::CONTACT_LIST_ACK){
   char folder[MAX_PATH]={};if(GetTempPathA(MAX_PATH,folder)){
    std::string path(folder);path+="PMLMRIM-roster.txt";
    FILE* fp=nullptr;if(fopen_s(&fp,path.c_str(),"a")==0&&fp){
     fprintf(fp,"ROSTER PARSE ERROR: %s (bytes=%u)\n",e.what(),(unsigned)p.body.size());fclose(fp);
    }
   }
  }
 }
}
void CMrimProto::ServerThread(void*){
 T2Utf host(m_server);int port=m_port;if(port<1||port>65535)port=2042;
 HNETLIBCONN conn=Netlib_OpenConnection(m_hNetlibUser,host,port,20);
 if(!conn){debugLogA("MRIM connect failed: %s:%d",host.get(),port);ProtoBroadcastAck(0,ACKTYPE_LOGIN,ACKRESULT_FAILED,nullptr,LOGINERR_NONETWORK);}
 else{
 {mir_cslock l(m_csSend);m_hConn=conn;}
 try{
 SendPacket(pmlmrim::makeHello(++m_seq));
 pmlmrim::Bytes pending;time_t lastPing=time(nullptr);
 while(!m_stop){
 NETLIBSELECT sel={};sel.dwTimeout=1000;sel.hReadConns[0]=conn;
 int ready=Netlib_Select(&sel);if(ready<0){debugLogA("MRIM Netlib_Select error, closing connection");break;}
 if(m_online&&time(nullptr)-lastPing>=m_pingInterval){if(!SendPacket(pmlmrim::makePing(++m_seq)))break;lastPing=time(nullptr);}
 // Complete orphaned searches even when this server never sends ANKETA_INFO.
 {
  std::vector<HANDLE> expired;
  { mir_cslock lock(m_csPending);
    for (auto it = m_pendingSearches.begin(); it != m_pendingSearches.end();) {
     if ((DWORD)(GetTickCount() - it->second.started) >= 15000u) {
      expired.push_back(it->second.handle); it = m_pendingSearches.erase(it);
     }
     else ++it;
    }
  }
  for (HANDLE handle : expired) {
   debugLogA("MRIM SEARCH: timed out waiting for ANKETA_INFO");
   ProtoBroadcastAck(0,ACKTYPE_SEARCH,ACKRESULT_FAILED,handle,0);
  }
 }
 if(ready==0)continue;
 char buf[8192];int n=Netlib_Recv(conn,buf,sizeof(buf),0);if(n<=0){debugLogA("MRIM Netlib_Recv returned %d, closing connection",n);break;}pending.insert(pending.end(),buf,buf+n);
 pmlmrim::Packet packet;while(pmlmrim::decodeOne(pending,packet)){HandlePacket(packet);if(m_stop)break;}

 }
 }catch(const std::exception& e){debugLogA("MRIM session error: %s",e.what());}
 {mir_cslock l(m_csSend);if(m_hConn==conn){m_hConn=nullptr;Netlib_CloseHandle(conn);}}
 }
 {
  std::map<uint32_t,MCONTACT> pending;
  {mir_cslock l(m_csPending);pending.swap(m_pendingMessages);}
  for(const auto& item:pending)ProtoBroadcastAsync(item.second,ACKTYPE_MESSAGE,ACKRESULT_FAILED,(HANDLE)(INT_PTR)item.first,(LPARAM)TranslateT("MRIM disconnected before delivery confirmation"));
 }
 {
  std::vector<HANDLE> interrupted;
  { mir_cslock lock(m_csPending);
    for (const auto& item : m_pendingSearches) interrupted.push_back(item.second.handle);
    m_pendingSearches.clear();
    m_pendingProfiles.clear();
  }
  for (HANDLE handle : interrupted)
   ProtoBroadcastAck(0,ACKTYPE_SEARCH,ACKRESULT_FAILED,handle,0);
 }
 m_online=false;int old=m_iStatus;m_iStatus=m_iDesiredStatus=ID_STATUS_OFFLINE;setAllContactStatuses(ID_STATUS_OFFLINE);
 for (auto &c : AccContacts()) { setByte(c, "XStatusActive", 0); ApplyXStatusIcon(c); }
 if(old!=ID_STATUS_OFFLINE)ProtoBroadcastAck(0,ACKTYPE_STATUS,ACKRESULT_SUCCESS,(HANDLE)old,m_iStatus);
}
