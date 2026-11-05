#include "stdafx.h"
#include <memory>
#include <algorithm>
#include <cctype>
#include <m_userinfo.h>
// One shared extra-icon slot for all PMLMRIM accounts.
static HANDLE g_mrimXIconSlot = nullptr;
static HANDLE g_mrimXImages[105] = {}; // Server identifiers status_0 through status_104
static std::vector<CMrimProto*> g_mrimXAccounts;

static int MrimXStatusResource(const char *uri);
bool MrimIsSupportedXStatus(const std::string& uri);

int __cdecl CMrimProto::RebuildXStatusIcons(WPARAM, LPARAM)
{
 for (int id=1; id<=104; ++id) {
  g_mrimXImages[id]=nullptr;
  char uri[32]; mir_snprintf(uri, "status_%d", id);
  if (!MrimIsSupportedXStatus(uri)) continue; // load icons only for catalogued statuses
  int resource=MrimXStatusResource(uri);
  if (resource == 33004) continue;
  HICON icon=(HICON)LoadImageW(g_plugin.getInst(),MAKEINTRESOURCEW(resource),IMAGE_ICON,16,16,LR_DEFAULTCOLOR);
  if (icon) { g_mrimXImages[id]=ExtraIcon_AddIcon(icon); DestroyIcon(icon); }
 }
 return 0;
}

void CMrimProto::ApplyXStatusIcon(MCONTACT hContact)
{
 if (!g_mrimXIconSlot || !hContact) return;
 // Match PMLICQ: INVALID_HANDLE_VALUE means NO extra icon.
 // ExtraIcon_Clear is not used: explicit SetIcon removal avoids default slot images.
 ExtraIcon_SetIcon(g_mrimXIconSlot, hContact, INVALID_HANDLE_VALUE);
 const WORD status = getWord(hContact, "Status", ID_STATUS_OFFLINE);
 const BYTE active = getByte(hContact, "XStatusActive", 0);
 if (status == ID_STATUS_OFFLINE || !active) {
  debugLogA("MRIM XICON: cleared contact=%llu offline=%d active=%u",
   (unsigned long long)hContact, status == ID_STATUS_OFFLINE, (unsigned)active);
  return;
 }
 auto uri = getMStringA(hContact, "XStatusUri");
 if (uri.IsEmpty() || !MrimIsSupportedXStatus(uri.c_str())) {
  debugLogA("MRIM XICON: cleared contact=%llu unsupported-or-empty-uri",
   (unsigned long long)hContact);
  return;
 }
 const char *value = uri.c_str();
 int id = -1;
 if (!_strnicmp(value, "status_", 7)) {
  char *end = nullptr;
  long v = strtol(value + 7, &end, 10);
  if (end && *end == 0 && v >= 1 && v <= 104) id = (int)v;
 }
 // Generic fallback is intentionally forbidden: if this URI has no matching
 // icon resource, the contact gets NO extra icon, not an unrelated smiley.
 HANDLE image = id > 0 ? g_mrimXImages[id] : nullptr;
 if (!image || image == INVALID_HANDLE_VALUE) {
  debugLogA("MRIM XICON: cleared contact=%llu missing-image uri=%s",
   (unsigned long long)hContact, value);
  return;
 }
 // Show only the supported, active server-provided X-status icon.
 ExtraIcon_SetIcon(g_mrimXIconSlot, hContact, image);
 debugLogA("MRIM XICON: set contact=%llu uri=%s", (unsigned long long)hContact, value);
}

int __cdecl CMrimProto::ApplyXStatusIcons(WPARAM wParam, LPARAM)
{
 MCONTACT hContact=(MCONTACT)wParam;
 if (!hContact) return 0;
 const char* account=Proto_GetBaseAccountName(hContact);
 if (!account) return 0;
 for (auto* proto:g_mrimXAccounts)
  if (!mir_strcmp(account, proto->m_szModuleName)) { proto->ApplyXStatusIcon(hContact); break; }
 return 0;
}

