#include "stdafx.h"
CMPlugin g_plugin;
PLUGININFOEX pluginInfoEx = {
 sizeof(PLUGININFOEX),__PLUGIN_NAME,
 PLUGIN_MAKE_VERSION(__MAJOR_VERSION,__MINOR_VERSION,__RELEASE_NUM,__BUILD_NUM),
 __DESCRIPTION,__AUTHOR,__COPYRIGHT,__AUTHORWEB,UNICODE_AWARE,
 {0x62f1244d,0x2e2b,0x4783,{0x90,0x3e,0x47,0x16,0x6a,0x1f,0x22,0x11}}
};
CMPlugin::CMPlugin(): ACCPROTOPLUGIN<CMrimProto>("PMLMRIM",pluginInfoEx) { SetUniqueId("Email"); }
extern "C" __declspec(dllexport) const MUUID MirandaInterfaces[]={MIID_PROTOCOL,MIID_LAST};
static IconItem iconList[]={{LPGEN("Protocol icon"),"main",IDI_PMLMRIM}};
int CMPlugin::Load(){registerIcon("Protocols/PMLMRIM",iconList,"PMLMRIM");return 0;}
