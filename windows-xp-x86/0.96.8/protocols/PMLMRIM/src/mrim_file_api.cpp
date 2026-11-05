// Direct TCP MRIM file transfers (no proxy fallback yet).
// winsock2 must precede windows.h in stdafx.h.
#include <winsock2.h>
#include <ws2tcpip.h>
#include "stdafx.h"
#include "mrim_file_connection.h"
#include "mrim_file_win32.h"
#include <memory>
#include <ctime>
#include <climits>
#include <stdexcept>
#include <algorithm>
#include <cstdlib>
#include <random>

namespace {
using namespace pmlmrim;
struct WsaScope {
 bool ready = false;
 WsaScope() { WSADATA d{}; ready = WSAStartup(MAKEWORD(2,2), &d) == 0; }
 ~WsaScope() { if (ready) WSACleanup(); }
 void transferToWorker() { ready = false; }
};
std::string wideToUtf8(const wchar_t *w) {
 if (!w) throw std::invalid_argument("null filename");
 int n = WideCharToMultiByte(CP_UTF8, 0 /* XP-compatible flags */, w, -1, nullptr, 0, nullptr, nullptr);
 if(n <= 1) throw std::invalid_argument("invalid Unicode filename");
 std::string value(size_t(n), '\0');
 if(!WideCharToMultiByte(CP_UTF8, 0 /* XP-compatible flags */, w, -1, &value[0], n, nullptr, nullptr))
  throw std::runtime_error("UTF-8 conversion failed");
 value.pop_back(); return value;
}
std::wstring utf8ToWide(const std::string &s) {
 if(s.empty()) return {};
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.c_str(),(int)s.size(),nullptr,0);
 if(n<=0) throw std::invalid_argument("invalid UTF-8 filename");
 std::wstring v(size_t(n),L'\0');
 if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.c_str(),(int)s.size(),&v[0],n))
  throw std::runtime_error("Unicode conversion failed");
 return v;
}
std::string localAddresses(uint16_t port) {
 char hostname[256]{};
 std::string result;
 if(gethostname(hostname,sizeof(hostname)-1)==0) {
  addrinfo hints{}; hints.ai_family=AF_INET; hints.ai_socktype=SOCK_STREAM;
  addrinfo *addresses=nullptr;
  if(getaddrinfo(hostname,nullptr,&hints,&addresses)==0) {
   for(auto *a=addresses;a;a=a->ai_next) {
    auto *sin=reinterpret_cast<sockaddr_in*>(a->ai_addr);
    // Use XP-compatible inet_ntoa for IPv4.
     const char *ip = inet_ntoa(sin->sin_addr);
     if(ip && strncmp(ip,"127.",4)) {
     std::string token=std::string(ip)+":"+std::to_string(port)+";";
     if(result.find(token)==std::string::npos) result+=token;
    }
   }
   freeaddrinfo(addresses);
  }
 }
 // Loopback enables two accounts on the same computer; never advertise it alone
 // to an external peer (doing so would connect to that peer's own machine).
 if(result.empty()) return {};
 result+="127.0.0.1:"+std::to_string(port)+";";
 return result;
}
uint32_t randomId() {
 try {
  std::random_device source;
  uint32_t value = static_cast<uint32_t>(source());
  return value == 0 ? 1u : value;
 }
 catch (...) { return 0; }
}
HANDLE transferHandle(uint32_t id) { return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(id)); }
}

