#include "SDKs/EATech/include/NFSMix/NFSMixMaster.hpp"
#include <cstddef> // offsetof

// Compile-gate: force NFSMixMaster (and its MixerMemBase base) to instantiate so the
// reconstructed layout is type-checked by the build. The field ORDER is the faithful
// invariant (x64 widths; X360 offsets in the header); a couple of order asserts guard it.

static void NFSMixMaster_embed_check()
{
    (void)sizeof(NFSMixMaster);
    static_assert(sizeof(int[25]) == 100, "m_StateRefCount is 25 ints");
    // field order (x64): the data block follows the empty MixerMemBase base at +0
    static_assert(offsetof(NFSMixMaster, m_pMainMixMap) < offsetof(NFSMixMaster, m_StateRefCount),
                  "m_pMainMixMap precedes m_StateRefCount");
    static_assert(offsetof(NFSMixMaster, m_StateRefCount) < offsetof(NFSMixMaster, m_pMixMaster),
                  "m_StateRefCount precedes the trailing m_pMixMaster");
}
