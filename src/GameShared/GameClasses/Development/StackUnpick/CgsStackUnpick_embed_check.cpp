// Embed check for CgsStackUnpick: exercises the ledger functions for this TU (already committed)
//   StackUnpickBase::GetStackAddress       @ 0x82819080
//   StackUnpickBase::RemoveFirstItemInStack @ 0x82817360
#include "GameShared/GameClasses/Development/StackUnpick/CgsStackUnpick.h"

namespace
{
    void ExerciseStackUnpick(CgsDev::StackUnpick& lrStack)
    {
        lrStack.Prepare();

        const s32 liCount = lrStack.GetNumStackAddresses();
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            volatile uintptr_t luAddr = lrStack.GetStackAddress(liIndex);
            (void)luAddr;
        }

        lrStack.RemoveFirstItemInStack();
    }
}

extern void CgsStackUnpick_embed_check_anchor(CgsDev::StackUnpick&);
void CgsStackUnpick_embed_check_anchor(CgsDev::StackUnpick& lrStack) { ExerciseStackUnpick(lrStack); }
