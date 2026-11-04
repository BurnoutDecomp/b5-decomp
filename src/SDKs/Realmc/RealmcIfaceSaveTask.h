#pragma once

#include "SDKs/Realmc/RealmcXenonRunnableTask.h"
#include "SDKs/Realmc/RealmcGameInfo.h"
#include "SDKs/Realmc/RealmcSaveInfo.h"
#include "SDKs/Realmc/RealmcTitleInfo.h"
#include "SDKs/Realmc/RealmcLoadEntryInfo.h"

namespace RealmcIface
{

class SaveTask : public XenonRunnableTask
{
public:
    SaveTask(RealmcCore::MessageQueue* pMessageQueue,
             RealmcCore::MemcardState* pMemcardState,
             XenonUtil::State* pState,
             const GameInfo* pGameInfo,
             const SaveInfo& rSaveInfo,
             s32 iNumEntries,
             const LoadEntryInfo* pEntries,
             s32 iFlags,
             const TitleInfo& rTitleInfo);
    ~SaveTask() {}

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

    // ARTIST SaveTask::Execute @0x82B56620 (vtable slot +0x0C).
    void Execute() override;

    // The GetTaskId slot is a folded shared return-8 thunk in ARTIST.
    int GetTaskId() override { return 8; }

private:
    void DisplayTcrMessages(const s32* pCardStatus);

    const GameInfo* mpGameInfo;   // +0x14
    s32 miNumEntries;             // +0x18
    LoadEntryInfo maEntries[4];   // +0x1C .. +0xDC
    SaveInfo mSaveInfo;           // +0xDC .. +0x214
    TitleInfo mTitleInfo;         // +0x214
    s32 miFlags;                  // +0x218
};

} // namespace RealmcIface

CGS_STATIC_ASSERT((sizeof(RealmcIface::SaveTask) == 0x21C), "ARTIST SaveTask size");