HANDLE CMrimProto::SendFile(MCONTACT contact, const wchar_t*, wchar_t **paths)
{
 if(!m_online || !contact || !paths || !paths[0]) return nullptr;
 const CMStringA peer=getMStringA(contact,"Email");
 const CMStringA myEmail=getMStringA("Email");
 if(peer.IsEmpty() || myEmail.IsEmpty()) return nullptr;
 WsaScope wsa;
 if(!wsa.ready) return nullptr;
 uint16_t port=0;
 SOCKET listener=pmlmrim::listenForFiles(port);
 if(listener==INVALID_SOCKET) return nullptr;
 std::shared_ptr<FileNegotiation> session;
 try {
  session=std::make_shared<FileNegotiation>();
  session->contact=contact; session->peer=peer.c_str(); session->incoming=false;
  session->listener=static_cast<uintptr_t>(listener);
  uint64_t total=0;
  for(size_t i=0;paths[i];++i) {
   if(i>=1024) throw std::runtime_error("too many files");
   const wchar_t *path=paths[i];
   const wchar_t *slash=wcsrchr(path,L'\\');
   const wchar_t *alt=wcsrchr(path,L'/');
   if(!slash || (alt && alt>slash)) slash=alt;
   const wchar_t *name=slash ? slash+1 : path;
   if(!*name) throw std::runtime_error("empty filename");
   WIN32_FILE_ATTRIBUTE_DATA data{};
   if(!GetFileAttributesExW(path,GetFileExInfoStandard,&data) ||
      (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || data.nFileSizeHigh)
    throw std::runtime_error("source file not available or exceeds MRIM 32-bit limit");
   uint64_t length=data.nFileSizeLow;
   total+=length;
   if(total>UINT32_MAX) throw std::runtime_error("MRIM total file size limit exceeded");
   session->paths.emplace_back(path);
   session->files.push_back({wideToUtf8(name),static_cast<uint32_t>(length)});
  }
  const std::string list=pmlmrim::makeFileList(session->files);
  std::string addresses=localAddresses(port);
  if(addresses.empty()) throw std::runtime_error("no local IPv4 endpoint");
  // Unique per account while negotiations are active.
  {
   mir_cslock lock(m_csFile);
   do { session->id=randomId(); } while(!session->id || session->id==UINT32_MAX || m_fileNegotiations.count(session->id));
   m_fileNegotiations[session->id]=session;
  }
  pmlmrim::FileOffer offer;
  offer.peer=session->peer; offer.id=session->id;
  offer.totalSize=static_cast<uint32_t>(total);
  offer.files=list; offer.addresses=addresses;
  if(!SendPacket(pmlmrim::makeFileOffer(++m_seq,offer))) {
   mir_cslock lock(m_csFile); m_fileNegotiations.erase(session->id);
   closesocket(listener); return nullptr;
  }
  // The worker becomes responsible for closing the listener.
  auto *arg=new std::shared_ptr<FileNegotiation>(session);
  ForkThread(&CMrimProto::FileWorker,arg);
  wsa.transferToWorker();
  ProtoBroadcastAck(contact,ACKTYPE_FILE,ACKRESULT_SENTREQUEST,transferHandle(session->id));
  return transferHandle(session->id);
 }
 catch(const std::exception &e) {
  debugLogA("MRIM FT SendFile error: %s",e.what());
  if(session && session->id) { mir_cslock lock(m_csFile); m_fileNegotiations.erase(session->id); }
  closesocket(listener); return nullptr;
 }
}

void CMrimProto::HandleIncomingFileOffer(const pmlmrim::FileOffer &offer)
{
 if(!m_online || offer.peer.empty() || offer.peer.size()>320 || !offer.id) return;
 auto refuse=[&]() {
  pmlmrim::FileOfferAck ack; ack.peer=offer.peer; ack.id=offer.id; ack.status=0;
  SendPacket(pmlmrim::makeFileOfferAck(++m_seq,ack));
 };
 try {
  MCONTACT contact=FindContact(offer.peer.c_str());
  if(!contact) { refuse(); return; }
  auto files=pmlmrim::parseFileList(offer.files);
  uint64_t total=0;
  for(const auto &f:files) total+=f.size;
  if(total!=offer.totalSize || offer.addresses.size()>8192) { refuse(); return; }
  auto session=std::make_shared<FileNegotiation>();
  session->incoming=true; session->contact=contact; session->id=offer.id;
  session->peer=offer.peer; session->addresses=offer.addresses;
  session->files=std::move(files);
  {
   mir_cslock lock(m_csFile);
   if(m_fileNegotiations.count(session->id)) { refuse(); return; }
   m_fileNegotiations[session->id]=session;
  }
  // File event is visible in the regular Miranda incoming-file dialog.
  DB::FILE_BLOB blob(utf8ToWide(session->files[0].name).c_str());
  blob.setSize(total);
  blob.setUserInfo(transferHandle(session->id));
  DB::EventInfo event;
  event.eventType=EVENTTYPE_FILE;
  event.szModule=m_szModuleName;
  event.iTimestamp=static_cast<uint32_t>(time(nullptr));
  ProtoChainRecvFile(contact,blob,event);
  debugLogA("MRIM FT: file offer id=%u delivered to Miranda",session->id);
 }
 catch(const std::exception &e) { debugLogA("MRIM FT incoming offer: %s",e.what()); refuse(); }
}

void CMrimProto::HandleFileOfferAck(const pmlmrim::FileOfferAck &ack)
{
 std::shared_ptr<FileNegotiation> session;
 {
  mir_cslock lock(m_csFile);
  auto it=m_fileNegotiations.find(ack.id);
  if(it==m_fileNegotiations.end() || it->second->incoming ||
     mir_strcmpi(it->second->peer.c_str(),ack.peer.c_str())) return;
  session=it->second;
 }
 if(ack.status==1) session->accepted=true;
 else { session->denied=true; session->cancelled=true; }
}

HANDLE CMrimProto::FileAllow(MCONTACT contact, HANDLE handle, const wchar_t *directory)
{
 if(!directory || !*directory || !m_online) return nullptr;
 auto id=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handle));
 std::shared_ptr<FileNegotiation> session;
 {
  mir_cslock lock(m_csFile);
  auto it=m_fileNegotiations.find(id);
  if(it==m_fileNegotiations.end() || !it->second->incoming || it->second->contact!=contact ||
     it->second->accepted) return nullptr;
  session=it->second;
  session->directory=directory;
  session->accepted=true;
 }
 // Fail immediately if there is no usable direct endpoint.
 try { if(pmlmrim::parseFileEndpoints(session->addresses).empty()) throw std::runtime_error("no peer addresses"); }
 catch(...) { session->cancelled=true; return nullptr; }
 WsaScope wsa;
 if(!wsa.ready) { session->cancelled=true; return nullptr; }
 pmlmrim::FileOfferAck ack; ack.status=1; ack.id=id; ack.peer=session->peer;
 if(!SendPacket(pmlmrim::makeFileOfferAck(++m_seq,ack))) { session->cancelled=true; session->accepted=false; return nullptr; }
 ForkThread(&CMrimProto::FileWorker,new std::shared_ptr<FileNegotiation>(session));
 wsa.transferToWorker();
 return handle;
}

