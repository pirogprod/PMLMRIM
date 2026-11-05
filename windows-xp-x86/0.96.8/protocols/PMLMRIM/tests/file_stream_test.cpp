#include "../src/mrim_file_stream.h"
#include <cassert>
#include <stdexcept>
int main() {
 using namespace pmlmrim;
 auto f=parseFileList("one.txt;0;two.bin;1024;");
 assert(f.size()==2 && f[0].size==0 && f[1].size==1024);
 assert(makeFileList(f)=="one.txt;0;two.bin;1024;");
 assert(matchesFileHello(makeFileHello("a@b.ru"),"a@b.ru"));
 assert(matchesFileGet(makeFileGet("one.txt"),"one.txt"));
 assert(!matchesFileGet(makeFileGet("one.txt"),"two.txt"));
 for (auto bad: {"../secret;4;", "a;4294967296;", "a;x;", "a;9", "dir\\file;2;", "a;1;b"}) {
  bool threw=false;try{parseFileList(bad);}catch(const std::exception&){threw=true;}assert(threw);
 }
 return 0;
}
