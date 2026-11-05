#include "stdafx.h"

// Encoding preference applies to OUTGOING chat messages only.
// Incoming MRIM messages already advertise UTF-16LE using a protocol flag;
// the existing receiver preserves automatic decoding for compatibility.
// MessageEncoding=1: UTF-16LE (default); 0: legacy Windows-1251.
class CMrimAccountDlg : public CProtoDlgBase<CMrimProto> {
 CCtrlEdit email, password, server, port, avatarServer, avatarPort;
 CCtrlCombo messageEncoding;
 CMrimProto* m_account;

protected:
 bool OnInitDialog() override {
  if (!CProtoDlgBase<CMrimProto>::OnInitDialog()) return false;
  messageEncoding.AddString(L"UTF-16LE (recommended)", 1);
  messageEncoding.AddString(L"Windows-1251 (legacy)", 0);
  // Ensure that a new account defaults to Unicode, but keep saved selection.
  messageEncoding.SelectData(m_account->getByte("MessageEncoding", 1) == 0 ? 0 : 1);
  return true;
 }

 bool OnApply() override {
  if (!CProtoDlgBase<CMrimProto>::OnApply()) return false;
  const LPARAM mode = messageEncoding.GetCurData();
  if (mode != 0 && mode != 1) return false;
  m_account->setByte("MessageEncoding", static_cast<BYTE>(mode));
  return true;
 }

public:
 CMrimAccountDlg(CMrimProto* p, int id) : CProtoDlgBase<CMrimProto>(p, id),
  email(this, IDC_EMAIL), password(this, IDC_PASSWORD),
  server(this, IDC_SERVER), port(this, IDC_PORT),
  avatarServer(this, IDC_AVATAR_SERVER), avatarPort(this, IDC_AVATAR_PORT),
  messageEncoding(this, IDC_MESSAGE_ENCODING), m_account(p) {
  CreateLink(email, "Email", L"");
  CreateLink(password, "Password", L"");
  CreateLink(server, p->m_server);
  CreateLink(port, p->m_port);
  CreateLink(avatarServer, p->m_avatarServer);
  CreateLink(avatarPort, p->m_avatarPort);
 }
};

MWindow CMrimProto::OnCreateAccMgrUI(MWindow parent) {
 auto* dlg = new CMrimAccountDlg(this, IDD_ACCMGRUI);
 dlg->SetParent(parent);
 dlg->Show();
 return dlg->GetHwnd();
}

int CMrimProto::OnOptionsInit(WPARAM w, LPARAM) {
 OPTIONSDIALOGPAGE o = {};
 o.flags = ODPF_UNICODE;
 o.szGroup.w = LPGENW("Network");
 o.szTitle.w = m_tszUserName;
 o.position = 1;
 o.pDialog = new CMrimAccountDlg(this, IDD_OPTIONS);
 g_plugin.addOptions(w, &o);
 return 0;
}
