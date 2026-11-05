#pragma once

// Windows XP SP3 target. Keep this before any Windows SDK header.
#if defined(_M_IX86) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0501
#endif
#if defined(_M_IX86) && !defined(WINVER)
#define WINVER 0x0501
#endif


#include <windows.h>
#include <string>
#include <vector>
#include <atomic>
#include <newpluginapi.h>
#include <m_system.h>
#include <m_clist.h>
#include <m_extraicons.h>
#include <m_contacts.h>
#include <m_database.h>
#include <m_gui.h>
#include <m_langpack.h>
#include <m_netlib.h>
#include <m_options.h>
#include <m_protocols.h>
#include <m_protosvc.h>
#include <m_protoint.h>
#include <m_avatars.h>
#include <m_skin.h>
#include <m_utils.h>
#include "resource.h"
#include "version.h"
#include "mrim_wire.h"
#include "mrim_session.h"
#include "mrim_xstatus_wire.h"
#include "proto.h"
extern PLUGININFOEX pluginInfoEx;
