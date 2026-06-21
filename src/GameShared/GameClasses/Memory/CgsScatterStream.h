#pragma once

#include "GameShared/GameClasses/Memory/CgsDistributionStream.h"

// CgsMemory::ScatterStream - the "packed -> scattered" half of the resource defragmenter
// data mover (see CgsDistributionStream.h). Each Update() copies up to muBytesToStream
// bytes out of the packed staging buffer (mpBaseAddress + entry.muPackedOffset) to the
// resources' real scattered addresses (entry.mpScatteredAddress); i.e. it redistributes
// resources from the scratch pool back into the live heap.
//
// SOURCES: CgsMemory::ScatterStreamBase::UpdateStream 0x82867540 (the portable memcpy
// core, CgsScatterStream.cpp) + the X360 leaf 0x82868680/0x828686B0 (X360/
// CgsScatterStreamX360.cpp - adds the 128-byte alignment asserts then calls the base).
namespace CgsMemory
{
    class ScatterStreamBase : public DistributionStream
    {
    public:
        // @ 0x82867540 - stream up to muBytesToStream bytes packed -> scattered; returns
        // true (and clears active) once the whole distribution list is consumed.
        bool UpdateStream();
    };

    class ScatterStream : public ScatterStreamBase
    {
    public:
        ScatterStream* Construct();
        bool Update();
    };
}
