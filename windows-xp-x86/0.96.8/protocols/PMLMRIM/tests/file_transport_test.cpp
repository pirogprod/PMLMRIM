#include "../src/mrim_file_transport.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <string>
int main() {
 const std::string content(100000,'x');
 std::string transferred;
 size_t sourcePos=0; uint64_t progress=0;
 pmlmrim::sendFileBody([&](char* dst,size_t n)->int {
  size_t count=std::min<size_t>({n,37,content.size()-sourcePos});
  memcpy(dst,content.data()+sourcePos,count);sourcePos+=count;return (int)count;
 },[&](const char* p,size_t n)->int {
  size_t count=std::min<size_t>(n,19);transferred.append(p,count);return (int)count;
 },content.size(),[&](uint64_t count){progress=count;});
 assert(transferred==content && progress==content.size());
 std::string received; size_t inPos=0;
 pmlmrim::receiveFileBody([&](char* b,size_t n)->int {
  size_t count=std::min<size_t>({n,11,transferred.size()-inPos});
  memcpy(b,transferred.data()+inPos,count);inPos+=count;return (int)count;
 },[&](const char* b,size_t n)->int {
  size_t count=std::min<size_t>(n,7);received.append(b,count);return (int)count;
 },transferred.size());
 assert(received==content);
 size_t cmdPos=0; const std::string cmd("MRA_FT_HELLO alice@example.com\0", sizeof("MRA_FT_HELLO alice@example.com\0")-1);
 const auto result=pmlmrim::receiveCommand([&](char* b,size_t)->int {
  if(cmdPos>=cmd.size()) return 0;
  *b=cmd[cmdPos++];return 1;
 });
 assert(result==cmd);
 bool failed=false;
 try {pmlmrim::receiveFileBody([](char*,size_t){return 0;},[](const char*,size_t){return 1;},10);}
 catch(const std::runtime_error&) {failed=true;}
 assert(failed);
 failed=false;
 try {pmlmrim::sendFileBody([](char*,size_t){return 0;},[](const char*,size_t){return 1;},10);}
 catch(const std::runtime_error&) {failed=true;}
 assert(failed);
 failed=false;
 try {pmlmrim::receiveCommand([](char* b,size_t){*b='a';return 1;},4);}
 catch(const std::length_error&) {failed=true;}
 assert(failed);
}
