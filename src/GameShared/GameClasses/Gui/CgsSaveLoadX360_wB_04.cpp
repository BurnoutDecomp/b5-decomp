#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"
#include "SDKs/Realmc/RealmcSaveInfo.h"
#include "SDKs/Realmc/RealmcTitleInfo.h"
#include "SDKs/Realmc/RealmcSaveReq.h"
#include "SDKs/Realmc/RealmcIfaceSaveCheckParams.h"
namespace CgsGui
{
// 8284C818: native aggregate return, hidden destination r3 and this r4.
RealmcIface::SaveInfo SaveLoadSystem::CreateRealmcSaveInfo()
{
    RealmcIface::DataBuffer data;
    data.mpData = mpContentInfoFileBuffer;
    data.muSize = static_cast<u32>(miContentInfoFileSize);
    RealmcIface::EntryContentName name(macwMetadataTitle, macTitle,
        miExtraFilesSizeBytes * miNumberOfMugshotsPerType * miNumberOfMugshotTypes
        + mStoredData.miSize);
    return RealmcIface::SaveInfo(name, data);
}
// 8284C898
RealmcIface::SaveCheckParams SaveLoadSystem::CreateRealmcSaveCheckParams()
{
    RealmcIface::TitleInfo& title = RealmcIface::TitleInfo::Empty();
    RealmcIface::SaveInfo info = CreateRealmcSaveInfo();
    RealmcIface::SaveReq request(macTitle, &info, 0x100u, mStoredData.miSize, &title.muTitle);
    RealmcIface::SaveReq* pRequest = &request;
    return RealmcIface::SaveCheckParams(1, &pRequest);
}
// 82852368
void SaveLoadSystem::EnableAutosave(s32)
{
    RealmcIface::SaveCheckParams params = CreateRealmcSaveCheckParams();
    mpMemcardInterface->CheckSave(1, &params);
}
}