int CMrimProto::FileDeny(MCONTACT contact,HANDLE handle,const wchar_t*)
{
 auto id=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handle));
 std::shared_ptr<FileNegotiation> session;
 {
  mir_cslock lock(m_csFile);
  auto it=m_fileNegotiations.find(id);
  if(it==m_fileNegotiations.end() || !it->second->incoming || it->second->contact!=contact) return 1;
  session=it->second; m_fileNegotiations.erase(it);
 }
 session->cancelled=true;
 pmlmrim::FileOfferAck ack; ack.status=0; ack.id=id; ack.peer=session->peer;
 return SendPacket(pmlmrim::makeFileOfferAck(++m_seq,ack)) ? 0 : 1;
}

int CMrimProto::FileCancel(MCONTACT contact,HANDLE handle)
{
 auto id=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handle));
 std::shared_ptr<FileNegotiation> session;
 {
  mir_cslock lock(m_csFile);
  auto it=m_fileNegotiations.find(id);
  if(it==m_fileNegotiations.end() || it->second->contact!=contact) return 1;
  session=it->second;
 }
 session->cancelled=true;
 SOCKET socket=static_cast<SOCKET>(session->socket.load());
 if(socket!=INVALID_SOCKET && socket!=0) shutdown(socket,SD_BOTH);
 if(session->incoming && !session->accepted) return FileDeny(contact,handle,nullptr);
 return 0;
}

