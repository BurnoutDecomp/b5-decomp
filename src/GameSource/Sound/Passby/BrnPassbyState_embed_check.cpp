#include "GameSource/Sound/Passby/BrnPassbyState.h"
#include <cstring>

// Embed check for BrnSound::Logic::Passby::PassbyState. Exercises the one bodied
// ledger function (GetStaticTypeInfo @ 0x82688FC8) and confirms the committed
// BrnState base is reused BY NAME. Compile-only (cl /c); not part of the shipped TU.

namespace
{
void ExercisePassbyState()
{
    BrnSound::Logic::Passby::PassbyState lState;

    // Reused-by-name: PassbyState IS-A BrnState IS-A CgsSound::Logic::State.
    BrnSound::Logic::BrnState*      lpBase  = &lState;
    CgsSound::Logic::State*         lpState = &lState;
    (void)lpBase;
    (void)lpState;

    // GetStaticTypeInfo @ 0x82688FC8 — stable address of the per-class descriptor,
    // whose typeName is the recovered class name.
    CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* lpInfo =
        BrnSound::Logic::Passby::PassbyState::GetStaticTypeInfo();
    if (lpInfo == nullptr || std::strcmp(lpInfo->mpcTypeName, "PassbyState") != 0)
    {
        volatile int liFail = 1;
        (void)liFail;
    }

    // The address is stable across calls (function-local static).
    if (lpInfo != BrnSound::Logic::Passby::PassbyState::GetStaticTypeInfo())
    {
        volatile int liFail = 1;
        (void)liFail;
    }
}
} // namespace

int main()
{
    ExercisePassbyState();
    return 0;
}