CMrimProto::CMrimProto(const char* module,const wchar_t* user):PROTO<CMrimProto>(module,user),m_server(this,"Server",L"mrim.mail.ru"),m_port(this,"Port",2042),m_avatarServer(this,"AvatarServer",L"obraz.foto.mail.ru"),m_avatarPort(this,"AvatarPort",80){
 NETLIBUSER n={};n.flags=NUF_OUTGOING|NUF_UNICODE;n.szSettingsModule=m_szModuleName;n.szDescriptiveName.w=m_tszUserName;m_hNetlibUser=Netlib_RegisterUser(&n);
 HookProtoEvent(ME_OPT_INITIALISE,&CMrimProto::OnOptionsInit);
 g_mrimXAccounts.push_back(this);
 CreateProtoService("/ChooseXStatus", &CMrimProto::OnChooseXStatus);
 CreateProtoService(PS_GETAVATARCAPS, &CMrimProto::OnAvatarCaps);
 CreateProtoService(PS_GETAVATARINFO, &CMrimProto::OnGetAvatarInfo);
 HookProtoEvent(ME_CLIST_PREBUILDSTATUSMENU, &CMrimProto::OnPreBuildStatusMenu);
}
CMrimProto::~CMrimProto(){Shutdown();g_mrimXAccounts.erase(std::remove(g_mrimXAccounts.begin(),g_mrimXAccounts.end(),this),g_mrimXAccounts.end());}
void CMrimProto::OnModulesLoaded(){
 setAllContactStatuses(ID_STATUS_OFFLINE);
 HookProtoEvent(ME_USERINFO_INITIALISE,&CMrimProto::OnUserInfoInit);
 EnsureXStatusMenu();
 if (!g_mrimXIconSlot) {
  g_mrimXIconSlot=ExtraIcon_RegisterCallback("pmlmrim_xstatus", "MRIM X-status", nullptr, RebuildXStatusIcons, ApplyXStatusIcons);
  if (g_mrimXIconSlot) RebuildXStatusIcons(0,0);
 }
 // Repair contacts imported by older builds as temporary/hidden. Only touch
 // contacts belonging to this protocol, not the user's other accounts.
 for(auto& c:AccContacts()){
  {
   if(getMStringA(c,"Email").IsEmpty()){
    CMStringA oldEmail=getMStringA(c,"e-mail");
    if(!oldEmail.IsEmpty())setString(c,"Email",oldEmail.c_str());
   }
   Contact::PutOnList(c);
   Contact::Hide(c,false);
   // Do not resurrect cached icons from the previous session.
   setByte(c, "XStatusActive", 0);
   ApplyXStatusIcon(c);
   db_unset(c,"CList","NotOnList");db_unset(c,"CList","Hidden");
  }
 }
}

// Canonical URI list extracted from the supplied Mra(2).dll pointer table.
// Deliberately no guessed title/icon associations: IDs, captions and resource
// numbers live in separate tables in that DLL. The original artificial
// status_1..status_104 range contained invalid / unrelated entries.
struct MrimXStatusChoice { const char* uri; const wchar_t* caption; };
static const MrimXStatusChoice kMrimXStatusChoices[] = {
 {"status_29", L"На своей волне"},
 {"status_30", L"Полный релакс"},
 {"status_32", L"Всё под контролем"},
 {"status_33", L"Улетаю мыслями"},
 {"status_34", L"Грустно"},
 {"status_35", L"Расстроен"},
 {"status_36", L"Удивлен"},
 {"status_37", L"Злой"},
 {"status_38", L"Творю хаос"},
 {"status_39", L"Жопа"},
 {"status_40", L"На позитиве"},
 {"status_41", L"В режиме невидимки"},
 {"status_42", L"Класс"},
 {"status_43", L"Все круто!"},
 {"status_44", L"Фига"},
 {"status_45", L"Отвали"},
 // BETA: Jimm 2010 MRIM catalog. Separate IDs, do not remap Agent icons.
 {"status_5", L"Дома"},
 {"status_18", L"На работе"},
 {"status_19", L"На встрече"},
 {"status_7", L"Где я?"},
 {"status_10", L"Гуляю"},
 {"status_47", L"Ракета"},
 {"status_22", L"Работаю"},
 {"status_26", L"Учусь"},
 {"status_24", L"Телефон"},
 {"status_27", L"В школе"},
 {"status_23", L"Сплю"},
 {"status_4", L"Болею"},
 {"status_9", L"Готовлю"},
 {"status_6", L"Ем"},
 {"status_21", L"Пью кофе"},
 {"status_20", L"Пиво"},
 {"status_17", L"Курю"},
 {"status_8", L"Туалет"},
 {"status_15", L"Утка"},
 {"status_16", L"Играю"},
 {"status_28", L"Вы ошиблись номером"},
 {"status_51", L"Белка"},
 {"status_52", L"Звезда"},
 {"status_46", L"Череп"},
 {"status_12", L"Я криветко!"},
 {"status_13", L"Потерялся"},
 {"status_11", L"Инопланетянин"},
 {"status_14", L"Влюблен"},
 {"status_48", L"Ктулху"},

};
// Only these advertised X-status IDs are supported by the current PMLMRIM menu.
// Do not expose unknown remote X-statuses in the contact list / Tipper.
bool MrimIsSupportedXStatus(const std::string& uri)
{
 for (const auto& choice : kMrimXStatusChoices)
  if (!_stricmp(choice.uri, uri.c_str())) return true;
 return false;
}

