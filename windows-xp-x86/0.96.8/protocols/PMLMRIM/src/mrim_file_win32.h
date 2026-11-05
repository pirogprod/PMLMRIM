#pragma once
#include "mrim_file_session.h"
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <vector>
namespace pmlmrim {
// Blocking Windows transport for an already established MRIM file socket.
// Run on a worker thread, never on Miranda's UI or packet reader thread.
// Caller owns the socket; these functions never close it.
void sendFilesOnSocket(SOCKET socket, const std::string& sender,
 const std::string& receiver, const std::vector<TransferItem>& files,
 const std::vector<std::wstring>& paths,
 const ItemProgress& progress = {}, const IsCancelled& cancelled = {});
void receiveFilesOnSocket(SOCKET socket, const std::string& sender,
 const std::string& receiver, const std::vector<TransferItem>& files,
 const std::wstring& directory,
 const ItemProgress& progress = {}, const IsCancelled& cancelled = {});
}
