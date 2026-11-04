#pragma once

#include "SDKs/Realmc/RealmcXenonRunnableTask.h"
#include "SDKs/Realmc/RealmcGameInfo.h"
#include "SDKs/Realmc/RealmcSaveInfo.h"
#include "SDKs/Realmc/RealmcTitleInfo.h"
#include "SDKs/Realmc/RealmcLoadEntryInfo.h"

namespace RealmcIface
{

class BootupCheckTask : public XenonRunnableTask
{
public:
    BootupCheckTask(RealmcCore::MessageQueue* pMessageQueue,
                    RealmcCore::MemcardState* pMemcardState,
                    XenonUtil::State* pState,
                    const GameInfo* pGameInfo,
                    const SaveInfo& rSaveInfo,
                    s32 iNumEntries,
                    const LoadEntryInfo* pEntries,
                    const TitleInfo& rTitleInfo);
    ~BootupCheckTask();

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

    // ARTIST BootupCheckTask::Execute @0x82B55980 (vtable slot +0x0C).
    void Execute() override;

    // ARTIST BootupCheckTask::GetTaskId @0x82B55368 (vtable slot +0x10).
    int GetTaskId() override { return 2; }

private:
    void DisplayTcrMessages(const s32* pCardStatus);

    const GameInfo* mpGameInfo;   // +0x14
    SaveInfo mSaveInfo;           // +0x18
    TitleInfo mTitleInfo;         // +0x150
    s32 miNumEntries;             // +0x154
    LoadEntryInfo* mpEntries;     // +0x158 (native-owned copy)
};

} // namespace RealmcIface

CGS_STATIC_ASSERT((sizeof(RealmcIface::BootupCheckTask) == 0x15C), "ARTIST BootupCheckTask size");
