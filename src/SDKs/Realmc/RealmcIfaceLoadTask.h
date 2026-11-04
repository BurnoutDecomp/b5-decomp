#pragma once

#include "SDKs/Realmc/RealmcXenonRunnableTask.h"
#include "SDKs/Realmc/RealmcGameInfo.h"
#include "SDKs/Realmc/RealmcEntryContentName.h"
#include "SDKs/Realmc/RealmcTitleInfo.h"
#include "SDKs/Realmc/RealmcLoadEntryInfo.h"

namespace RealmcIface
{

class LoadTask : public XenonRunnableTask
{
public:
    LoadTask(RealmcCore::MessageQueue* pMessageQueue,
             RealmcCore::MemcardState* pMemcardState,
             XenonUtil::State* pState,
             const GameInfo* pGameInfo,
             const EntryContentName& rEntryContentName,
             s32 iNumEntries,
             const LoadEntryInfo* pEntries,
             s32 iFlags,
             const TitleInfo& rTitleInfo);
    ~LoadTask() {}

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

    // ARTIST LoadTask::Execute @0x82B57410 (vtable slot +0x0C).
    void Execute() override;

    // ARTIST LoadTask::GetTaskId @0x82B42CC8 (folded return-16 thunk).
    int GetTaskId() override { return 16; }

private:
    void DisplayTcrMessages(const s32* pCardStatus);

    s32 miNumEntries;             // +0x14
    LoadEntryInfo maEntries[4];   // +0x18 .. +0xD8
    EntryContentName mEntryContentName; // +0xD8 .. +0x208
    const GameInfo* mpGameInfo;   // +0x208
    TitleInfo mTitleInfo;         // +0x20C
    s32 miFlags;                  // +0x210
};

} // namespace RealmcIface

CGS_STATIC_ASSERT((sizeof(RealmcIface::LoadTask) == 0x214), "ARTIST LoadTask size");
