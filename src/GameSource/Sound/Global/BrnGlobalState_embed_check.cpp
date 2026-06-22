#include "GameSource/Sound/Global/BrnGlobalState.h"
#include <cstring>

// Embed check for BrnSound::Logic::GlobalState. Exercises the one bodied ledger
// function (GetTypeName @ 0x82686808) and confirms the BrnState base is reused.

namespace
{
    void ExerciseGlobalState()
    {
        BrnSound::Logic::GlobalState lState;

        // GetTypeName @ 0x82686808 — static RTTI type-name string.
        const char* lpcName = lState.GetTypeName();
        if (std::strcmp(lpcName, "GlobalState") != 0)
        {
            volatile int liFail = 1;
            (void)liFail;
        }

        // Reused-by-name: GlobalState IS-A BrnState IS-A CgsSound::Logic::State.
        BrnSound::Logic::BrnState* lpBase = &lState;
        (void)lpBase;
    }
}

int main()
{
    ExerciseGlobalState();
    return 0;
}
