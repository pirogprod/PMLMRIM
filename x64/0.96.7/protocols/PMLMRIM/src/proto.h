#pragma once
#include <map>
#include <set>
#include <atomic>
#include <string>
#include <memory>
#include <vector>
#include "mrim_file_stream.h"

// Shared between proto.cpp (definition) and server.cpp (status parser).
bool MrimIsSupportedXStatus(const std::string& uri);
class CMrimProto : public PROTO<CMrimProto>
{
 friend class CMrimAccountDlg;
 HNETLIBCONN m_hConn = nullptr;
 mir_cs m_csSend;
 mir_cs m_csAvatar;
 std::set<MCONTACT> m_avatarPending;
 void __cdecl AvatarWorker(void*);
 INT_PTR MIR_CDECL OnAvatarCaps(WPARAM, LPARAM);
 INT_PTR MIR_CDECL OnGetAvatarInfo(WPARAM, LPARAM);
 std::atomic<bool> m_stop{false};
 bool m_online = false;
 unsigned m_pingInterval = 5;
 std::atomic<uint32_t> m_seq{1};
 mir_cs m_csPending;
 // A transfer is referenced by its MRIM id and its Miranda process handle.
 struct FileNegotiation {
  MCONTACT contact = 0;
  uint32_t id = 0;
  std::string peer, addresses;
  std::vector<pmlmrim::TransferItem> files;
  std::vector<std::wstring> paths;
  std::wstring directory;
  std::atomic<bool> cancelled{false}, accepted{false}, denied{false};
  bool incoming = false;
  uintptr_t listener = 0;
  std::atomic<uintptr_t> socket{0};
 };
 mir_cs m_csFile;
 std::map<uintptr_t, std::shared_ptr<FileNegotiation>> m_fileNegotiations;
 void __cdecl FileWorker(void*);
 void StopFileTransfers();
 void HandleIncomingFileOffer(const pmlmrim::FileOffer&);
 void HandleFileOfferAck(const pmlmrim::FileOfferAck&);
 std::map<uint32_t, MCONTACT> m_pendingMessages;
 std::map<uint32_t, MCONTACT> m_pendingProfiles;
 // One entry per real MRIM WP_REQUEST. Guarded by m_csPending.
 struct PendingSearch { HANDLE handle; std::wstring email; DWORD started; };
 std::map<uint32_t, PendingSearch> m_pendingSearches;

 HGENMENU m_hXStatusMenu = nullptr;
 std::wstring m_xStatusMenuLabel;
 void EnsureXStatusMenu();
 void ApplyXStatusIcon(MCONTACT);
 static int __cdecl RebuildXStatusIcons(WPARAM, LPARAM);
 static int __cdecl ApplyXStatusIcons(WPARAM, LPARAM);
 int __cdecl OnPreBuildStatusMenu(WPARAM, LPARAM);
 INT_PTR __cdecl OnChooseXStatus(WPARAM, LPARAM);
 int __cdecl OnUserInfoInit(WPARAM,LPARAM);
 void __cdecl ServerThread(void*);
 void Shutdown();
 void __cdecl ProfileRequestThread(void*);
 bool SendPacket(const pmlmrim::Bytes&);
 void HandlePacket(const pmlmrim::Packet&);
 MCONTACT FindContact(const char*);
 MCONTACT EnsureContact(const char*);
 int ToMirandaStatus(uint32_t);
 uint32_t ToMrimStatus(int);
public:
 CMrimProto(const char*,const wchar_t*);
 ~CMrimProto();
 CMOption<wchar_t*> m_server;
 CMOption<int> m_port;
 // Separate legacy MRIM avatar (obraz) server.
 CMOption<wchar_t*> m_avatarServer;
 CMOption<int> m_avatarPort;
 MCONTACT AddToList(int,PROTOSEARCHRESULT*) override;
 HANDLE SearchBasic(const wchar_t*) override;
 HANDLE SearchByEmail(const wchar_t*) override;
 INT_PTR GetCaps(int,MCONTACT = 0) override;
 int SendMsg(MCONTACT,MEVENT,const char*) override;
 HANDLE SendFile(MCONTACT, const wchar_t*, wchar_t**) override;
 HANDLE FileAllow(MCONTACT, HANDLE, const wchar_t*) override;
 int FileDeny(MCONTACT, HANDLE, const wchar_t*) override;
 int FileCancel(MCONTACT, HANDLE) override;
 int Authorize(MEVENT) override;
 int AuthDeny(MEVENT,const wchar_t*) override;
 int AuthRequest(MCONTACT,const wchar_t*) override;
 int GetInfo(MCONTACT,int) override;
 int SetStatus(int) override;
 // Called by a future status-menu controller; no menu is registered yet.
 bool SetExtendedStatus(const std::string& uri, const std::string& title,
                        const std::string& description);
 MWindow OnCreateAccMgrUI(MWindow) override;
 void OnModulesLoaded() override;
 void OnShutdown() override;
 int __cdecl OnOptionsInit(WPARAM,LPARAM);
};
struct CMPlugin : public ACCPROTOPLUGIN<CMrimProto> {
 CMPlugin();
 int Load() override;
};
