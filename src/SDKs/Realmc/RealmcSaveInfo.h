#pragma once
#include "SDKs/Realmc/RealmcEntryContentName.h"
#include "SDKs/Realmc/RealmcDataBuffer.h"
namespace RealmcIface
{
// 82B51CC0 constructs the 130-byte name then copies DataBuffer to 130/134.
// 82B51D10 is this type's copy constructor, used by SaveReq.
class SaveInfo
{
public:
    SaveInfo(const EntryContentName& rName, const DataBuffer& rData);
    SaveInfo(const SaveInfo& rOther);
    EntryContentName mContentName;
    DataBuffer mContentInfo;
};
CGS_STATIC_ASSERT(sizeof(SaveInfo) == 0x138, "ARTIST SaveInfo size");
}
