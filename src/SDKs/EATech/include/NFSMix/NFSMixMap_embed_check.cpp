#include "SDKs/EATech/include/NFSMix/NFSMixMap.hpp"
#include <cstddef> // offsetof

// Compile-gate: instantiate NFSMixMap so the reconstructed 564-byte layout is
// type-checked. Field ORDER is the faithful invariant (x64 widths; X360 offsets in
// the header). Guard the Init-verified members' order + the two array sizes.

static void NFSMixMap_embed_check()
{
    (void)sizeof(NFSMixMap);
    static_assert(sizeof(int[25]) == 100, "m_StateRefCount is 25 ints");
    static_assert(sizeof(int[10][2]) == 80, "m_CurveProcsTotal is 10x2 ints");
    // Init-verified member order (ARTIST @0x82B47E48): mNumStates < m_pNFSMixMaster
    // < m_pStateProcs < m_nStateMapCount < m_fDeltaTimeRatio.
    static_assert(offsetof(NFSMixMap, mNumStates) < offsetof(NFSMixMap, m_pNFSMixMaster), "order 1");
    static_assert(offsetof(NFSMixMap, m_pNFSMixMaster) < offsetof(NFSMixMap, m_pStateProcs), "order 2");
    static_assert(offsetof(NFSMixMap, m_pStateProcs) < offsetof(NFSMixMap, m_nStateMapCount), "order 3");
    static_assert(offsetof(NFSMixMap, m_nStateMapCount) < offsetof(NFSMixMap, m_fDeltaTimeRatio), "order 4");
}