struct MrimStatusDialogState { const wchar_t* caption; std::wstring message; };
static INT_PTR CALLBACK MrimStatusMessageProc(HWND dlg, UINT message, WPARAM wp, LPARAM lp)
{
 if (message == WM_INITDIALOG) {
  auto* state = reinterpret_cast<MrimStatusDialogState*>(lp);
  SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)state);
  SetWindowTextW(dlg, L"Текст X-статуса PMLMRIM");
  HWND label = CreateWindowExW(0, L"STATIC", state->caption,
    WS_CHILD | WS_VISIBLE, 10, 8, 258, 18, dlg, nullptr, g_plugin.getInst(), nullptr);
  HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", state->message.c_str(),
    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
    10, 29, 258, 22, dlg, (HMENU)100, g_plugin.getInst(), nullptr);
  HWND ok = CreateWindowExW(0, L"BUTTON", L"Установить",
    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
    103, 60, 80, 23, dlg, (HMENU)IDOK, g_plugin.getInst(), nullptr);
  HWND cancel = CreateWindowExW(0, L"BUTTON", L"Отмена",
    WS_CHILD | WS_VISIBLE | WS_TABSTOP,
    188, 60, 80, 23, dlg, (HMENU)IDCANCEL, g_plugin.getInst(), nullptr);
  HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
  for (HWND ctl : {label, edit, ok, cancel}) SendMessageW(ctl, WM_SETFONT, (WPARAM)font, TRUE);
  SetFocus(edit);
  return FALSE;
 }
 if (message == WM_COMMAND) {
  if (LOWORD(wp) == IDOK) {
   auto* state = reinterpret_cast<MrimStatusDialogState*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
   if (state) {
    wchar_t buffer[513] = {};
    GetDlgItemTextW(dlg, 100, buffer, _countof(buffer));
    state->message = buffer;
   }
   EndDialog(dlg, IDOK);
   return TRUE;
  }
  if (LOWORD(wp) == IDCANCEL) { EndDialog(dlg, IDCANCEL); return TRUE; }
 }
 if (message == WM_CLOSE) { EndDialog(dlg, IDCANCEL); return TRUE; }
 return FALSE;
}
static bool MrimAskStatusMessage(HWND owner, const wchar_t* caption, std::wstring& text)
{
 // No dialog resource needed; valid on both x86 and x64.
 DLGTEMPLATE tmpl = {};
 tmpl.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
 tmpl.dwExtendedStyle = 0;
 tmpl.cdit = 0;
 tmpl.x = 0; tmpl.y = 0; tmpl.cx = 150; tmpl.cy = 50;
 // DLGTEMPLATE followed by menu=0, class=0, title=0.
 struct { DLGTEMPLATE dlg; WORD zero[3]; } data = {tmpl, {0,0,0}};
 MrimStatusDialogState state = {caption, text};
 const INT_PTR result = DialogBoxIndirectParamW(g_plugin.getInst(), &data.dlg, owner,
   MrimStatusMessageProc, (LPARAM)&state);
 if (result != IDOK) return false;
 text.swap(state.message);
 return true;
}

void CMrimProto::EnsureXStatusMenu()
{
 if (m_hXStatusMenu) return;
 HGENMENU root = Menu_GetProtocolRoot(this);
 if (!root) return;
 CMenuItem item(&g_plugin);
 item.root = root;
 item.pszService = "/ChooseXStatus";
 m_xStatusMenuLabel = L"X-status: ";
 const CMStringA initialUri = getMStringA("XStatusRequestedUri");
 bool found = false;
 for (const auto& choice : kMrimXStatusChoices)
  if (!mir_strcmpi(initialUri, choice.uri)) { m_xStatusMenuLabel += choice.caption; found = true; break; }
 if (!found) m_xStatusMenuLabel += L"Нет";
 item.flags |= CMIF_UNICODE;
 item.name.w = const_cast<wchar_t*>(m_xStatusMenuLabel.c_str()); // Miranda 0.96.7 CMenuItem expects mutable pointer
 item.position = 200100;
 m_hXStatusMenu = Menu_AddProtoMenuItem(&item, m_szModuleName);
}
int __cdecl CMrimProto::OnPreBuildStatusMenu(WPARAM, LPARAM)
{
 EnsureXStatusMenu();
 if (m_hXStatusMenu) {
  std::wstring label = L"X-status: ";
  const CMStringA currentUri = getMStringA("XStatusRequestedUri");
  bool found = false;
  for (const auto& choice : kMrimXStatusChoices)
   if (!mir_strcmpi(currentUri, choice.uri)) { label += choice.caption; found = true; break; }
  if (!found) label += L"Нет";
  if (label != m_xStatusMenuLabel) {
   m_xStatusMenuLabel = label;
   Menu_ModifyItem(m_hXStatusMenu, m_xStatusMenuLabel.c_str());
  }
 }
 return 0;
}
// The menu index is deliberately independent of the server URI.
// Only load numeric resources explicitly present in the Agent archive.
// Unverified status IDs use a neutral icon rather than a guessed picture.
static int MrimXStatusResource(const char *uri)
{
 if (!mir_strcmpi(uri, "status_chat")) return 33001;
 if (!mir_strcmpi(uri, "status_dnd")) return 33002;
 if (!mir_strcmpi(uri, "status_dating")) return 33003;
 if (_strnicmp(uri, "status_", 7)) return 33004;
 char *end = nullptr;
 const long value = strtol(uri + 7, &end, 10);
 if (!end || *end) return 33004;
 // These are actual files named status_N.ico in Agent(1).zip.
 // Other numeric files N.ico have unrelated numbering and are NOT used.
 switch (value) {
 case 29: return 34029;
 case 30: return 34030;
 case 32: return 34032;
 case 33: return 34033;
 case 34: return 34034;
 case 35: return 34035;
 case 36: return 34036;
 case 37: return 34037;
 case 38: return 34038;
 case 39: return 34039; // Original Agent ass.gif (ЖОПА)
 case 40: return 34040;
 case 41: return 34041;
 case 42: return 34042;
 case 43: return 34043;
 case 44: return 34044;
 case 45: return 34045;
 case 5: return 35005; // Jimm beta
 case 18: return 35018; // Jimm beta
 case 19: return 35019; // Jimm beta
 case 7: return 35007; // Jimm beta
 case 10: return 35010; // Jimm beta
 case 47: return 35047; // Jimm beta
 case 22: return 35022; // Jimm beta
 case 26: return 35026; // Jimm beta
 case 24: return 35024; // Jimm beta
 case 27: return 35027; // Jimm beta
 case 23: return 35023; // Jimm beta
 case 4: return 35004; // Jimm beta
 case 9: return 35009; // Jimm beta
 case 6: return 35006; // Jimm beta
 case 21: return 35021; // Jimm beta
 case 20: return 35020; // Jimm beta
 case 17: return 35017; // Jimm beta
 case 8: return 35008; // Jimm beta
 case 15: return 35015; // Jimm beta
 case 16: return 35016; // Jimm beta
 case 28: return 35028; // Jimm beta
 case 51: return 35051; // Jimm beta
 case 52: return 35052; // Jimm beta
 case 46: return 35046; // Jimm beta
 case 12: return 35012; // Jimm beta
 case 13: return 35013; // Jimm beta
 case 11: return 35011; // Jimm beta
 case 14: return 35014; // Jimm beta
 case 48: return 35048; // Jimm beta
 case 49: return 34049;
 case 50: return 34050;
 default: return 33004;
 }
}

