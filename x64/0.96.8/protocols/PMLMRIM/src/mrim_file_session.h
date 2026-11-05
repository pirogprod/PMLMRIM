#pragma once
#include "mrim_file_transport.h"
#include "mrim_file_stream.h"
#include <vector>
#include <string>
namespace pmlmrim {
// One established MRIM file data channel; the caller owns the socket and disk handles.
// Never exposes arbitrary remote paths: names must match validated offer entries.
using OpenRead = std::function<ReadBytes(const TransferItem&)>;
using OpenWrite = std::function<WriteBytes(const TransferItem&)>;
using ItemProgress = std::function<void(size_t, uint64_t)>;
void serveFileRequests(const ReadBytes& socketRead, const WriteBytes& socketWrite,
 const std::string& senderEmail, const std::string& recipientEmail,
 const std::vector<TransferItem>& files,
 const OpenRead& openSource, const ItemProgress& onProgress = {},
 const IsCancelled& cancelled = {});
void requestFiles(const ReadBytes& socketRead, const WriteBytes& socketWrite,
 const std::string& senderEmail, const std::string& recipientEmail,
 const std::vector<TransferItem>& files,
 const OpenWrite& openDestination, const ItemProgress& onProgress = {},
 const IsCancelled& cancelled = {});
}
