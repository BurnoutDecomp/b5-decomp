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

#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h" // CGS_ASSERT

static const char KAC_ARTIST_ASSERT_FILE[] = "..\\..\\..\\GameShared\\GameClasses\\Gui/CgsSaveLoadX360.cpp";

namespace CgsGui
{
    // X360 0x8284C7D8. Realmc save-ready callback -- never expected on X360.
    s32 SaveLoadSystem::SaveReady()
    {
        CGS_ASSERT(false,
            CgsDev::Assert::MessageAt("SaveReady called.\n", KAC_ARTIST_ASSERT_FILE, 917 /* ARTIST 0x8284C7EC */));
        return 0;
    }

    // X360 0x8284CB80. Realmc find-entry callback -- never expected on X360.
    void SaveLoadSystem::FoundEntry(const void*)
    {
        CGS_ASSERT(false,
            CgsDev::Assert::MessageAt("FoundEntry called", KAC_ARTIST_ASSERT_FILE, 1252 /* ARTIST 0x8284CB94 */));
    }

    // X360 0x8284CBC0. Realmc find-entries-done callback -- never expected on X360.
    void SaveLoadSystem::FindEntriesDone(u32, u32)
    {
        CGS_ASSERT(false,
            CgsDev::Assert::MessageAt(
                "FindEntriesDone called", KAC_ARTIST_ASSERT_FILE, 1274 /* ARTIST 0x8284CBD4 */));
    }

    // X360 0x8284C648. Realmc delete result callback -- streamed assert stub.
    void SaveLoadSystem::DeleteDone()
    {
        CGS_ASSERT(false, CgsDev::Assert::StreamMessageAt("DeleteDone: not implemented.", "..\\..\\..\\GameShared\\GameClasses\\Gui/CgsSaveLoadX360.cpp", 775));
    }
}
