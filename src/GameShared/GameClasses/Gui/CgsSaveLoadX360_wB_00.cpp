// CgsGui::SaveLoadSystem -- wave-B fan-out part 00 (group G1: assert-only stubs).
//
// Realmc result callbacks that are never expected to fire on X360. Each is a
// streamed-assert stub: the X360 builds an assert message through the
// gpcMessageBuffer / StrStream machinery (BeginAssert / BasePriorityQueue::Clear /
// FireAssert / EndAssert), which folds to a plain CGS_ASSERT(false, "literal")
// per the established project rule (see CgsScriptedFsm.cpp). Assert messages are
// verbatim from the X360 rodata; the file/line arg is dropped (committed style).
//
// Include order is load-bearing: CgsSaveLoadX360.h FIRST (it fwd-declares
// ::SystemUserProfile / ::LanguageManager at global scope).

#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"

#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT

namespace CgsGui
{
    // X360 0x8284C7D8. Realmc save-ready callback -- never expected on X360.
    s32 SaveLoadSystem::SaveReady()
    {
        CGS_ASSERT(false, "SaveReady called.\n");
        return 0;
    }

    // X360 0x8284CB80. Realmc find-entry callback -- never expected on X360.
    void SaveLoadSystem::FoundEntry()
    {
        CGS_ASSERT(false, "FoundEntry called");
    }

    // X360 0x8284CBC0. Realmc find-entries-done callback -- never expected on X360.
    void SaveLoadSystem::FindEntriesDone()
    {
        CGS_ASSERT(false, "FindEntriesDone called");
    }

    // X360 0x8284C648. Realmc delete result callback -- streamed assert stub.
    void SaveLoadSystem::DeleteDone()
    {
        CGS_ASSERT(false, "DeleteDone: not implemented.");
    }
}
