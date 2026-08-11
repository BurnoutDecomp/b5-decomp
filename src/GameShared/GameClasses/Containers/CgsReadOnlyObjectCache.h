#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// =============================================================================
// GameShared/GameClasses/Containers/CgsReadOnlyObjectCache.h
//
// CgsContainers::ReadOnlyObjectCache<Type> -- a fetch/release cache over a
// caller-owned, read-only source buffer of Type records. Because the buffer is
// read-only and always resident, a "fetch" is a direct pointer into the source
// (no copy / DMA), and "release" is therefore a no-op. Reconstructed from the
// DecFIGS DWARF (references/DecFIGS/dwarfdump/GameShared/GameClasses/Containers/
// CgsReadOnlyObjectCache.h) and the X360 ARTIST bodies the build emitted out-of-
// line for the <PolygonSoupLeafNode> instantiation:
//
//   Construct @0x829170F8   (guard source/count, store the two members)
//   Release   @0x829172D0   (empty -- direct pointer into read-only source)
//
// SHAPE (DWARF CgsReadOnlyObjectCache.h:68-106):
//   const Type* mpSourceBuffer;      // +0x00  :105
//   s32         miNumSourceEntries;  // +0x04  :106
// The remaining members (StartFetch/WaitForFetch/Fetch/ReleaseAll/GetNumUsedSlots/
// GetNumFreeSlots) are declared-only here -- their bodies are not attested in this
// batch; grow them in place when their symbols land. This is the ONE shared home
// for the generic; each per-Type instantiation TU emits only `template ...;` lines.
// =============================================================================

namespace CgsContainers
{

template <class Type>
class ReadOnlyObjectCache
{
public:
    // Construct @0x829170F8 (<PolygonSoupLeafNode> instantiation)
    // X360: guard(source != NULL); guard(count >= 0); mpSourceBuffer = source;
    //       miNumSourceEntries = count;
    // The last two int arguments are stored to the stack (arg_2C/arg_34) but are
    // otherwise unused by this instantiation's body (DWARF signature is
    // Construct(const T*, int, int, int)).
    void Construct(const Type* lpSourceBuffer, s32 liNumSourceEntries, s32 liArg2, s32 liArg3)
    {
        (void)liArg2;
        (void)liArg3;

        CGS_ASSERT(lpSourceBuffer != nullptr, "Source data is NULL\n");
        CGS_ASSERT(liNumSourceEntries >= 0, "Source data count must be positive\n");

        mpSourceBuffer     = lpSourceBuffer;
        miNumSourceEntries = liNumSourceEntries;
    }

    // ⭐ Get @0x82917218 (46, <PolygonSoupLeafNode> instantiation), ADDED 2026-08-10
    // (fill-worker wave 2). Bounds-assert then hand back a DIRECT pointer into the
    // read-only source buffer -- no copy, which is why Release is a no-op.
    //   0x82917230  cmpwi r11, 0 ; blt  -> index >= 0
    //   0x82917240  lwz   r11, 4(this)  -> miNumSourceEntries
    //   0x82917244  cmpw  r10, r11 ; blt -> index < count
    //   0x8291725C  "Index out of range\n", CgsReadOnlyObjectCache.h:0x109 == 265
    //   0x829172B0  mulli r11, r11, 0x30 -> the CONSOLE'S element stride
    //   0x829172B8  lwz   r10, 0(this)  ; add r3, r10, r11
    // ⚠️ The 0x30 is the console's sizeof(PolygonSoupLeafNode). It is NOT reproduced as
    // a literal: the host element type is indexed directly, and CgsPolygonSoupSpacialNode.h
    // already gates both node types at 48 bytes on x64 as well (the alignas(16) inside
    // AxisAlignedBox absorbs the pointer widening). Both callers of this instantiation --
    // FillTriangleCache and RunJobQuery -- reach elements by index, never by byte offset.
    const Type* Get(s32 liIndex) const
    {
        CGS_ASSERT(liIndex >= 0 && liIndex < miNumSourceEntries, "Index out of range\n"); // :265

        return &mpSourceBuffer[liIndex];
    }

    // DWARF :190 / :232 / :263 -- declared-only (bodies not attested in this batch).
    s32         StartFetch(s32 liIndex);
    const Type* WaitForFetch(s32 liIndex);
    const Type* Fetch(s32 liIndex);

    // Release @0x829172D0 (<PolygonSoupLeafNode> instantiation)
    // X360 body is empty (stores its two args then returns) -- the fetched entry is a
    // direct pointer into the read-only source buffer, so releasing it is a no-op.
    void Release(const Type* lpEntry)
    {
        (void)lpEntry;
    }

    // DWARF :316 / :358 / :379 -- declared-only (bodies not attested in this batch).
    void ReleaseAll();
    s32  GetNumUsedSlots() const;
    s32  GetNumFreeSlots() const;

private:
    const Type* mpSourceBuffer;      // +0x00  :105
    s32         miNumSourceEntries;  // +0x04  :106
};

} // namespace CgsContainers
