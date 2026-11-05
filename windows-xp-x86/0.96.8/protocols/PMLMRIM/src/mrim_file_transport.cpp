#include "mrim_file_transport.h"
#include <algorithm>
#include <stdexcept>
namespace pmlmrim {
void sendAll(const WriteBytes& write, const char* data, size_t size,
             const IsCancelled& cancelled) {
 size_t sent=0;
 while(sent<size) {
  if(cancelled && cancelled()) throw std::runtime_error("MRIM transfer cancelled");
  int n=write(data+sent,size-sent);
  if(n<=0 || static_cast<size_t>(n)>size-sent)
   throw std::runtime_error("MRIM transfer write failed");
  sent+=static_cast<size_t>(n);
 }
}
std::string receiveCommand(const ReadBytes& read, size_t maxSize,
                           const IsCancelled& cancelled) {
 if(maxSize==0 || maxSize>65536) throw std::invalid_argument("invalid command limit");
 std::string result;
 while(result.size()<maxSize) {
  if(cancelled && cancelled()) throw std::runtime_error("MRIM transfer cancelled");
  char c=0; int n=read(&c,1);
  if(n!=1) throw std::runtime_error("MRIM transfer command truncated");
  result.push_back(c);
  if(c=='\0') return result;
 }
 throw std::length_error("MRIM transfer command too long");
}
void sendFileBody(const ReadBytes& source, const WriteBytes& socket,
                  uint64_t size, const ProgressBytes& progress,
                  const IsCancelled& cancelled) {
 char buf[1460]; uint64_t transferred=0;
 while(transferred<size) {
  if(cancelled && cancelled()) throw std::runtime_error("MRIM transfer cancelled");
  const size_t count=static_cast<size_t>(std::min<uint64_t>(size-transferred,sizeof(buf)));
  const int n=source(buf,count);
  if(n<=0 || static_cast<size_t>(n)>count) throw std::runtime_error("MRIM source file truncated");
  sendAll(socket,buf,static_cast<size_t>(n),cancelled);
  transferred+=static_cast<unsigned>(n);
  if(progress) progress(transferred);
 }
}
void receiveFileBody(const ReadBytes& socket, const WriteBytes& destination,
                     uint64_t size, const ProgressBytes& progress,
                     const IsCancelled& cancelled) {
 char buf[1460]; uint64_t transferred=0;
 while(transferred<size) {
  if(cancelled && cancelled()) throw std::runtime_error("MRIM transfer cancelled");
  const size_t count=static_cast<size_t>(std::min<uint64_t>(size-transferred,sizeof(buf)));
  const int n=socket(buf,count);
  if(n<=0 || static_cast<size_t>(n)>count) throw std::runtime_error("MRIM remote file truncated");
  sendAll(destination,buf,static_cast<size_t>(n),cancelled);
  transferred+=static_cast<unsigned>(n);
  if(progress) progress(transferred);
 }
}
}