// Menu bitmaps are local to the popup and are destroyed when it closes.
// Windows menus treat a 32-bit item bitmap as premultiplied ARGB.
// Drawing old Agent ICO resources directly into a zeroed DIB can leave the
// alpha channel zero, making a perfectly valid icon almost invisible.
// Compose on the current menu background and make all pixels opaque instead.
static HBITMAP MrimXStatusBitmap(HICON icon)
{
 if (!icon) return nullptr;
 BITMAPINFO info = {};
 info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
 info.bmiHeader.biWidth = 16;
 info.bmiHeader.biHeight = -16;
 info.bmiHeader.biPlanes = 1;
 info.bmiHeader.biBitCount = 32;
 info.bmiHeader.biCompression = BI_RGB;
 void *pixels = nullptr;
 HDC screen = GetDC(nullptr);
 if (!screen) return nullptr;
 HBITMAP bmp = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
 if (bmp && pixels) {
  HDC canvas = CreateCompatibleDC(screen);
  if (canvas) {
   HGDIOBJ previous = SelectObject(canvas, bmp);
   RECT bounds = {0, 0, 16, 16};
   FillRect(canvas, &bounds, GetSysColorBrush(COLOR_MENU));
   DrawIconEx(canvas, 0, 0, icon, 16, 16, 0, nullptr, DI_NORMAL);
   SelectObject(canvas, previous);
   DeleteDC(canvas);
   // The image is now opaque (including menu-colored background); ensure
   // Windows does not discard it because of stale alpha in legacy ICOs.
   auto *argb = static_cast<unsigned char *>(pixels);
   for (int i = 0; i < 16 * 16; ++i) argb[i * 4 + 3] = 255;
  }
  else {
   DeleteObject(bmp);
   bmp = nullptr;
  }
 }
 ReleaseDC(nullptr, screen);
 return bmp;
}

