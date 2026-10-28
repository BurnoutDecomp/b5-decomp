#pragma once
#include "types.hpp"

namespace renderengine
{
struct ResourceDescriptor5
{
    struct Entry
    {
        u32 muSize;
        u32 muAlignment;
    };

    Entry maEntries[5];
};
CGS_STATIC_ASSERT((sizeof(ResourceDescriptor5) == 40), "X360 five-entry resource descriptor");
}
