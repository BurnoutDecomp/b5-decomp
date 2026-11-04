#pragma once

#include "SDKs/Realmc/RealmcXenonRunnableTask.h"
#include "SDKs/Realmc/RealmcGameInfo.h"

namespace RealmcIface
{

class SetActiveCardTask : public XenonRunnableTask
{
public:
    SetActiveCardTask(RealmcCore::MessageQueue* pMessageQueue,
                      RealmcCore::MemcardState* pMemcardState,
                      XenonUtil::State* pState,
                      const GameInfo* pGameInfo,
                      s32 iField18, s32 iField1C, s32 iField20, char cField24);
    ~SetActiveCardTask();

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

    // ARTIST SetActiveCardTask::Execute @0x82B55018 (vtable slot +0x0C).
    void Execute() override;

    // ARTIST SetActiveCardTask::GetTaskId @0x82B55010 (vtable slot +0x10).
    int GetTaskId() override { return 512; }

private:
    const GameInfo* mpGameInfo; // +0x14
    s32 miField18;              // +0x18 (native role not independently named)
    s32 miField1C;              // +0x1C (native role not independently named)
    s32 miField20;              // +0x20 (content flag selected by Execute)
    char mcField24;             // +0x24 (native byte tested by Execute)
};

} // namespace RealmcIface

CGS_STATIC_ASSERT((sizeof(RealmcIface::SetActiveCardTask) == 0x28), "ARTIST SetActiveCardTask size");
