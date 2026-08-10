#pragma once

// CgsGeometric::PolygonSoupListSpatialMap — a runtime spatial-query structure over a
// set of PolygonSoupLists. It holds the per-list resource handles plus a multi-level
// (up to KI_MAX_LEVELS) spatial partition (parent grid nodes + leaf nodes) and the
// scratch query buffers used to run box/line queries against that partition.
//
// This is a RUNTIME object built in-process (not an on-disk relocated resource), so it
// is modelled with host-native NAMED members; the node types live in their own TUs and
// are referenced only by pointer here, so they are forward-declared.
//
// Class shape from the DecFIGS DWARF (CgsPolygonSoupListSpatialMap.h members).
#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h" // CgsResource::ResourceHandle (members accessed)
// ⭐ 2026-08-10 (spatial-partition wave): the two node types are now COMPLETE (they were
// forward-declared while nothing could build them). BuildSpacialPartition carves both, so
// it needs their real 48-byte layouts -- and completing them here is what lets
// GetPolygonSoup's hard-coded 0x30 element step be static_assert-gated instead of trusted.
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupSpacialNode.h"

namespace CgsMemory { class LinearMalloc; }   // BuildSpacialPartition allocator (by pointer)

namespace CgsGeometric
{
    struct PolygonSoupList;         // forward-decl (GetPolySoupList return type; CgsPolygonSoupList.h)

    struct PolygonSoupListSpatialMap
    {
        static const s32 KI_MAX_LEVELS = 8;

    private:
        CgsResource::ResourceHandle* mpaPolySoupListHandles; // result[0]    handle array (one per soup list)
        s32                          miNumSoupLists;          // result[1]    active handle/list count
        PolygonSoupSpacialNode*      mapParentNodes[8];       // result[2..9]  per-level parent grid nodes
        s32                          maiParentNodeCounts[8];  // result[10..17] per-level parent node counts
        PolygonSoupLeafNode*         mpLeafNodes;             // result[18]   leaf node array
        s32                          miLeafNodeCount;         // result[19]   leaf node count
        u16*                         mapQueryBuffers[2];      // result[20..21] ping-pong query scratch buffers
        u16*                         mpOutputQueryBuffer;     // result[22]   query result buffer
        s32                          miNumLevels;             // result[23]   partition level count
        s32                          miQueryBufferSize;       // result[24]   query buffer capacity
        s32                          miLastQueryResultCount;  // result[25]   number of results from the last query

    public:
        // Construct @0x82839600 — zero the handle pointer + count (minimal POD init).
        void Construct();
        // Prepare @0x82839610 — point at a caller-owned handle array of the given size.
        void Prepare(CgsResource::ResourceHandle* lpaHandles, s32 liNumSoupLists);
        // Clear @0x82839620 — reset the partition/query scratch fields + every active handle, then zero the count.
        void Clear();
        // GetPolySoupList @0x8283AB40 — fetch the (asserted-valid) PolygonSoupList for a slot.
        const PolygonSoupList* GetPolySoupList(s32 liPolySoupListIndex) const;
        // AddList @0x82839680 — bump the active list count (the X360 body ignores its argument).
        void AddList(const PolygonSoupList* lpPolySoupList);

        // The active handle/list count (result[1] / +0x04). Read by
        // TriangleCollisionManager::ProcessAddPolySoupListEvents to assert its own count matches.
        s32 GetNumPolySoupLists() const { return miNumSoupLists; }

        // BuildSpacialPartition @0x82841740 — (re)build the multi-level spatial partition over
        // the active soup lists, carving all node/index/query storage from lpAllocator.
        // Body in CgsPolygonSoupListSpatialMap_Build.cpp.
        //
        // ⚠️ PARAMETER NAMES CORRECTED 2026-08-10 (spatial-partition wave). They previously read
        // `liListsPerNode` / `liItemBudget` and were commented as a per-node list cap and an item
        // budget. That was a fabricated reading of the call site's literal `(…, 8, 2048)`. The PS3
        // DWARF names them, and the body agrees with DWARF on both counts:
        //   ._ZN12CgsGeometric25PolygonSoupListSpatialMap21BuildSpacialPartition
        //     EPN9CgsMemory12LinearMallocEii   @0xB605EC
        //   void BuildSpacialPartition(LinearMalloc* lpAllocator, int32_t liNumLevels,
        //                              int32_t liQueryBufferSize)
        // liNumLevels is asserted 1..8 by the body's own "Num levels must be between 1 and 8"
        // (i.e. it is KI_MAX_LEVELS, hence the 8), and liQueryBufferSize is stored verbatim into
        // miQueryBufferSize after sizing both mapQueryBuffers (hence the 2048).
        void BuildSpacialPartition(CgsMemory::LinearMalloc* lpAllocator, s32 liNumLevels, s32 liQueryBufferSize);

        // GetNumLeafNodes @0x82917018 — return the leaf-node count (result[19] / +0x4C).
        s32 GetNumLeafNodes() const;

        // The leaf-node array itself (result[18] / +0x48), i.e. "has a spatial partition been
        // built?". ⚠️ FLAG (header grow, 2026-08-10 cache-fill wave): the console reads the
        // member DIRECTLY at its one attested consumer -- TriangleCacheManager::
        // StartUpdateTriangleCaches @0x828BED68 `lwz r11, 0x48(r29) ; beq <epilogue>` -- so no
        // symbol attests this accessor's name. It is added for the same reason (and by the same
        // precedent) as GetNumPolySoupLists above: a private member read by another class.
        const PolygonSoupLeafNode* GetLeafNodes() const { return mpLeafNodes; }
        // GetPolygonSoup @0x8280FFD0 — return &mpLeafNodes[index] (base +0x48, asm element
        // stride 0x30). The leaf-node element type is declared-only, so the 0x30 stride is
        // honoured explicitly (opaque-element-stride precedent; see dep_flags).
        PolygonSoupLeafNode* GetPolygonSoup(s32 liIndex) const;
    };
}
