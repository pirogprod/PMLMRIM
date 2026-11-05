#include "mrim_file_connection.h"
#include <ws2tcpip.h>
#include <stdexcept>
#include <algorithm>
#include <climits>

namespace pmlmrim {
namespace {
bool numericIpv4(const std::string& addr, in_addr& ip) {
 if(addr.empty() || addr.size()>15) return false;
 return InetPtonA(AF_INET,addr.c_str(),&ip)==1;
}
SOCKET failClose(SOCKET sock) { if(sock!=INVALID_SOCKET) closesocket(sock); return INVALID_SOCKET; }
}
std::vector<FileEndpoint> parseFileEndpoints(const std::string& addresses) {
 if(addresses.size()>8192) throw std::invalid_argument("too many MRIM addresses");
 std::vector<FileEndpoint> result;
 size_t offset=0;
 while(offset<addresses.size()) {
  const auto end=addresses.find(';',offset);
  const auto part=addresses.substr(offset,(end==std::string::npos ? addresses.size() : end)-offset);
  offset=(end==std::string::npos ? addresses.size() : end+1);
  if(part.empty()) continue;
  const auto colon=part.find(':');
  if(colon==std::string::npos || part.find(':',colon+1)!=std::string::npos) continue;
  const auto ip=part.substr(0,colon);
  const auto port=part.substr(colon+1);
  in_addr parsed{};
  if(!numericIpv4(ip,parsed) || port.empty() || port.size()>5) continue;
  unsigned n=0;
  for(char c:port) { if(c<'0'||c>'9') { n=0; break; } n=n*10+(c-'0'); }
  if(n==0 || n>65535) continue;
  if(result.size()>=32) throw std::invalid_argument("too many MRIM endpoints");
  FileEndpoint endpoint{ip,static_cast<uint16_t>(n)};
  if(std::find_if(result.begin(),result.end(),[&](const FileEndpoint& a){return a.port==endpoint.port && a.ipv4==endpoint.ipv4;})==result.end()) result.push_back(endpoint);
 }
 return result;
}
SOCKET connectFileEndpoint(const FileEndpoint& endpoint,unsigned timeoutMs) {
 in_addr ip{};
 if(!numericIpv4(endpoint.ipv4,ip) || !endpoint.port) return INVALID_SOCKET;
 SOCKET sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
 if(sock==INVALID_SOCKET) return INVALID_SOCKET;
 u_long one=1;
 if(ioctlsocket(sock,FIONBIO,&one)!=0) return failClose(sock);
 sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(endpoint.port);addr.sin_addr=ip;
 int result=connect(sock,reinterpret_cast<sockaddr*>(&addr),sizeof(addr));
 if(result==SOCKET_ERROR && WSAGetLastError()!=WSAEWOULDBLOCK) return failClose(sock);
 if(result==SOCKET_ERROR) {
  fd_set writeSet;FD_ZERO(&writeSet);FD_SET(sock,&writeSet);
  fd_set errSet;FD_ZERO(&errSet);FD_SET(sock,&errSet);
  timeval tv{static_cast<long>(timeoutMs/1000),static_cast<long>((timeoutMs%1000)*1000)};
  result=select(0,nullptr,&writeSet,&errSet,&tv);
  if(result<=0 || !FD_ISSET(sock,&writeSet)) return failClose(sock);
  int error=0;int size=sizeof(error);
  if(getsockopt(sock,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&size)!=0 || error) return failClose(sock);
 }
 one=0;
 if(ioctlsocket(sock,FIONBIO,&one)!=0) return failClose(sock);
 if(!setFileSocketTimeout(sock)) return failClose(sock);
 return sock;
}
SOCKET connectFileEndpoints(const std::string& addresses,unsigned timeoutMs) {
 for(const auto& endpoint:parseFileEndpoints(addresses)) {
  SOCKET sock=connectFileEndpoint(endpoint,timeoutMs);
  if(sock!=INVALID_SOCKET) return sock;
 }
 return INVALID_SOCKET;
}
SOCKET listenForFiles(uint16_t& boundPort) {
 boundPort=0;
 SOCKET sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
 if(sock==INVALID_SOCKET) return INVALID_SOCKET;
 sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_ANY);addr.sin_port=0;
 if(bind(sock,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))!=0 || listen(sock,1)!=0) return failClose(sock);
 int len=sizeof(addr);
 if(getsockname(sock,reinterpret_cast<sockaddr*>(&addr),&len)!=0) return failClose(sock);
 boundPort=ntohs(addr.sin_port);
 return sock;
}
bool setFileSocketTimeout(SOCKET sock,unsigned timeoutMs) {
 if(timeoutMs>static_cast<unsigned>(INT_MAX)) timeoutMs=INT_MAX;
 DWORD ms=timeoutMs;
 return setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&ms),sizeof(ms))==0 &&
  setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&ms),sizeof(ms))==0;
}
}
