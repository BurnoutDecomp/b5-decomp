#pragma once

// CgsResource::ResourceHandle — a handle onto a loaded resource (the resource memory
// plus its source pool entry). Reconstructed from the DecFIGS DWARF; minimal recon of
// the two stored pointers (accessors are inlined / live in their own TUs).
#include "types.hpp"

namespace CgsResource
{
    class Entry;

    struct ResourceHandle
    {
        void* mpResourceMemory;
        Entry* mpSourceEntry;
    };
}