INT_PTR __cdecl CMrimProto::OnChooseXStatus(WPARAM, LPARAM)
{
 HMENU menu = CreatePopupMenu();
 if (!menu) return 1;
 std::vector<HBITMAP> bitmaps;
 const CMStringA savedUri = getMStringA("XStatusRequestedUri");
 AppendMenuW(menu, MF_STRING | (savedUri.IsEmpty() ? MF_CHECKED : 0), 1, L"Без X-статуса");
 AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
 for (size_t i = 0; i < _countof(kMrimXStatusChoices); ++i) {
  const auto &choice = kMrimXStatusChoices[i];
  if (i == 16) AppendMenuW(menu, MF_SEPARATOR, 0, nullptr); // Jimm beta catalog
  const UINT command = (UINT)(1000 + i);
  AppendMenuW(menu, MF_STRING | (!mir_strcmpi(savedUri, choice.uri) ? MF_CHECKED : 0), command, choice.caption);
  HICON icon = (HICON)LoadImageW(g_plugin.getInst(),
    MAKEINTRESOURCEW(MrimXStatusResource(choice.uri)), IMAGE_ICON,
    16, 16, LR_DEFAULTCOLOR);
  if (icon) {
   HBITMAP bmp = MrimXStatusBitmap(icon);
   DestroyIcon(icon);
   if (bmp) {
    MENUITEMINFOW item = {};
    item.cbSize = sizeof(item);
    item.fMask = MIIM_BITMAP;
    item.hbmpItem = bmp;
    if (SetMenuItemInfoW(menu, command, FALSE, &item))
     bitmaps.push_back(bmp);
    else DeleteObject(bmp);
   }
  }
 }
 POINT point = {}; GetCursorPos(&point);
 HWND owner = GetForegroundWindow();
 int selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                               point.x, point.y, 0, owner, nullptr);
 DestroyMenu(menu);
 for (auto bmp : bitmaps) DeleteObject(bmp);

 if (!selected) return 0;
 if (!m_online) {
  MessageBoxW(owner, L"Для изменения X-статуса подключитесь к MRIM.", L"PMLMRIM", MB_ICONINFORMATION);
  return 0;
 }
 if (selected == 1) {
  if (!SetExtendedStatus("", "", ""))
   MessageBoxW(owner, L"Не удалось отправить X-статус.", L"PMLMRIM", MB_ICONWARNING);
 } else if (selected >= 1000 && selected < 1000 + (int)_countof(kMrimXStatusChoices)) {
  const auto &choice = kMrimXStatusChoices[selected-1000];
  std::wstring text;
  if (!mir_strcmpi(savedUri, choice.uri)) {
   auto oldText = getMStringA("XStatusRequestedMessage");
   auto wide = ptrW(mir_utf8decodeW(oldText.c_str()));
   if (wide) text = wide.get();
  }
  if (!MrimAskStatusMessage(owner, choice.caption, text)) return 0;
  auto title = ptrA(mir_utf8encodeW(choice.caption));
  auto description = ptrA(mir_utf8encodeW(text.c_str()));
  if (!SetExtendedStatus(choice.uri, title ? title.get() : choice.uri,
                         description ? description.get() : ""))
   MessageBoxW(owner, L"Не удалось отправить X-статус.", L"PMLMRIM", MB_ICONWARNING);
 }
 return 0;
}

void CMrimProto::OnShutdown(){Shutdown();}
void CMrimProto::Shutdown(){
 m_stop=true;
 StopFileTransfers();
 // An offline transition does not necessarily trigger USER_STATUS per buddy.
 // Clear this protocol's extra icons explicitly on disconnect.
 for (auto &c : AccContacts()) {
  setByte(c, "XStatusActive", 0);
  ApplyXStatusIcon(c);
 }
 mir_cslock l(m_csSend);
 if(m_hConn){Netlib_CloseHandle(m_hConn);m_hConn=nullptr;}
}
uint32_t CMrimProto::ToMrimStatus(int s){if(s==ID_STATUS_INVISIBLE)return pmlmrim::INVISIBLE;if(s==ID_STATUS_AWAY||s==ID_STATUS_NA||s==ID_STATUS_DND)return pmlmrim::AWAY;return pmlmrim::ONLINE;}
int CMrimProto::ToMirandaStatus(uint32_t s){if(s==pmlmrim::ONLINE || s==4u)return ID_STATUS_ONLINE;if(s==pmlmrim::AWAY)return ID_STATUS_AWAY;if(s==pmlmrim::INVISIBLE)return ID_STATUS_INVISIBLE;return ID_STATUS_OFFLINE;}
INT_PTR CMrimProto::GetCaps(int t,MCONTACT){switch(t){case PFLAGNUM_1:return PF1_IM|PF1_FILE|PF1_AUTHREQ|PF1_BASICSEARCH|PF1_SEARCHBYEMAIL|PF1_ADDSEARCHRES|PF1_USERIDISEMAIL;case PFLAGNUM_2:return PF2_ONLINE|PF2_SHORTAWAY|PF2_LONGAWAY|PF2_INVISIBLE;case PFLAGNUM_3:return PF2_SHORTAWAY|PF2_LONGAWAY;case PFLAGNUM_4:return PF4_AVATARS;case PFLAG_UNIQUEIDTEXT:return (INT_PTR)TranslateT("Email");case PFLAG_MAXLENOFMESSAGE:return 4096;}return 0;}
MCONTACT CMrimProto::FindContact(const char* email){for(auto& c:AccContacts())if(!mir_strcmpi(getMStringA(c,"Email"),email) || !mir_strcmpi(getMStringA(c,"e-mail"),email))return c;return 0;}
MCONTACT CMrimProto::EnsureContact(const char* email){MCONTACT c=FindContact(email);if(c){
  // A previously created temporary/hidden contact becomes a normal roster contact.
  Contact::PutOnList(c);Contact::Hide(c,false);
  db_unset(c,"CList","NotOnList");db_unset(c,"CList","Hidden");
  return c;
 }
c=db_add_contact(m_szModuleName, 0);
 if(c){setString(c,"Email",email);setWord(c,"Status",ID_STATUS_OFFLINE);}return c;}
