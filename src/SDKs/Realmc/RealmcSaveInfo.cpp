#include "SDKs/Realmc/RealmcSaveInfo.h"
namespace RealmcIface
{
SaveInfo::SaveInfo(const EntryContentName& rName, const DataBuffer& rData)
    : mContentName(rName), mContentInfo(rData) {}
SaveInfo::SaveInfo(const SaveInfo& rOther)
    : mContentName(rOther.mContentName), mContentInfo(rOther.mContentInfo) {}
}
