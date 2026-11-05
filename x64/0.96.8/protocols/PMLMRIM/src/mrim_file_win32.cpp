#include "mrim_file_win32.h"
#include <stdexcept>
#include <memory>
#include <limits>
#include <climits>
namespace pmlmrim {
namespace {
struct HandleCloser { void operator()(void* p) const { if(p && p!=INVALID_HANDLE_VALUE) CloseHandle(p); } };
using UniqueHandle=std::unique_ptr<void,HandleCloser>;
UniqueHandle openFile(const std::wstring& path, bool write) {
 HANDLE h=CreateFileW(path.c_str(),write?GENERIC_WRITE:GENERIC_READ,
  write?0:FILE_SHARE_READ,nullptr,write?CREATE_NEW:OPEN_EXISTING,
  FILE_ATTRIBUTE_NORMAL,nullptr);
 if(h==INVALID_HANDLE_VALUE) throw std::runtime_error("MRIM file open failed");
 return UniqueHandle(h);
}
ReadBytes sockReader(SOCKET socket) {
 return [socket](char* p,size_t n)->int {
  if(n>static_cast<size_t>((std::numeric_limits<int>::max)())) n=INT_MAX;
  return recv(socket,p,static_cast<int>(n),0);
 };
}
WriteBytes sockWriter(SOCKET socket) {
 return [socket](const char* p,size_t n)->int {
  if(n>static_cast<size_t>((std::numeric_limits<int>::max)())) n=INT_MAX;
  return send(socket,p,static_cast<int>(n),0);
 };
}
ReadBytes diskReader(HANDLE file) {
 return [file](char* p,size_t n)->int {
  DWORD count=0;
  if(!ReadFile(file,p,static_cast<DWORD>(n),&count,nullptr)) return -1;
  return static_cast<int>(count);
 };
}
WriteBytes diskWriter(HANDLE file) {
 return [file](const char* p,size_t n)->int {
  DWORD count=0;
  if(!WriteFile(file,p,static_cast<DWORD>(n),&count,nullptr)) return -1;
  return static_cast<int>(count);
 };
}
std::wstring wideName(const std::string& text) {
 if(text.empty() || text.size()>255 || text.find_first_of("/\\:")!=std::string::npos ||
    text=="." || text=="..") throw std::runtime_error("unsafe remote filename");
 int needed=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),
  static_cast<int>(text.size()),nullptr,0);
 if(!needed) throw std::runtime_error("bad UTF-8 filename");
 std::wstring wide(needed,L'\0');
 if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),
  static_cast<int>(text.size()),&wide[0],needed)) throw std::runtime_error("invalid filename");
 if(wide.back()==L'.' || wide.back()==L' ' || wide.find_first_of(L"<>\"|?*")!=std::wstring::npos)
  throw std::runtime_error("unsafe Windows filename");
 return wide;
}
}
void sendFilesOnSocket(SOCKET socket,const std::string& sender,const std::string& receiver,
 const std::vector<TransferItem>& files,const std::vector<std::wstring>& paths,
 const ItemProgress& progress,const IsCancelled& cancelled) {
 if(files.size()!=paths.size()) throw std::invalid_argument("file/path count mismatch");
 // Keep source handles alive for the entire transfer, including the callbacks.
 std::vector<UniqueHandle> handles;
 handles.reserve(files.size());
 for(size_t i=0;i<files.size();++i) {
  auto h=openFile(paths[i],false);
  LARGE_INTEGER actual{};
  if(!GetFileSizeEx(h.get(),&actual) || actual.QuadPart<0 ||
    static_cast<uint64_t>(actual.QuadPart)!=files[i].size)
   throw std::runtime_error("source file size changed");
  handles.push_back(std::move(h));
 }
 serveFileRequests(sockReader(socket),sockWriter(socket),sender,receiver,files,
  [&](const TransferItem& file)->ReadBytes {
   for(size_t i=0;i<files.size();++i) if(&file==&files[i]) return diskReader(handles[i].get());
   throw std::runtime_error("unknown source item");
  },progress,cancelled);
}
void receiveFilesOnSocket(SOCKET socket,const std::string& sender,const std::string& receiver,
 const std::vector<TransferItem>& files,const std::wstring& directory,
 const ItemProgress& progress,const IsCancelled& cancelled) {
 if(directory.empty()) throw std::invalid_argument("missing destination directory");
 std::vector<UniqueHandle> handles;
 std::vector<std::wstring> created;
 try {
  requestFiles(sockReader(socket),sockWriter(socket),sender,receiver,files,
   [&](const TransferItem& file)->WriteBytes {
    std::wstring path=directory;
    if(path.back()!=L'\\' && path.back()!=L'/') path+=L'\\';
    path+=wideName(file.name);
    auto h=openFile(path,true); // CREATE_NEW: never overwrite or silently follow existing files
    created.push_back(path);
    HANDLE raw=h.get(); handles.push_back(std::move(h));
    return diskWriter(raw);
   },progress,cancelled);
 }
 catch(...) {
  handles.clear();
  for(const auto& path:created) DeleteFileW(path.c_str());
  throw;
 }
}
}