MCONTACT CMrimProto::AddToList(int flags,PROTOSEARCHRESULT* p){if(!p)return 0;CMStringA email;if(p->flags&PSR_UNICODE)email=mir_u2a(p->id.w);else email=p->id.a;if(email.IsEmpty())return 0;auto c=EnsureContact(email);if(c&&!(flags&PALF_TEMPORARY)){Contact::PutOnList(c);Contact::Hide(c,false);if(m_online)SendPacket(pmlmrim::makeAddContact(++m_seq,email.c_str()));}return c;}
bool CMrimProto::SendPacket(const pmlmrim::Bytes& data){mir_cslock l(m_csSend);if(!m_hConn)return false;size_t pos=0;while(pos<data.size()){int n=Netlib_Send(m_hConn,(const char*)data.data()+pos,(int)(data.size()-pos),0);if(n<=0)return false;pos+=n;}return true;}
int CMrimProto::SetStatus(int s){int old=m_iStatus;m_iDesiredStatus=s;if(s==ID_STATUS_OFFLINE){Shutdown();m_iStatus=ID_STATUS_OFFLINE;ProtoBroadcastAck(0,ACKTYPE_STATUS,ACKRESULT_SUCCESS,(HANDLE)old,m_iStatus);return 0;}
 if(m_online){m_iStatus=s;const CMStringA requestedUri = getMStringA("XStatusRequestedUri");
  if (!requestedUri.IsEmpty() && MrimIsSupportedXStatus(requestedUri.c_str())) {
   const auto title=getMStringA("XStatusRequestedTitle");
   const auto message=getMStringA("XStatusRequestedMessage");
   SetExtendedStatus(requestedUri.c_str(),title.c_str(),message.c_str());
  } else SendPacket(pmlmrim::makeStatus(++m_seq,ToMrimStatus(s)));
  ProtoBroadcastAck(0,ACKTYPE_STATUS,ACKRESULT_SUCCESS,(HANDLE)old,m_iStatus);return 0;}
 if(IsStatusConnecting(m_iStatus))return 0;
 if(getMStringA("Email").IsEmpty()||getMStringA("Password").IsEmpty()){ProtoBroadcastAck(0,ACKTYPE_LOGIN,ACKRESULT_FAILED,nullptr,LOGINERR_BADUSERID);return 0;}
 m_stop=false;m_iStatus=ID_STATUS_CONNECTING;ProtoBroadcastAck(0,ACKTYPE_STATUS,ACKRESULT_SUCCESS,(HANDLE)old,m_iStatus);ForkThread(&CMrimProto::ServerThread);return 0;}
// Extended status requires the 1.15 wire format. This method is intentionally
// not invoked from SetStatus: legacy servers still speak MRIM 1.13.
bool CMrimProto::SetExtendedStatus(const std::string& uri, const std::string& title,
                                    const std::string& description) {
 if (!m_online || m_stop) return false;
 if (!uri.empty() && !MrimIsSupportedXStatus(uri)) return false;
 try {
  pmlmrim_xstatus::State st;
  st.status=uri.empty() ? ToMrimStatus(m_iStatus) : 4u;
  st.uri=uri;
  // Agent and Renaissance peers announce X-status identifiers in lower case.
  std::transform(st.uri.begin(),st.uri.end(),st.uri.begin(),[](unsigned char c){return (char)std::tolower(c);});
  st.title=pmlmrim::utf8ToMrimText(title);
  st.description=pmlmrim::utf8ToMrimText(description);
  const auto raw=pmlmrim_xstatus::makeChangePacket(++m_seq,st);
  debugLogA("MRIM XStatus: sending status=0x%X uri=%s titleBytes=%u features=0x%X packetBytes=%u", st.status, st.uri.c_str(), (unsigned)st.title.size(), st.features, (unsigned)raw.size());
  if (!SendPacket(raw)) return false;
  debugLogA("MRIM XStatus: CHANGE_STATUS sent; server acceptance is not acknowledged by this command");
  // Locally requested values; server acceptance must be verified from presence.
  setString("XStatusRequestedUri",uri.c_str());
  setString("XStatusRequestedTitle",title.c_str());
  setString("XStatusRequestedMessage",description.c_str());
  // Update the status-menu label immediately; some Miranda builds only
  // issue ME_CLIST_PREBUILDSTATUSMENU when the menu is opened again.
  OnPreBuildStatusMenu(0, 0);
  return true;
 } catch (...) { return false; }
}

