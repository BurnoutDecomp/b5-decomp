#include "SDKs/EATech/include/NFSMix/NFSMixMapState.hpp"
#include <cstddef> // offsetof

// Compile-gate: instantiate NFSMixMapState (the 96-byte per-state record). Field ORDER
// is the faithful invariant; guard the Initialize-verified members + the params block.
static void NFSMixMapState_embed_check()
{
    (void)sizeof(NFSMixMapState);
    static_assert(sizeof(stMixStateParams) == 5 * sizeof(void*), "stMixStateParams is 5 proc-array bases");
    static_assert(offsetof(NFSMixMapState, m_pNFSMixMap) < offsetof(NFSMixMapState, m_MixStateParams), "order 1");
    static_assert(offsetof(NFSMixMapState, m_MixStateParams) < offsetof(NFSMixMapState, m_StateIndex), "order 2");
    static_assert(offsetof(NFSMixMapState, m_StateIndex) < offsetof(NFSMixMapState, m_ThisStateRefCnt), "order 3");
    static_assert(offsetof(NFSMixMapState, m_MixCtlsAdded) < offsetof(NFSMixMapState, m_pFirstInstance), "order 4");
}
