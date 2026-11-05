#include "stdafx.h"
#include <winhttp.h>
#include <ctime>
#include <memory>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

namespace {
struct WinHandle {
 HINTERNET h;
 explicit WinHandle(HINTERNET value = nullptr) : h(value) {}
 ~WinHandle() { if (h) WinHttpCloseHandle(h); }
 operator bool() const { return h != nullptr; }
};

// Use a restricted email alphabet to prevent URL-path injection.
bool AvatarPathParts(const char *email, std::wstring &domain, std::wstring &name)
{
 if (!email) return false;
 std::string address(email);
 size_t at = address.find('@');
 if (at == std::string::npos || address.find('@', at + 1) != std::string::npos) return false;
 std::string local = address.substr(0, at), mailDomain = address.substr(at + 1);
 if (local.empty() || local.size() > 80 || mailDomain.empty() || mailDomain.size() > 100) return false;
 auto valid = [](char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '.' || ch == '-'; };
 if (!std::all_of(local.begin(), local.end(), valid) || !std::all_of(mailDomain.begin(), mailDomain.end(), valid)) return false;
 size_t dot = mailDomain.find('.');
 if (dot == std::string::npos || dot == 0) return false;
 name.assign(local.begin(), local.end());
 domain.assign(mailDomain.begin(), mailDomain.begin() + dot);
 return true;
}

std::wstring AvatarCacheFile(const char *module, const char *email)
{
 wchar_t base[MAX_PATH] = {};
 DWORD size = GetTempPathW(MAX_PATH, base);
 if (!size || size >= MAX_PATH) return {};
 std::wstring dir(base);
 dir += L"PMLMRIM-avatars";
 CreateDirectoryW(dir.c_str(), nullptr);
 dir += L"\\";
 if (module) {
  for (const char *p = module; *p; ++p) {
   const unsigned char ch = (unsigned char)*p;
   if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_') dir.push_back((wchar_t)ch);
  }
 }
 CreateDirectoryW(dir.c_str(), nullptr);
 // Cache key depends on the email, not the database contact handle, which
 // can be reused after an account is removed/re-added.
 unsigned long long hash = 14695981039346656037ull;
 for (const char *p = email ? email : ""; *p; ++p) {
  unsigned char c = (unsigned char)*p;
  if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + ('a' - 'A'));
  hash = (hash ^ c) * 1099511628211ull;
 }
 wchar_t suffix[64];
 swprintf_s(suffix, L"\\%016llx.jpg", hash);
 return dir + suffix;
}

