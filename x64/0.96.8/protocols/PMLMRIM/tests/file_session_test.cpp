#include "../src/mrim_file_session.h"
#include <cassert>
#include <string>
#include <stdexcept>
#include <iostream>
using namespace pmlmrim;
int main(){
 std::vector<TransferItem> items={{"alpha.txt",5},{"beta.dat",8}};
 std::vector<std::string> payload={"hello","12345678"}, received(2);
 // The two independent phases emulate full-duplex command + content streams.
 std::string requests, sent;
 WriteBytes sendRequests=[&](const char* p,size_t n){requests.append(p,n);return (int)n;};
 ReadBytes empty=[](char*,size_t){return -1;};
 // Generate protocol request bytes, with deliberately incomplete body to assert failure.
 bool truncated=false;
 try {requestFiles(empty,sendRequests,"sender@example.org","recipient@example.org",items,
  [&](const TransferItem& item)->WriteBytes {size_t i=item.name=="alpha.txt"?0:1;
   return [&,i](const char* p,size_t n){received[i].append(p,n);return (int)n;};});}
 catch(const std::runtime_error&){truncated=true;}
 assert(truncated);
 // Build properly ordered command bytes for the sender.
 requests=makeFileHello("recipient@example.org")+makeFileGet("alpha.txt")+makeFileGet("beta.dat");
 size_t cmdpos=0;
 ReadBytes readCommands=[&](char* p,size_t n){if(cmdpos+n>requests.size())return -1;
  for(size_t j=0;j<n;j++){p[j]=requests[cmdpos++];} return (int)n;};
 WriteBytes writeData=[&](const char* p,size_t n){size_t k=n>2?2:n;sent.append(p,k);return (int)k;};
 size_t opened=0;
 serveFileRequests(readCommands,writeData,"sender@example.org","recipient@example.org",items,
  [&](const TransferItem& item)->ReadBytes {size_t i=opened++,offset=0;
   assert(item.name==items[i].name);
   return [&,i,offset](char* p,size_t n) mutable {size_t count=std::min(n,payload[i].size()-offset);
    for(size_t j=0;j<count;j++){p[j]=payload[i][offset++];} return (int)count;};});
 assert(sent==makeFileHello("sender@example.org")+"hello12345678");
 // Receiver consumes the sender's bytes in one-byte fragmented reads.
 size_t inpos=0; ReadBytes input=[&](char* p,size_t){if(inpos==sent.size())return 0;
  p[0]=sent[inpos++];return 1;};
 requests.clear(); received={"",""};
 requestFiles(input,sendRequests,"sender@example.org","recipient@example.org",items,
  [&](const TransferItem& item)->WriteBytes {size_t i=item.name=="alpha.txt"?0:1;
   return [&,i](const char* p,size_t n){received[i].append(p,n);return (int)n;};});
 assert(received==payload);
 assert(requests==makeFileHello("recipient@example.org")+makeFileGet("alpha.txt")+makeFileGet("beta.dat"));
 bool rejected=false;
 try {serveFileRequests(readCommands,writeData,"sender@example.org","other@example.org",items,
  [&](const TransferItem&)->ReadBytes{return empty;});}catch(const std::exception&){rejected=true;}
 assert(rejected);
 std::cout<<"MRIM file session tests passed\n";
}
