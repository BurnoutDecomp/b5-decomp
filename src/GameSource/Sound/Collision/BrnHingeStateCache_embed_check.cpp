#include "GameSource/Sound/Collision/BrnHingeStateCache.h"

// Embed check for BrnSound::Logic::Collision::HingeStateCache. Exercises the one
// bodied ledger function (Update @ 0x826830D8) and confirms the expiry semantics:
// a valid node seen within 0.1s stays valid; one seen earlier is cleared.
// Compile-only (cl /c); not part of the shipped TU.

namespace
{
void ExerciseHingeStateCache()
{
    BrnSound::Logic::Collision::HingeStateCache lCache;

    // Node 0: seen very recently relative to the update time -> stays valid.
    lCache.maEvents[0].mbValid        = true;
    lCache.maEvents[0].mfTimeLastSeen = 10.00f;

    // Node 1: seen long ago relative to the update time -> expires.
    lCache.maEvents[1].mbValid        = true;
    lCache.maEvents[1].mfTimeLastSeen = 5.00f;

    // Node 2: already invalid -> untouched (the guard skips it).
    lCache.maEvents[2].mbValid        = false;
    lCache.maEvents[2].mfTimeLastSeen = 10.00f;

    lCache.Update(10.05f); // currentTime

    const bool lbOk =
        lCache.maEvents[0].mbValid  == true  && // 10.05 - 10.00 = 0.05 < 0.1
        lCache.maEvents[1].mbValid  == false && // 10.05 -  5.00 = 5.05 >= 0.1
        lCache.maEvents[2].mbValid  == false;   // stayed cleared
    if (!lbOk)
    {
        volatile int liFail = 1;
        (void)liFail;
    }
}
} // namespace

int main()
{
    ExerciseHingeStateCache();
    return 0;
}