bool DownloadAvatar(const wchar_t *server, int port, const std::wstring &domain, const std::wstring &user, std::vector<unsigned char> &data, bool &noImage)
{
 noImage = false;
 if (!server || !*server || port < 1 || port > 65535) return false;
 WinHandle session(WinHttpOpen(L"PMLMRIM-BETA3/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
 if (!session) return false;
 WinHttpSetTimeouts(session.h, 3000, 3000, 5000, 5000);
 WinHandle conn(WinHttpConnect(session.h, server, (INTERNET_PORT)port, 0));
 if (!conn) return false;
 std::wstring path = L"/" + domain + L"/" + user + L"/_mrimavatar";
 WinHandle req(WinHttpOpenRequest(conn.h, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0));
 if (!req || !WinHttpSendRequest(req.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(req.h, nullptr)) return false;
 DWORD status = 0, sz = sizeof(status);
 if (!WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &sz, nullptr) || status != 200) return false;
 wchar_t emptyHeader[64] = {};
 sz = sizeof(emptyHeader);
 if (WinHttpQueryHeaders(req.h, WINHTTP_QUERY_CUSTOM, L"X-NoImage", emptyHeader, &sz, nullptr)) {
  if (emptyHeader[0] == L'1') { noImage = true; return true; }
 }
 wchar_t contentType[80] = {};
 sz = sizeof(contentType);
 if (!WinHttpQueryHeaders(req.h, WINHTTP_QUERY_CONTENT_TYPE, nullptr, contentType, &sz, nullptr) || _wcsnicmp(contentType, L"image/jpeg", 10)) return false;
 constexpr size_t MaxAvatar = 512 * 1024;
 for (;;) {
  unsigned char buffer[8192]; DWORD got = 0;
  if (!WinHttpReadData(req.h, buffer, sizeof(buffer), &got)) return false;
  if (!got) break;
  if (data.size() + got > MaxAvatar) return false;
  data.insert(data.end(), buffer, buffer + got);
 }
 return data.size() >= 4 && data[0] == 0xff && data[1] == 0xd8 && data[data.size()-2] == 0xff && data.back() == 0xd9;
}
}

INT_PTR MIR_CDECL CMrimProto::OnAvatarCaps(WPARAM wParam, LPARAM lParam)
{
 switch (wParam) {
 case AF_ENABLED: return 1;
 case AF_MAXSIZE:
  if (lParam) { POINT *size = reinterpret_cast<POINT *>(lParam); size->x = 90; size->y = 90; }
  return 0;
 case AF_PROPORTION: return PIP_SQUARE;
 case AF_MAXFILESIZE: return 512 * 1024;
 case AF_FORMATSUPPORTED: return lParam == PA_FORMAT_JPEG;
 case AF_FETCHIFCONTACTOFFLINE: return 1;
 }
 return 0;
}

INT_PTR MIR_CDECL CMrimProto::OnGetAvatarInfo(WPARAM, LPARAM lParam)
{
 auto *info = reinterpret_cast<PROTO_AVATAR_INFORMATION *>(lParam);
 if (!info || !info->hContact) return GAIR_NOAVATAR;
 MCONTACT contact = info->hContact;
 CMStringA email = getMStringA(contact, "Email");
 std::wstring domain, name;
 if (!AvatarPathParts(email.c_str(), domain, name)) return GAIR_NOAVATAR;
 std::wstring path = AvatarCacheFile(m_szModuleName, email.c_str());
 if (path.empty() || path.size() >= MAX_PATH) return GAIR_NOAVATAR;
 DWORD fileAttr = GetFileAttributesW(path.c_str());
 if (fileAttr != INVALID_FILE_ATTRIBUTES && !(fileAttr & FILE_ATTRIBUTE_DIRECTORY)) {
  wcscpy_s(info->filename, _countof(info->filename), path.c_str());
  info->format = PA_FORMAT_JPEG;
  // Keep showing the cached image while refreshing it in the background.
  const DWORD now = (DWORD)time(nullptr);
  const DWORD lastCheck = getDword(contact, "AvatarLastCheck", 0);
  if (!lastCheck || now < lastCheck || now - lastCheck >= 7 * 24 * 3600) {
   mir_cslock lock(m_csAvatar);
   if (!m_avatarPending.count(contact)) {
    m_avatarPending.insert(contact);
    ForkThread(&CMrimProto::AvatarWorker, new MCONTACT(contact));
   }
  }
  return GAIR_SUCCESS;
 }
 // Throttle missing/failed avatars as well as duplicate AVS calls.
 const DWORD now = (DWORD)time(nullptr);
 const DWORD lastCheck = getDword(contact, "AvatarLastCheck", 0);
 if (lastCheck && now >= lastCheck && now - lastCheck < 6 * 3600) return GAIR_NOAVATAR;
 {
  mir_cslock lock(m_csAvatar);
  if (m_avatarPending.count(contact)) return GAIR_WAITFOR;
  m_avatarPending.insert(contact);
 }
 ForkThread(&CMrimProto::AvatarWorker, new MCONTACT(contact));
 return GAIR_WAITFOR;
}

void __cdecl CMrimProto::AvatarWorker(void *arg)
{
 std::unique_ptr<MCONTACT> key((MCONTACT *)arg);
 if (!key) return;
 MCONTACT contact = *key;
 const CMStringA email = getMStringA(contact, "Email");
 std::wstring domain, name;
 std::vector<unsigned char> jpeg;
 bool noImage = false;
 bool ok = false;
 if (AvatarPathParts(email.c_str(), domain, name)) {
  const wchar_t *host = m_avatarServer;
  ok = DownloadAvatar(host, m_avatarPort, domain, name, jpeg, noImage);
 }
 const std::wstring path = AvatarCacheFile(m_szModuleName, email.c_str());
 bool saved = false;
 if (ok && !noImage && !path.empty()) {
  // Atomic replace: never let AVS observe a partial JPEG.
  std::wstring tmp = path + L".tmp";
  HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (f != INVALID_HANDLE_VALUE) {
   DWORD written = 0;
   saved = WriteFile(f, jpeg.data(), (DWORD)jpeg.size(), &written, nullptr) && written == jpeg.size();
   CloseHandle(f);
   if (saved) saved = MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
   if (!saved) DeleteFileW(tmp.c_str());
  }
 }
 // Don't retry unavailable avatars on every contact-list repaint.
 if (ok || noImage) setDword(contact, "AvatarLastCheck", (DWORD)time(nullptr));
 {
  mir_cslock lock(m_csAvatar);
  m_avatarPending.erase(contact);
 }
 if (saved && path.size() < MAX_PATH) {
  PROTO_AVATAR_INFORMATION info = {};
  info.hContact = contact;
  info.format = PA_FORMAT_JPEG;
  wcscpy_s(info.filename, _countof(info.filename), path.c_str());
  ProtoBroadcastAck(contact, ACKTYPE_AVATAR, ACKRESULT_SUCCESS, (HANDLE)&info);
 }
 else if (noImage && !path.empty()) {
  DeleteFileW(path.c_str());
  ProtoBroadcastAck(contact, ACKTYPE_AVATAR, ACKRESULT_FAILED, nullptr);
 }
}
