#pragma once

#include "SDKs/Realmc/RealmcXenonRunnableTask.h"
#include "SDKs/Realmc/RealmcGameInfo.h"
#include "SDKs/Realmc/RealmcIfaceSaveCheckParams.h"

namespace RealmcIface
{

class SetAutosaveTask : public XenonRunnableTask
{
public:
    SetAutosaveTask(RealmcCore::MessageQueue* pMessageQueue,
                    RealmcCore::MemcardState* pMemcardState,
                    XenonUtil::State* pState,
                    const GameInfo* pGameInfo,
                    s32 iEnabled,
                    const SaveCheckParams& rParams);
    ~SetAutosaveTask();

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

    // ARTIST SetAutosaveTask::Execute @0x82B56CB0 (vtable slot +0x0C).
    void Execute() override;

    // ARTIST SetAutosaveTask::GetTaskId @0x82B56C60 (vtable slot +0x10).
    int GetTaskId() override { return 128; }

private:
    const GameInfo* mpGameInfo; // +0x14
    s32 miEnabled;              // +0x18
    SaveCheckParams mParams;    // +0x1C
};

} // namespace RealmcIface

CGS_STATIC_ASSERT((sizeof(RealmcIface::SetAutosaveTask) == 0x24), "ARTIST SetAutosaveTask size");
