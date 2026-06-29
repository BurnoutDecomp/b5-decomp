#include "renderstates.h"

namespace renderengine
{
ResourceDescriptor5* DepthStencilState::GetResourceDescriptor(ResourceDescriptor5* lpDescriptor)
{
    for (ResourceDescriptor5::Entry& lEntry : lpDescriptor->maEntries)
    {
        lEntry.muSize = 0;
        lEntry.muAlignment = 1;
    }

    lpDescriptor->maEntries[0].muSize = 6;
    lpDescriptor->maEntries[0].muAlignment = 4;
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
// store the descriptor sized; *ppState is the first handle). The X360 marshals the params into the GPU
// depth/stencil descriptor; here the leading 17 state words are recorded into the state's state block.
DepthStencilState* DepthStencilState::Initialize(DepthStencilState** ppState, const Parameters* lpParameters)
{
    DepthStencilState* lpState = *ppState;
    const u32* lpWords = reinterpret_cast<const u32*>(lpParameters);
    for (int liWord = 0; liWord < 17; ++liWord)
        lpState->maState[liWord] = lpWords[liWord];
    return lpState;
}
}
