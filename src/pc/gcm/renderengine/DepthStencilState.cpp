#include "renderstates.h"

namespace renderengine
{
// X360 0x82B636F8: five {0, 1} entries, then the entry-0 qword store 0x0000006000000004
// = { muSize 0x60, muAlignment 4 } -- the full 24-word object (17 state words + 6 widened
// flag words + the initialised word), matching the serialised world MaterialState blocks.
ResourceDescriptor5* DepthStencilState::GetResourceDescriptor(ResourceDescriptor5* lpDescriptor)
{
    for (ResourceDescriptor5::Entry& lEntry : lpDescriptor->maEntries)
    {
        lEntry.muSize = 0;
        lEntry.muAlignment = 1;
    }

    lpDescriptor->maEntries[0].muSize = static_cast<u32>(sizeof(DepthStencilState));  // X360: 0x60
    lpDescriptor->maEntries[0].muAlignment = 4;
    static_assert(sizeof(DepthStencilState) == 0x60, "the X360 descriptor sizes the object at 0x60");
    return lpDescriptor;
}

// X360 the immediate-mode state-library builder passes the params alongside the out descriptor; the
// descriptor build is a fixed { size, align } block and ignores the params. Delegates to the 1-arg form.
ResourceDescriptor5* DepthStencilState::GetResourceDescriptor(
    ResourceDescriptor5* lpDescriptor, const Parameters* /*lpParameters*/)
{
    return GetResourceDescriptor(lpDescriptor);
}

// Create a depth/stencil state from the parameter block (the allocator has already carved the backing
// store the descriptor sized; *ppState is the first handle). The 17 state words copy through, the six
// Parameters flag bytes widen to the trailing words, and the initialised word is set -- the attested
// sibling pattern (BlendState::Initialize 0x82B627C8: lwz/stw word copies, lbz->stw flag widening,
// li 1 -> stw initialised; the DepthStencilState body 0x82B62890 itself has no export).
DepthStencilState* DepthStencilState::Initialize(DepthStencilState** ppState, const Parameters* lpParameters)
{
    DepthStencilState* lpState = *ppState;
    const u32* lpWords = reinterpret_cast<const u32*>(lpParameters);
    for (int liWord = 0; liWord < 17; ++liWord)
        lpState->maState[liWord] = lpWords[liWord];
    lpState->mauFlagWords[0] = lpParameters->mbDepthTestEnable;
    lpState->mauFlagWords[1] = lpParameters->mbDepthWriteEnable;
    lpState->mauFlagWords[2] = lpParameters->mu8Flag2;
    lpState->mauFlagWords[3] = lpParameters->mu8Flag3;
    lpState->mauFlagWords[4] = lpParameters->mu8Flag4;
    lpState->mauFlagWords[5] = lpParameters->mu8Flag5;
    lpState->muInitialised = 1u;
    return lpState;
}
}
