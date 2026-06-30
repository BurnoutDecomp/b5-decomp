// Compile-only embed check for AptAnimationTarget.h -- verifies the 88-byte Apt
// animation director composes with AptTarget (its owner) and that the named members
// the dependents reach (the root display list, the action queue, the interval timers)
// are accessible. Not a runtime TU.

#include "SDKs/EATech/include/Apt/AptTarget.h"
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"
#include "SDKs/EATech/include/Apt/AptActionQueueC.h"   // mpActionQueue as a complete type

namespace
{
    void AptAnimationTarget_EmbedCheck()
    {
        // Reach the director the way the runtime does: through the context singleton.
        AptAnimationTarget* pAnim = gpAptTarget->GetAnimationTarget();

        AptDisplayList*   pRoot   = pAnim->GetRootDisplayList();   // &mDisplayList (+0x20)
        AptDisplayList*   pRoot2  = &pAnim->mDisplayList;          // same
        AptActionQueueC*  pQueue  = pAnim->mpActionQueue;          // +0x0C
        u32               nQueue  = pQueue ? pQueue->mnCapacity : 0u;  // complete type
        AptIntervalTimer* pTimers = pAnim->mpIntervalTimers;       // +0x24
        u32               nTimers = pAnim->mnNumIntervalTimers;    // +0x04
        u32*              pInputs = pAnim->mpQueuedInputs;          // +0x2C (per-frame input ring)
        AptValue**        pLstSlt = pAnim->mListenerSet.mppSlots;   // +0x14 (listener-set slots)

        (void)pRoot; (void)pRoot2; (void)pQueue; (void)nQueue; (void)pTimers; (void)nTimers; (void)pInputs; (void)pLstSlt;
    }
}
