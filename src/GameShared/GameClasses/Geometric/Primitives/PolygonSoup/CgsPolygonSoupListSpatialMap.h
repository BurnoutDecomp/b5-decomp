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

namespace CgsGeometric
{
    struct PolygonSoupList;         // forward-decl (GetPolySoupList return type; defined in CgsPolygonSoupList.cpp)
    struct PolygonSoupSpacialNode;  // forward-decl (pointer member only; own TU, embeds AxisAlignedBox)
    struct PolygonSoupLeafNode;     // forward-decl (pointer member only; own TU, embeds AxisAlignedBox)

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

        // GetNumLeafNodes @0x82917018 — return the leaf-node count (result[19] / +0x4C).
        s32 GetNumLeafNodes() const;
        // GetPolygonSoup @0x8280FFD0 — return &mpLeafNodes[index] (base +0x48, asm element
        // stride 0x30). The leaf-node element type is declared-only, so the 0x30 stride is
        // honoured explicitly (opaque-element-stride precedent; see dep_flags).
        PolygonSoupLeafNode* GetPolygonSoup(s32 liIndex) const;
    };
}
