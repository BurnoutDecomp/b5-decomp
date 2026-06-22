// Tiny embed check: proves GameTalk.h is self-contained and the EA::GameTalk
// namespace-level helpers (Alloc/Free/StrIEqual) are usable by name with the
// proven X360 signatures. Compile-only; no linkage.

#include "SDKs/EA/GameTalk/GameTalk.h"

namespace
{
    bool GameTalkFuncsEmbedCheck()
    {
        using namespace EA::GameTalk;

        void* lpBlock = Alloc(64);
        lpBlock = Free(lpBlock);
        (void)lpBlock;

        return StrIEqual("Camera", "camera");
    }
}
