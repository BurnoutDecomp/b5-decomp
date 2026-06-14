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
}
