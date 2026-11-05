#include "mrim_file_stream.h"
#include <stdexcept>
#include <limits>
#include <cctype>
namespace pmlmrim {
namespace {
void checkName(const std::string& name) {
 if (name.empty() || name.size() > 255 || name == "." || name == ".." ||
     name.find_first_of("/\\;:\r\n\0", 0, 6) != std::string::npos ||
     name.find("..") != std::string::npos)
  throw std::invalid_argument("unsafe MRIM filename");
 for (unsigned char c: name) if (c < 32 || c == 127) throw std::invalid_argument("invalid filename control character");
}
std::string command(const char* prefix, const std::string& arg) {
 if (arg.empty() || arg.size() > 1024 || arg.find('\0') != std::string::npos ||
     arg.find('\n') != std::string::npos || arg.find('\r') != std::string::npos)
  throw std::invalid_argument("invalid MRIM transfer argument");
 std::string result(prefix); result += arg; result += '\0'; return result;
}
bool matches(const std::string& wire, const char* prefix, const std::string& arg) {
 try { return wire == command(prefix, arg); } catch (...) { return false; }
}
}
std::vector<TransferItem> parseFileList(const std::string& list) {
 if (list.empty() || list.size() > 1024 * 1024) throw std::invalid_argument("invalid file list length");
 std::vector<TransferItem> files; size_t pos=0;
 while (pos < list.size()) {
  const auto endName=list.find(';',pos); if(endName==std::string::npos) throw std::invalid_argument("missing file name delimiter");
  std::string name=list.substr(pos,endName-pos); checkName(name);
  const auto endSize=list.find(';',endName+1); if(endSize==std::string::npos || endSize==endName+1) throw std::invalid_argument("missing file size");
  uint64_t n=0;
  for(size_t i=endName+1;i<endSize;i++) {
   unsigned char c=list[i]; if(c<'0'||c>'9') throw std::invalid_argument("non-numeric file size");
   n=n*10+(c-'0'); if(n>std::numeric_limits<uint32_t>::max()) throw std::out_of_range("file size exceeds MRIM 32-bit limit");
  }
  if(files.size()>=1024) throw std::length_error("too many files");
  files.push_back({name,static_cast<uint32_t>(n)}); pos=endSize+1;
 }
 return files;
}
std::string makeFileList(const std::vector<TransferItem>& items) {
 if(items.empty()||items.size()>1024) throw std::invalid_argument("invalid number of files");
 std::string out;
 for(const auto& item:items) {checkName(item.name);out+=item.name+';'+std::to_string(item.size)+';';
  if(out.size()>1024*1024) throw std::length_error("file list too long");}
 return out;
}
std::string makeFileHello(const std::string& email) {return command("MRA_FT_HELLO ",email);}
std::string makeFileGet(const std::string& filename) {checkName(filename);return command("MRA_FT_GET_FILE ",filename);}
bool matchesFileHello(const std::string& wire,const std::string& email) {return matches(wire,"MRA_FT_HELLO ",email);}
bool matchesFileGet(const std::string& wire,const std::string& filename) {return matches(wire,"MRA_FT_GET_FILE ",filename);}
}
