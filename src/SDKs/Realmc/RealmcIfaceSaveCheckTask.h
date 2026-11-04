#pragma once

#include "SDKs/Realmc/RealmcXenonRunnableTask.h"
#include "SDKs/Realmc/RealmcGameInfo.h"
#include "SDKs/Realmc/RealmcIfaceSaveCheckParams.h"

namespace RealmcIface
{

class SetAutosaveTask;

class SaveCheckTask : public XenonRunnableTask
{
public:
    SaveCheckTask(RealmcCore::MessageQueue* pMessageQueue,
                  RealmcCore::MemcardState* pMemcardState,
                  XenonUtil::State* pState,
                  const GameInfo* pGameInfo,
                  const SaveCheckParams& rParams);
    ~SaveCheckTask();

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

    // ARTIST SaveCheckTask::Execute @0x82B56028 (vtable slot +0x0C).
    void Execute() override;

    // ARTIST SaveCheckTask::GetTaskId @0x82B55FD8 (vtable slot +0x10).
    int GetTaskId() override { return 4; }

private:
    friend class SetAutosaveTask;

    const GameInfo* mpGameInfo; // +0x14
    SaveCheckParams mParams;    // +0x18
    s32 miField20;              // +0x20
    s32 miField24;              // +0x24
};

} // namespace RealmcIface

CGS_STATIC_ASSERT((sizeof(RealmcIface::SaveCheckTask) == 0x28), "ARTIST SaveCheckTask size");
