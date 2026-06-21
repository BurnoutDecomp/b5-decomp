#pragma once

#include "types.hpp"

// CgsMemory::DistributionStream - the shared base of the resource defragmenter's two
// budgeted data movers, GatherStream and ScatterStream.
//
// A "distribution list" describes a set of live resources to relocate: each entry pairs
// a scattered absolute address with a length and an offset into a packed staging buffer
// (the ScratchPool's linear allocator). Execute() arms the stream over a list; each
// Update() then moves up to muBytesToStream bytes, walking a (entry, offset) cursor, and
// reports completion when the last entry is consumed. GatherStream copies scattered ->
// packed (collect resources into the scratch pool); ScatterStream copies packed ->
// scattered (redistribute them back). The transfer is a plain XMemCpy (memcpy), so the
// whole cluster is portable; only the X360 leaf Update() overrides add a 128-byte
// distribution-list alignment assert.
//
// SOURCES: DistributionStream::Execute 0x82867178 (CgsDistributionStream.cpp:80);
// Gather/ScatterStreamBase::UpdateStream 0x828672C0 / 0x82867540. Field order faithful
// (9 dwords on X360); the two address fields widen to pointers for x64 (compiler layout).
namespace CgsMemory
{
    // A 16-byte (X360) distribution-list entry. All three fields are 16-byte aligned
    // (the gather builder asserts this). mpScatteredAddress is the resource's real
    // address; muPackedOffset is its slot inside the packed staging buffer.
    struct DistributionStreamEntry
    {
        u8* mpScatteredAddress;  // [0x00] resource address (dest for scatter, src for gather)
        u32 muLength;            // [0x04] byte length of the entry
        u32 muPackedOffset;      // [0x08] offset of the entry inside the packed base buffer
        u32 muReserved;          // [0x0C]
    };

    class DistributionStream
    {
    public:
        DistributionStream* Construct();
        // @ 0x82867178 - arm the stream over [lpList, lpList+luCount) against the packed
        // base buffer lpBaseAddress, and set the active flag. (The per-Update byte budget
        // muBytesToStream is set separately by the defrag driver.)
        void Execute(const DistributionStreamEntry* lpList, u32 luCount, u8* lpBaseAddress, u32 luField4 = 0);

        void SetBytesPerUpdate(u32 luBytes) { muBytesToStream = luBytes; }
        bool IsActive() const { return (mu32Flags & 1u) != 0; }

    protected:
        u32                            muBytesToStream;   // [0x00] per-Update byte budget
        const DistributionStreamEntry* mpList;            // [0x04] distribution list
        u32                            muListLength;      // [0x08] entry count
        u8*                            mpBaseAddress;     // [0x0C] packed staging base
        u32                            muField4;          // [0x10]
        u32                            muCurrentEntry;    // [0x14] cursor: entry index
        u32                            muEntryOffset;     // [0x18] cursor: offset in entry
        u32                            muStreamPosition;  // [0x1C] running progress
        u32                            mu32Flags;         // [0x20] bit0 = active
    };
}
