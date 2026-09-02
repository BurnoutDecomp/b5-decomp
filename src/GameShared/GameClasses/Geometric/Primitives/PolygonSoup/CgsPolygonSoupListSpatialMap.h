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
// RunJobQuery's fifth parameter (the DWARF types it; the X360 body never dereferences it).
#include "GameShared/GameClasses/Containers/CgsReadOnlyObjectCache.h"

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

        // =========================================================================================
        // ⭐⭐ ADDED 2026-08-10 (fill-worker wave 2): the JOB-SIDE box query — the function the
        // triangle-cache fill worker actually runs, and the one every previous costing of this leg
        // missed while naming its sibling.
        //
        // ⚠️ THE NAME IS RECOVERED, NOT INVENTED. X360 @0x82844680 (316) carries NO IDA symbol.
        // Its single caller is PolygonSoupTesterJob::RunBoxQuery @0x82916D28, and the PS3 DWARF
        // mangle types every one of the six arguments:
        //   _ZNK12CgsGeometric25PolygonSoupListSpatialMap11RunJobQueryERKNS_14AxisAlignedBoxE
        //     PNS_25PolygonSoupJobQueryParamsEPPtPiPN13CgsContainers19ReadOnlyObjectCache
        //     INS_22PolygonSoupSpacialNodeEEE                                   @0xB63F20 (405)
        //
        // ⭐ It is the CONST, job-side twin of RunQuery @0x82843A80 (261, asserts at
        // CgsPolygonSoupListSpatialMap.cpp:447; this one asserts at :614 — same file, two
        // overloads). The difference is entirely about ownership: RunQuery ping-pongs through the
        // map's OWN mapQueryBuffers and publishes into mpOutputQueryBuffer/miLastQueryResultCount,
        // which a job running off a read-only DMA'd copy of the map cannot do. RunJobQuery takes
        // the two scratch buffers in a params block and returns its answer through out-params, so
        // the map stays const.
        //
        // ⚠️ The sixth argument is a ReadOnlyObjectCache<PolygonSoupSpacialNode>* the caller
        // carves (8 bytes). The X360 body never dereferences it: it indexes the level's node array
        // directly. It is threaded through faithfully (the SPU build fetches nodes through it) and
        // named UNUSED-AS-SHIPPED rather than dropped, per the standing "prefer the DWARF arity"
        // rule.
        // =========================================================================================
        struct PolygonSoupJobQueryParams
        {
            u16* mpaQueryBufferA;   // +0x00  ping  (RunBoxQuery carves 4096 B == 2048 u16)
            u16* mpaQueryBufferB;   // +0x04  pong
            s32  miQueryBufferSize; // +0x08  capacity in ENTRIES (RunBoxQuery passes 2048)
        };

        // RunQuery @0x82843A80 (261) -- the SYNCHRONOUS box query (scene-query wave 1b,
        // 2026-09-02; body in CgsPolygonSoupListSpatialMap_Query.cpp). The same breadth-first
        // level sweep as RunJobQuery but through the map's OWN ping-pong buffers
        // (mapQueryBuffers[0]/[1], capacity miQueryBufferSize), publishing the buffer the last
        // level wrote into mpOutputQueryBuffer and its count into miLastQueryResultCount. Returns
        // the count; a map with no levels returns 0 and writes nothing. Its overflow assert is
        // CgsPolygonSoupListSpatialMap.cpp:447 and prints the query box. The one caller is
        // BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest @0x828131C0 (the short-line
        // arm), which then reads mpOutputQueryBuffer[i] as LEAF indices into GetLeafNodes().
        s32 RunQuery(const AxisAlignedBox& lrQueryBox);

        // The last RunQuery's leaf-index list (+0x58 / +0x64 on the console; the caller reads both
        // fields directly at 0x8281328C/0x82813294).
        const u16* GetOutputQueryBuffer() const { return mpOutputQueryBuffer; }
        s32 GetLastQueryResultCount() const { return miLastQueryResultCount; }

        s32 RunJobQuery(const AxisAlignedBox&                                      lrQueryBox,
                        PolygonSoupJobQueryParams*                                 lpParams,
                        u16**                                                      lppaOutResults,
                        s32*                                                       lpiOutNumResults,
                        CgsContainers::ReadOnlyObjectCache<PolygonSoupSpacialNode>* lpNodeCache) const;
    };
}