// The auth dialog passes its database event, not a contact handle.
int CMrimProto::Authorize(MEVENT hEvent) {
 DB::EventInfo event(hEvent);
 if (!event || event.eventType != EVENTTYPE_AUTHREQUEST || !event.pBlob) return 1;
 DB::AUTH_BLOB blob(event.pBlob);
 MCONTACT c=blob.get_contact();
 if (!c || !m_online) return 1;
 CMStringA email=getMStringA(c,"Email");
 if (email.IsEmpty()) return 1;
 try {
  if (!SendPacket(pmlmrim::makeAuthorizationGrant(++m_seq,email.c_str()))) return 1;
  setByte(c,"Grant",0);
  Contact::PutOnList(c);
  Contact::Hide(c,false);
  db_unset(c,"CList","NotOnList");
  db_unset(c,"CList","Hidden");
  return 0;
 } catch (...) { return 1; }
}

// MRIM has an affirmative AUTHORIZE command but no matching denial command.
// Declining dismisses the local pending approval without authorizing the peer.
int CMrimProto::AuthDeny(MEVENT hEvent,const wchar_t*) {
 DB::EventInfo event(hEvent);
 if (!event || event.eventType != EVENTTYPE_AUTHREQUEST || !event.pBlob) return 1;
 DB::AUTH_BLOB blob(event.pBlob);
 MCONTACT c=blob.get_contact();
 if (!c) return 1;
 setByte(c,"Grant",0);
 return 0;
}

int CMrimProto::AuthRequest(MCONTACT c,const wchar_t* reason) {
 if (!c || !m_online) return 1;
 CMStringA email=getMStringA(c,"Email");
 if (email.IsEmpty()) return 1;
 auto utf8=ptrA(mir_utf8encodeW(reason ? reason : L""));
 try {
  if (!SendPacket(pmlmrim::makeAuthorizationRequest(++m_seq,email.c_str(),utf8 ? utf8.get() : ""))) return 1;
  setByte(c,"Auth",1);
  return 0;
 } catch (...) { return 1; }
}

int CMrimProto::SendMsg(MCONTACT c,MEVENT,const char* msg){int seq=(int)++m_seq;CMStringA to=getMStringA(c,"Email");if(!m_online||to.IsEmpty()){ProtoBroadcastAsync(c,ACKTYPE_MESSAGE,ACKRESULT_FAILED,(HANDLE)(INT_PTR)seq,(LPARAM)TranslateT("Not connected"));return seq;}
 try{
  auto packet=pmlmrim::makeTextMessage(seq,to.c_str(),msg?msg:"",getByte("MessageEncoding",1)!=0);
  {mir_cslock l(m_csPending);m_pendingMessages[(uint32_t)seq]=c;}
  if(!SendPacket(packet)){
   {mir_cslock l(m_csPending);m_pendingMessages.erase((uint32_t)seq);}
   ProtoBroadcastAsync(c,ACKTYPE_MESSAGE,ACKRESULT_FAILED,(HANDLE)(INT_PTR)seq,(LPARAM)TranslateT("Network send failed"));
  }else debugLogA("MRIM message %d sent; waiting for server MESSAGE_STATUS",seq);
 }catch(...){
  {mir_cslock l(m_csPending);m_pendingMessages.erase((uint32_t)seq);}
  ProtoBroadcastAsync(c,ACKTYPE_MESSAGE,ACKRESULT_FAILED,(HANDLE)(INT_PTR)seq,(LPARAM)TranslateT("Message encoding failed"));
 }return seq;}

// Exact e-mail search. The result is reported ONLY after MRIM_CS_ANKETA_INFO
// confirms existence; do not manufacture a result locally.
// The Miranda Find/Add dialog calls PS_SEARCHBYEMAIL for the "E-mail address"
// radio button, not PS_BASICSEARCH. Both fields use the same MRIM exact
// address lookup, so keep one implementation for validation, pending requests
// and response ACKs.
HANDLE CMrimProto::SearchByEmail(const wchar_t* email)
{
 return SearchBasic(email);
}

HANDLE CMrimProto::SearchBasic(const wchar_t* id)
{
 if (!m_online || m_stop || !id || !*id) return nullptr;
 std::wstring email(id);
 // MRIM WP_REQUEST uses Username + Domain, not arbitrary text search.
 if (email.find_first_of(L" \t\r\n") != std::wstring::npos) return nullptr;
 const size_t at = email.find(L'@');
 if (at == std::wstring::npos || at == 0 || at + 1 >= email.size()) return nullptr;
 auto utf8 = ptrA(mir_utf8encodeW(email.c_str()));
 if (!utf8 || !*utf8) return nullptr;
 const uint32_t seq = ++m_seq;
 HANDLE handle = (HANDLE)(INT_PTR)seq;
 try {
  auto packet = pmlmrim::makeProfileRequest(seq, utf8.get());
  { mir_cslock lock(m_csPending);
    m_pendingSearches.emplace(seq, PendingSearch{handle,email,GetTickCount()}); }
  if (!SendPacket(packet)) {
   { mir_cslock lock(m_csPending); m_pendingSearches.erase(seq); }
   debugLogA("MRIM SEARCH: failed to send WP_REQUEST seq=%u",seq);
   return nullptr;
  }
  debugLogA("MRIM SEARCH: WP_REQUEST sent seq=%u",seq);
  return handle;
 }
 catch (const std::exception& e) {
  { mir_cslock lock(m_csPending); m_pendingSearches.erase(seq); }
  debugLogA("MRIM SEARCH: request error: %s",e.what());
  return nullptr;
 }
}