void __cdecl CMrimProto::FileWorker(void *arg)
{
 std::unique_ptr<std::shared_ptr<FileNegotiation>> owner(static_cast<std::shared_ptr<FileNegotiation>*>(arg));
 auto session=*owner;
 const HANDLE handle=transferHandle(session->id);
 SOCKET socket=INVALID_SOCKET;
 SOCKET listener=session->incoming ? INVALID_SOCKET : static_cast<SOCKET>(session->listener);
 try {
  if(session->incoming) {
   ProtoBroadcastAck(session->contact,ACKTYPE_FILE,ACKRESULT_CONNECTING,handle);
   socket=pmlmrim::connectFileEndpoints(session->addresses,6000);
   if(socket==INVALID_SOCKET) throw std::runtime_error("MRIM TCP connect failed; proxy is not implemented");
  }
  else {
   ProtoBroadcastAck(session->contact,ACKTYPE_FILE,ACKRESULT_LISTENING,handle);
   // Do not allow unbounded accept() stalls; a denied offer exits promptly.
   for(unsigned waited=0;waited<1200 && !session->cancelled;++waited) {
    if(!session->accepted) { Sleep(100); continue; }
    fd_set ready; FD_ZERO(&ready); FD_SET(listener,&ready);
    timeval tv{0,100000};
    int n=select(0,&ready,nullptr,nullptr,&tv);
    if(n==SOCKET_ERROR) throw std::runtime_error("MRIM listener failed");
    if(n>0 && session->accepted) {
     socket=accept(listener,nullptr,nullptr);
     break;
    }
   }
   if(socket==INVALID_SOCKET) throw std::runtime_error("MRIM file transfer timed out or declined");
   if(!pmlmrim::setFileSocketTimeout(socket)) throw std::runtime_error("MRIM socket timeout setup failed");
  }
  session->socket=static_cast<uintptr_t>(socket);
  ProtoBroadcastAck(session->contact,ACKTYPE_FILE,ACKRESULT_CONNECTED,handle);
  const std::string own=getMStringA("Email").c_str();
  std::vector<std::wstring> wideNames;
  std::vector<wchar_t*> widePointers;
  for(const auto& item:session->files) wideNames.push_back(utf8ToWide(item.name));
  for(auto& name:wideNames) widePointers.push_back(&name[0]);
  const auto callback=[&](size_t index,uint64_t progress) {
   PROTOFILETRANSFERSTATUS status{};
   status.hContact=session->contact;
   status.flags=(session->incoming ? PFTS_RECEIVING : PFTS_SENDING) | PFTS_UNICODE;
   status.totalFiles=static_cast<int>(session->files.size());
   status.pszFiles.w=widePointers.data();
   status.currentFileNumber=static_cast<int>(index);
   uint64_t previous=0,total=0;
   for(size_t i=0;i<session->files.size();++i) {
    total+=session->files[i].size;
    if(i<index) previous+=session->files[i].size;
   }
   status.totalBytes=total;
   status.totalProgress=previous+progress;
   status.currentFileSize=session->files[index].size;
   status.currentFileProgress=progress;
   status.szCurrentFile.w=const_cast<wchar_t*>(wideNames[index].c_str());
   status.szWorkingDir.w=const_cast<wchar_t*>(session->incoming ? session->directory.c_str() : L"");
   ProtoBroadcastAck(session->contact,ACKTYPE_FILE,ACKRESULT_DATA,handle,(LPARAM)&status);
  };
  const auto cancelled=[&]() {return session->cancelled.load() || m_stop.load();};
  if(session->incoming)
   pmlmrim::receiveFilesOnSocket(socket,session->peer,own,session->files,session->directory,callback,cancelled);
  else
   pmlmrim::sendFilesOnSocket(socket,own,session->peer,session->files,session->paths,callback,cancelled);
  ProtoBroadcastAck(session->contact,ACKTYPE_FILE,ACKRESULT_SUCCESS,handle);
 }
 catch(const std::exception &e) {
  debugLogA("MRIM FT %u: %s",session->id,e.what());
  ProtoBroadcastAck(session->contact,ACKTYPE_FILE,session->denied ? ACKRESULT_DENIED : ACKRESULT_FAILED,handle);
 }
 if(socket!=INVALID_SOCKET) { session->socket=0; closesocket(socket); }
 if(listener!=INVALID_SOCKET) closesocket(listener);
 WSACleanup(); // Matches the WSAStartup transferred from SendFile/FileAllow.
 {mir_cslock lock(m_csFile);
  auto it=m_fileNegotiations.find(session->id);
  if(it!=m_fileNegotiations.end() && it->second==session) m_fileNegotiations.erase(it);
 }
}

void CMrimProto::StopFileTransfers()
{
 mir_cslock lock(m_csFile);
 for(auto &entry:m_fileNegotiations) {
  auto &session=entry.second;
  session->cancelled=true;
  SOCKET sock=static_cast<SOCKET>(session->socket.load());
  if(sock!=INVALID_SOCKET && sock!=0) shutdown(sock,SD_BOTH);
 }
}
