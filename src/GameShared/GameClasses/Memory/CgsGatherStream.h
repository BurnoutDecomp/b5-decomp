#pragma once

#include "GameShared/GameClasses/Memory/CgsDistributionStream.h"

// CgsMemory::GatherStream - the "scattered -> packed" half of the resource defragmenter
// data mover (see CgsDistributionStream.h). Each Update() copies up to muBytesToStream
// bytes from the resources' real scattered addresses (entry.mpScatteredAddress) into the
// packed staging buffer (mpBaseAddress + entry.muPackedOffset); i.e. it collects live
// resources into the scratch pool ready to be relocated.
//
// SOURCES: CgsMemory::GatherStreamBase::UpdateStream 0x828672C0 (the portable memcpy
// core, CgsGatherStream.cpp) + the X360 leaf 0x82868568 (X360/CgsGatherStreamX360.cpp -
// adds the 128-byte alignment asserts then calls the base). Mirror of ScatterStream with
// the copy direction reversed.
namespace CgsMemory
{
    class GatherStreamBase : public DistributionStream
    {
    public:
        // @ 0x828672C0 - stream up to muBytesToStream bytes scattered -> packed; returns
        // true (and clears active) once the whole distribution list is consumed.
        bool UpdateStream();
    };

    class GatherStream : public GatherStreamBase
    {
    public:
        GatherStream* Construct();
        bool Update();
    };
}
