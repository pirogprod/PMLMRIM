#include "mrim_file_session.h"
#include <stdexcept>
namespace pmlmrim {
namespace {
void sendCommand(const WriteBytes& out,const std::string& command,const IsCancelled& cancelled){
 sendAll(out,command.data(),command.size(),cancelled);
}
void ensureUnique(const std::vector<TransferItem>& files){
 if(files.empty() || files.size()>1024) throw std::invalid_argument("invalid file count");
 // Reusing the codec ensures every filename is wire-safe.
 (void)makeFileList(files);
 for(size_t i=0;i<files.size();++i)
  for(size_t j=0;j<i;++j)
   if(files[i].name==files[j].name) throw std::invalid_argument("duplicate file name");
}
}
void serveFileRequests(const ReadBytes& socketRead,const WriteBytes& socketWrite,
 const std::string& senderEmail,const std::string& recipientEmail,
 const std::vector<TransferItem>& files,
 const OpenRead& openSource,const ItemProgress& onProgress,const IsCancelled& cancelled){
 ensureUnique(files);
 if(!openSource) throw std::invalid_argument("missing source opener");
 sendCommand(socketWrite,makeFileHello(senderEmail),cancelled);
 if(!matchesFileHello(receiveCommand(socketRead),recipientEmail))
  throw std::runtime_error("MRIM file hello mismatch");
 for(size_t i=0;i<files.size();++i){
  if(!matchesFileGet(receiveCommand(socketRead),files[i].name))
   throw std::runtime_error("unexpected MRIM file request");
  auto source=openSource(files[i]);
  if(!source) throw std::runtime_error("MRIM source could not be opened");
  sendFileBody(source,socketWrite,files[i].size,
   [&](uint64_t n){if(onProgress)onProgress(i,n);},cancelled);
 }
}
void requestFiles(const ReadBytes& socketRead,const WriteBytes& socketWrite,
 const std::string& senderEmail,const std::string& recipientEmail,
 const std::vector<TransferItem>& files,
 const OpenWrite& openDestination,const ItemProgress& onProgress,const IsCancelled& cancelled){
 ensureUnique(files);
 if(!openDestination) throw std::invalid_argument("missing destination opener");
 if(!matchesFileHello(receiveCommand(socketRead),senderEmail))
  throw std::runtime_error("MRIM sender hello mismatch");
 sendCommand(socketWrite,makeFileHello(recipientEmail),cancelled);
 for(size_t i=0;i<files.size();++i){
  auto destination=openDestination(files[i]);
  if(!destination) throw std::runtime_error("MRIM destination could not be opened");
  sendCommand(socketWrite,makeFileGet(files[i].name),cancelled);
  receiveFileBody(socketRead,destination,files[i].size,
   [&](uint64_t n){if(onProgress)onProgress(i,n);},cancelled);
 }
}
}
