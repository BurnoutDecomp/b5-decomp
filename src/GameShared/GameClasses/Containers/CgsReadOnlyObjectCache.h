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