// Display the identity already known from the roster. The MRIM server's
// extended profile lookup is not implemented here; never invent personal data.
int CMrimProto::GetInfo(MCONTACT c,int){
 if(!c)return 1;
 CMStringA email=getMStringA(c,"Email");
 if(email.IsEmpty())return 1;
 setString(c,"e-mail",email.c_str());
 if(m_online){
  const uint32_t seq=++m_seq;
  {mir_cslock lock(m_csPending);m_pendingProfiles[seq]=c;}
  try { if(!SendPacket(pmlmrim::makeProfileRequest(seq,email.c_str()))){mir_cslock lock(m_csPending);m_pendingProfiles.erase(seq);} }
  catch(...) {mir_cslock lock(m_csPending);m_pendingProfiles.erase(seq);}
 }
 if(getMStringW(c,"Nick").IsEmpty()){
  auto at=strchr(email.c_str(),'@');
  std::string nick(email.c_str(),at?size_t(at-email.c_str()):strlen(email.c_str()));
  setWString(c,"Nick",ptrW(mir_utf8decodeW(nick.c_str())));
 }
 ProtoBroadcastAsync(c,ACKTYPE_GETINFO,ACKRESULT_SUCCESS,(HANDLE)1,0);
 return 0;
}


// Separate Miranda user-info page. The stock notes-only window has no MRIM
// fields unless a protocol registers its own CUserInfoPageDlg.
class CMrimInfoPage : public CUserInfoPageDlg {
 CMrimProto* m_proto;
public:
 CMrimInfoPage(CMrimProto* proto):CUserInfoPageDlg(g_plugin,IDD_MRIM_INFO),m_proto(proto) {}
 bool IsEmpty() const override { return false; }
 bool OnRefresh() override {
  if(!m_hContact || m_hContact==INVALID_CONTACT_ID)return true;
  const char* keys[]={"Email","Nick","FirstName","LastName","Gender","Age","BirthDate","City","Country","Phone","Location"};
  const wchar_t* titles[]={L"Email",L"Nickname",L"First name",L"Last name",L"Gender",L"Age",L"Birthday",L"City",L"Country",L"Phone",L"Location"};
  std::wstring output;
  for(size_t k=0;k<sizeof(keys)/sizeof(keys[0]);++k){
   DBVARIANT v={};
   if(db_get_ws(m_hContact,Proto_GetBaseAccountName(m_hContact),keys[k],&v)==0){
    if(v.pwszVal && *v.pwszVal){
     output+=titles[k];output+=L": ";
     // Translate only the MRIM gender codes for the profile display.
     if(k==4 && wcscmp(v.pwszVal,L"1.0")==0)output+=L"Мужской";
     else if(k==4 && wcscmp(v.pwszVal,L"2.0")==0)output+=L"Женский";
     else output+=v.pwszVal;
     output+=L"\r\n";
    }
    db_free(&v);
   } else if(k==0){
    if(db_get_s(m_hContact,Proto_GetBaseAccountName(m_hContact),"Email",&v)==0){
     if(v.pszVal){output+=L"Email: ";output+=ptrW(mir_utf8decodeW(v.pszVal));output+=L"\r\n";}db_free(&v);
    }
   }
  }
  if(output.empty())output=L"Profile not loaded. Press Refresh while online.";
  SetDlgItemTextW(GetHwnd(),IDC_PROFILE_TEXT,output.c_str());
  // IMPORTANT: OnRefresh is invoked by the modal user-info UI.
  // Do not perform network I/O or initiate repeated profile requests here.
  return true;
 }
};
int CMrimProto::OnUserInfoInit(WPARAM w,LPARAM l){
 MCONTACT c=(MCONTACT)l;
 if(c && mir_strcmp(Proto_GetBaseAccountName(c),m_szModuleName)==0){
  USERINFOPAGE page={};page.szTitle.w=LPGENW("MRIM profile");
  page.pDialog=new CMrimInfoPage(this);
  g_plugin.addUserInfo(w,&page);
  // Queue the profile fetch off the UI thread. The page reads cached DB fields.
  if(m_online) ForkThread(&CMrimProto::ProfileRequestThread, new MCONTACT(c));
 }
 return 0;
}

void __cdecl CMrimProto::ProfileRequestThread(void* arg){
 std::unique_ptr<MCONTACT> contact(static_cast<MCONTACT*>(arg));
 if(m_stop || !m_online || !*contact)return;
 GetInfo(*contact,0);
}

