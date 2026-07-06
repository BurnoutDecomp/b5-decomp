// Compile-only embed check for AptTarget.h -- verifies the Apt context/director
// struct is well-formed and its named members are reachable (the shape the ~100
// off_8324E574 dependents will access). Not a runtime TU.

#include "SDKs/EATech/include/Apt/AptTarget.h"
#include "SDKs/EATech/include/Apt/AptLoader.h"

namespace
{
    // Exercise the named members + the accessor exactly as the dependents do.
    // FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
    void AptTarget_EmbedCheck()
    {
        AptTarget* pCtx = gpAptTarget;                 // off_8324E574 (the current context)

        AptAnimationTarget* pAnim   = pCtx->GetAnimationTarget();   // +0x18
        AptAnimationTarget* pAnim2  = pCtx->mpAnimationTarget;      // same, by member
        AptLoader*          pLoader = pCtx->mpLoader;               // +0x1C (AptLinker::Update uses this)
        AptLinker*          pLinker = pCtx->mpLinker;               // +0x20
        void*               pF24    = pCtx->mpField24;              // +0x24
        u32                 nCfg    = pCtx->mnConfigE;              // +0x10

        (void)pAnim; (void)pAnim2; (void)pLoader; (void)pLinker; (void)pF24; (void)nCfg;
        (void)gpAptTargetCurrent; (void)gpAptTargetTLS;
    }
}
