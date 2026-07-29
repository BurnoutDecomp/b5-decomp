#include "SharedClasses/AI/AISectionsResourceType.h"          // BrnAI::AISection / AISectionsData (layout home)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"    // CgsMemory::LinearMalloc (the scratch allocator)
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT

#include <cstdint>   // uintptr_t (load-time pointer relocation arithmetic)

#include <cstring>   // memset
#include <cmath>     // ceilf, floorf

// BrnAI::AISectionsData spatial-lookup bodies.
//
//   BrnAI::AISectionsData::BuildAISectionPointMap @ 0x8267A688
//   BrnAI::AISectionsData::FindNearestAISection   @ 0x82676CC0   (the (Vector3, AISectionPointMap*) overload)
//
// These two build and query a uniform-grid spatial acceleration structure over the AI
// sections. The grid is KI_GRID_WIDTH x KI_GRID_HEIGHT (100 x 100) cells covering the
// section AABB in the horizontal (world X, world Z) plane; each section is bucketed by
// the cell that contains its middle (BrnAI::AISection::GetMiddle). FindNearestAISection
// expands a square ring of cells around the query position until a section whose middle is
// nearest is found, returning that section's index.
//
// The GetAISection/GetMiddle/GetSizeInBytes members live in AISectionsResourceType.cpp;
// this TU only adds the two spatial functions. The AISectionPointMap layout is X360-asm
// authoritative (the DWARF AISectionPointMap orders the two Vector2 grid fields before the
// pointers; the X360 build stores mvGridUnitSize @+0x10, mvGridMin @+0x20 and the five
// pointers @+0x30..+0x40, so the struct is modelled to those offsets and pinned below).

namespace BrnAI
{
    // Grid dimensions baked into the X360 asm (li r11, 100 stored at map+0x00/+0x04;
    // the count arrays are Malloc'd at 20000 bytes == 10000 u16 == 100*100; the asserts
    // read "liGridX < KI_GRID_WIDTH", "liGridY < KI_GRID_HEIGHT",
    // "liGridUnit < (KI_GRID_WIDTH*KI_GRID_HEIGHT)").
    static const s32 KI_GRID_WIDTH  = 100;
    static const s32 KI_GRID_HEIGHT = 100;

    // BrnAI::AISectionPointMap -- the uniform-grid bucket index produced by
    // BuildAISectionPointMap. DWARF home SharedClasses/AI/AISectionsData.h:540 gives the
    // member NAMES/TYPES. The X360 stores (BuildAISectionPointMap @0x8267A688) place the
    // header ints at +0x00/+0x04, the two Vector2 grid fields 16-aligned at +0x10/+0x20, and
    // the five bucket pointers at +0x30..+0x40 (map Malloc'd at 0x50 == 80 console bytes).
    // The PC reconstruction keeps the same field ORDER and reaches every member by NAME (the
    // X360 raw-offset indexing v6[12]..v6[16] is reproduced as named member access); byte
    // offsets are NOT static-pinned here because PC pointers are 8 bytes -- matching the
    // committed AISectionsData/AISection layout convention, which likewise documents the
    // console offsets in comments without static_assert.
    struct alignas(16) AISectionPointMap
    {
        s32      miGridWidth;             // +0x00  cell count along world-X (== KI_GRID_WIDTH)
        s32      miGridHeight;            // +0x04  cell count along world-Z (== KI_GRID_HEIGHT)
        Vector2  mvGridUnitSize;          // +0x10  world-space cell SIZE (extent / 100); FindNearest reciprocates it
        Vector2  mvGridMin;               // +0x20  AABB-min corner of the grid (floor'd)
        Vector3* mpPoints;                // +0x30  per-valid-section middle (parallel to mpuSections)
        u16*     mpuSections;             // +0x34  per-valid-section -> original section index
        u16*     mpuGridUnitMap;          // +0x38  scatter array: bucketed valid-section indices
        u16*     mpuGridUnitPointIndices; // +0x3C  prefix-sum first-index per grid cell
        u16*     mpuGridUnitPointCount;   // +0x40  count of points per grid cell
    };

    // ------------------------------------------------------------------------------------
    // BrnAI::AISectionsData::BuildAISectionPointMap @ 0x8267A688
    //
    // Allocates the AISectionPointMap and its bucket arrays out of lpMalloc (16-aligned),
    // then:
    //   1. Walks every section; for the ones usable for spatial lookup (those NOT flagged
    //      IsUnsuitableForResetOnTrackLink -- mx8Flags bit 0x01 or 0x40), records the
    //      section's middle and original index, and accumulates the world-space AABB.
    //   2. Floors the AABB min and (floor+1)'s the max, then derives a per-axis
    //      cell-inverse scale = KI_GRID_WIDTH / (max - min) so a world position maps to a
    //      [0,100) grid cell via floor((pos - min) * cellInv).
    //   3. Histograms each valid point's grid cell (counts), prefix-sums the counts into
    //      first-indices, then scatters the valid-section indices into the grid-unit map.
    //   4. Fills the map header and (in the original) runs a debug pass asserting every
    //      stored point lies within its assigned cell's bounds.
    // ------------------------------------------------------------------------------------
    AISectionPointMap* AISectionsData::BuildAISectionPointMap(CgsMemory::LinearMalloc* lpMalloc) const
    {
        lpMalloc->SetAlignment(16);

        AISectionPointMap* lpMap   = static_cast<AISectionPointMap*>(lpMalloc->Malloc(sizeof(AISectionPointMap)));
        Vector3*           lpPoints = static_cast<Vector3*>(lpMalloc->Malloc(16 * muNumSections));
        u16*               lpuSections  = static_cast<u16*>(lpMalloc->Malloc(2 * muNumSections));
        u16*               lpuGridUnit  = static_cast<u16*>(lpMalloc->Malloc(2 * muNumSections));

        CGS_ASSERT(lpMap && lpPoints && lpuSections && lpuGridUnit,
                   "lpMap && lpPoints && lpuSections && lpuGridUnit");

        // Build the per-point arrays and the section AABB. Min seeds at +100000, max at
        // -100000 (the X360 floats flt_820080E8 / flt_820936E0).
        Vector3 lGridMin = { 100000.0f, 100000.0f, 100000.0f, 100000.0f };
        Vector3 lGridMax = { -100000.0f, -100000.0f, -100000.0f, -100000.0f };

        u16 luPointCount = 0;
        for (u32 luSectionIndex = 0; luSectionIndex < muNumSections; ++luSectionIndex)
        {
            const AISection& lSection = mpaSections[luSectionIndex];
            if (lSection.IsUnsuitableForResetOnTrackLink())
            {
                continue;
            }

            Vector3 lMiddle = lSection.GetMiddle();
            lpPoints[luPointCount]    = lMiddle;
            lpuSections[luPointCount] = static_cast<u16>(luSectionIndex);
            ++luPointCount;

            lGridMin.x = lMiddle.x < lGridMin.x ? lMiddle.x : lGridMin.x;
            lGridMin.y = lMiddle.y < lGridMin.y ? lMiddle.y : lGridMin.y;
            lGridMin.z = lMiddle.z < lGridMin.z ? lMiddle.z : lGridMin.z;
            lGridMin.w = lMiddle.w < lGridMin.w ? lMiddle.w : lGridMin.w;
            lGridMax.x = lMiddle.x > lGridMax.x ? lMiddle.x : lGridMax.x;
            lGridMax.y = lMiddle.y > lGridMax.y ? lMiddle.y : lGridMax.y;
            lGridMax.z = lMiddle.z > lGridMax.z ? lMiddle.z : lGridMax.z;
            lGridMax.w = lMiddle.w > lGridMax.w ? lMiddle.w : lGridMax.w;
        }

        // Snap the AABB to integer world units: floor the min, floor-then-+1 the max (the
        // X360 vrfim is floor; the +1 is vaddfp of a broadcast 1.0 from vcsxwfp(vspltisw 1)).
        lGridMin.x = floorf(lGridMin.x); lGridMin.y = floorf(lGridMin.y);
        lGridMin.z = floorf(lGridMin.z); lGridMin.w = floorf(lGridMin.w);
        lGridMax.x = floorf(lGridMax.x) + 1.0f; lGridMax.y = floorf(lGridMax.y) + 1.0f;
        lGridMax.z = floorf(lGridMax.z) + 1.0f; lGridMax.w = floorf(lGridMax.w) + 1.0f;

        // Per-cell bucket arrays: KI_GRID_WIDTH*KI_GRID_HEIGHT u16 counts/first-indices
        // (10000 entries == 20000 bytes), and one u16 per valid point for the scatter map.
        u16* lpuGridUnitPointCounts  = static_cast<u16*>(lpMalloc->Malloc(2 * KI_GRID_WIDTH * KI_GRID_HEIGHT));
        u16* lpuGridUnitPointIndices = static_cast<u16*>(lpMalloc->Malloc(2 * KI_GRID_WIDTH * KI_GRID_HEIGHT));
        u16* lpuGridPointMap         = static_cast<u16*>(lpMalloc->Malloc(2 * luPointCount));

        CGS_ASSERT(lpuGridUnitPointCounts && lpuGridUnitPointIndices && lpuGridPointMap,
                   "lpuGridUnitPointCounts && lpuGridUnitPointIndices && lpuGridPointMap");

        memset(lpuGridUnitPointCounts, 0, 2 * KI_GRID_WIDTH * KI_GRID_HEIGHT);

        // Two grid factors per axis, both derived from extent = (gridMax - gridMin):
        //   recipCellSize = KI_GRID_WIDTH / extent  (== cellInv; world position -> cell)
        //   cellSize      = extent / KI_GRID_WIDTH  (== 1 / cellInv; cell -> world span)
        // The X360 builds BOTH via vrefp + Newton-Raphson refine: cellInv (v127) is the
        // transient histogram factor used to bucket points below; cellSize is what gets
        // STORED into mvGridUnitSize (the asm reciprocates mvGridUnitSize again in
        // FindNearest). Mapping a position to a cell is floor((pos - gridMin) * cellInv).
        // The horizontal plane uses world (X, Z); lane mapping follows the X360 vperm
        // shuffles (cell.X <- world X, cell.Y <- world Z).
        Vector3 lGridExtent;
        lGridExtent.x = lGridMax.x - lGridMin.x;
        lGridExtent.y = lGridMax.y - lGridMin.y;
        lGridExtent.z = lGridMax.z - lGridMin.z;
        lGridExtent.w = lGridMax.w - lGridMin.w;

        Vector3 lRecipCellSize;   // cellInv = 100 / extent (histogram factor; not stored)
        lRecipCellSize.x = static_cast<f32>(KI_GRID_WIDTH)  / lGridExtent.x;
        lRecipCellSize.y = 0.0f;
        lRecipCellSize.z = static_cast<f32>(KI_GRID_HEIGHT) / lGridExtent.z;
        lRecipCellSize.w = 0.0f;

        Vector3 lCellSize;        // cellSize = extent / 100 (stored into mvGridUnitSize)
        lCellSize.x = lGridExtent.x / static_cast<f32>(KI_GRID_WIDTH);
        lCellSize.y = 0.0f;
        lCellSize.z = lGridExtent.z / static_cast<f32>(KI_GRID_HEIGHT);
        lCellSize.w = 0.0f;

        // 1) Histogram: assign every valid point to a grid cell and bump that cell's count.
        for (u32 luPoint = 0; luPoint < luPointCount; ++luPoint)
        {
            const Vector3& lPoint = lpPoints[luPoint];
            s32 liGridX = static_cast<s32>((lPoint.x - lGridMin.x) * lRecipCellSize.x);
            s32 liGridY = static_cast<s32>((lPoint.z - lGridMin.z) * lRecipCellSize.z);
            s32 liGridUnit = KI_GRID_WIDTH * liGridX + liGridY;

            CGS_ASSERT(liGridX < KI_GRID_WIDTH,  "liGridX < KI_GRID_WIDTH");
            CGS_ASSERT(liGridY < KI_GRID_HEIGHT, "liGridY < KI_GRID_HEIGHT");
            CGS_ASSERT(liGridUnit < (KI_GRID_WIDTH * KI_GRID_HEIGHT),
                       "liGridUnit < (KI_GRID_WIDTH*KI_GRID_HEIGHT)");

            lpuGridUnit[luPoint] = static_cast<u16>(liGridUnit);
            ++lpuGridUnitPointCounts[liGridUnit];
        }

        // 2) Prefix-sum the per-cell counts into first-indices, clearing the counts as we go
        // (they are re-used as running write cursors in the scatter pass below).
        u16 luRunning = 0;
        for (u32 luUnit = 0; luUnit < static_cast<u32>(KI_GRID_WIDTH * KI_GRID_HEIGHT); ++luUnit)
        {
            lpuGridUnitPointIndices[luUnit] = luRunning;
            u16 luCount = lpuGridUnitPointCounts[luUnit];
            lpuGridUnitPointCounts[luUnit] = 0;
            luRunning = static_cast<u16>(luRunning + luCount);
        }

        // 3) Scatter: write each valid point's index into its cell's slice of the map.
        memset(lpuGridPointMap, 0xFF, 2 * luPointCount);
        for (u32 luPoint = 0; luPoint < luPointCount; ++luPoint)
        {
            u32 luGridUnit    = lpuGridUnit[luPoint];
            u16 luFirstIndex  = lpuGridUnitPointIndices[luGridUnit];
            u16 luCurrentCount = lpuGridUnitPointCounts[luGridUnit];

            CGS_ASSERT(luGridUnit < (KI_GRID_WIDTH * KI_GRID_HEIGHT),
                       "luGridUnit < (KI_GRID_WIDTH*KI_GRID_HEIGHT)");
            CGS_ASSERT((luFirstIndex + luCurrentCount) < luPointCount,
                       "(luFirstIndex+luCurrentCount) < luNumSections");

            lpuGridPointMap[luFirstIndex + luCurrentCount] = static_cast<u16>(luPoint);
            ++lpuGridUnitPointCounts[luGridUnit];
        }

        // 4) Publish the header (the X360 vperm shuffles pack the X/Z lanes of the grid
        // factors into the 2D mvGridUnitSize / mvGridMin slots).
        lpMap->miGridWidth  = KI_GRID_WIDTH;
        lpMap->miGridHeight = KI_GRID_HEIGHT;
        lpMap->mvGridUnitSize.x = lCellSize.x;   // world-space cell SIZE (extent / 100)
        lpMap->mvGridUnitSize.y = lCellSize.z;
        lpMap->mvGridMin.x = lGridMin.x;
        lpMap->mvGridMin.y = lGridMin.z;
        lpMap->mpPoints                = lpPoints;
        lpMap->mpuSections             = lpuSections;
        lpMap->mpuGridUnitMap          = lpuGridPointMap;
        lpMap->mpuGridUnitPointIndices = lpuGridUnitPointIndices;
        lpMap->mpuGridUnitPointCount   = lpuGridUnitPointCounts;

        // Debug validation pass: walk every cell, derive that cell's world-space min/max
        // from mvGridMin + cell-index * cellSize, and assert each point bucketed into the
        // cell falls inside those bounds. Stores nothing into the map; pure consistency
        // check (the X360 only ever fires asserts in this loop). mvGridUnitSize already
        // holds the world-space cell size, so it is used directly (no reciprocal).
        const Vector2 lDebugCellSize = { lpMap->mvGridUnitSize.x, lpMap->mvGridUnitSize.y };
        for (s32 liGridX = 0; liGridX < lpMap->miGridWidth; ++liGridX)
        {
            if (lpMap->miGridHeight <= 0)
            {
                continue;
            }
            for (s32 liGridY = 0; liGridY < lpMap->miGridHeight; ++liGridY)
            {
                Vector2 lCellMin;
                lCellMin.x = lpMap->mvGridMin.x + static_cast<f32>(liGridX) * lDebugCellSize.x;
                lCellMin.y = lpMap->mvGridMin.y + static_cast<f32>(liGridY) * lDebugCellSize.y;
                Vector2 lCellMax;
                lCellMax.x = lCellMin.x + lDebugCellSize.x;
                lCellMax.y = lCellMin.y + lDebugCellSize.y;

                s32 liGridUnit = liGridX * lpMap->miGridWidth + liGridY;
                u16 luCellCount = lpMap->mpuGridUnitPointCount[liGridUnit];
                if (luCellCount == 0)
                {
                    continue;
                }

                u16 luCellFirst = lpMap->mpuGridUnitPointIndices[liGridUnit];
                for (u16 luSlot = 0; luSlot < luCellCount; ++luSlot)
                {
                    u16 luMappedPoint = lpMap->mpuGridUnitMap[luCellFirst + luSlot];
                    const Vector3& lPos = lpMap->mpPoints[luMappedPoint];
                    CGS_ASSERT((lPos.x >= lCellMin.x) && (lPos.x <= lCellMax.x),
                               "(lPos.X() >= lGridMin.X()) && (lPos.X() <= lGridMax.X())");
                    CGS_ASSERT((lPos.z >= lCellMin.y) && (lPos.z <= lCellMax.y),
                               "(lPos.Z() >= lGridMin.Y()) && (lPos.Z() <= lGridMax.Y())");
                }
            }
        }

        return lpMap;
    }

    // ------------------------------------------------------------------------------------
    // BrnAI::AISectionsData::FindNearestAISection @ 0x82676CC0  (Vector3, AISectionPointMap*)
    //
    // Returns the original section index of the section whose middle is closest to lPosition,
    // searched through the point map. The search grows outward in concentric square rings of
    // grid cells: it maps lPosition to its cell, then for an expanding half-extent it
    // clamps a [minX,maxX] x [minY,maxY] cell window to the grid, visits every cell in the
    // window, and over the points bucketed there keeps the one with the smallest squared
    // distance to lPosition. When a ring yields no hit, the window half-extent is grown by
    // the per-cell size and the search repeats; the first ring that finds a section returns
    // its index (mpuSections[best]).
    // ------------------------------------------------------------------------------------
    u16 AISectionsData::FindNearestAISection(Vector3 lPosition, AISectionPointMap* lpMap) const
    {
        const s32 liGridWidth  = lpMap->miGridWidth;
        const s32 liGridHeight = lpMap->miGridHeight;

        // Half-cell window seed: the X360 starts the search half-extent at
        // min(gridWidth-1, gridHeight-1) * 0.5 (vminfp of (W-1),(H-1) then vmulfp by
        // vcfsx(1,1) == 0.5), and grows it by that same min-extent each miss.
        // mvGridUnitSize holds the world-space cell SIZE; the X360 reciprocates it (vrefp +
        // Newton-Raphson) to recover cellInv = 1 / cellSize for the world->cell mapping.
        const f32 lfCellInvX = 1.0f / lpMap->mvGridUnitSize.x;   // 1 / cellSizeX
        const f32 lfCellInvY = 1.0f / lpMap->mvGridUnitSize.y;   // 1 / cellSizeY
        const f32 lfMinExtent = (static_cast<f32>(liGridWidth - 1) < static_cast<f32>(liGridHeight - 1))
                                    ? static_cast<f32>(liGridWidth - 1)
                                    : static_cast<f32>(liGridHeight - 1);
        f32 lfHalfExtent = lfMinExtent * 0.5f;

        // Grid-space upper clamps (gridWidth-1, gridHeight-1) used when bounding the window.
        const f32 lfMaxGridX = static_cast<f32>(liGridWidth - 1);
        const f32 lfMaxGridY = static_cast<f32>(liGridHeight - 1);

        s32 liBest = -1;

        while (true)
        {
            // Window in grid cells around lPosition. The X360 applies the half-extent in
            // WORLD space (pos -/+ half) BEFORE subtracting gridMin and scaling by cellInv:
            //   min corner: floor((pos - half - gridMin) * cellInv), then max(.,0)   [vrfim+vmaxfp]
            //   max corner: ceil ((pos + half - gridMin) * cellInv), then min(.,gridDim-1)
            // (half-extent is the same value compared as a squared distance below, so it is
            // intentionally treated as a world-space radius here, bug-for-bug.)
            f32 lfMinXf = floorf((lPosition.x - lfHalfExtent - lpMap->mvGridMin.x) * lfCellInvX);
            f32 lfMinYf = floorf((lPosition.z - lfHalfExtent - lpMap->mvGridMin.y) * lfCellInvY);
            lfMinXf = lfMinXf > 0.0f ? lfMinXf : 0.0f;
            lfMinYf = lfMinYf > 0.0f ? lfMinYf : 0.0f;

            f32 lfMaxXf = ceilf((lPosition.x + lfHalfExtent - lpMap->mvGridMin.x) * lfCellInvX);
            f32 lfMaxYf = ceilf((lPosition.z + lfHalfExtent - lpMap->mvGridMin.y) * lfCellInvY);
            lfMaxXf = lfMaxXf < lfMaxGridX ? lfMaxXf : lfMaxGridX;
            lfMaxYf = lfMaxYf < lfMaxGridY ? lfMaxYf : lfMaxGridY;

            const u16 luMinX = static_cast<u16>(static_cast<s32>(lfMinXf));
            const u16 luMinY = static_cast<u16>(static_cast<s32>(lfMinYf));
            const u16 luMaxX = static_cast<u16>(static_cast<s32>(lfMaxXf));
            const u16 luMaxY = static_cast<u16>(static_cast<s32>(lfMaxYf));

            // Squared-distance accumulator seeds at halfExtent^2 (only points strictly
            // nearer than the current best replace it).
            f32 lfBestDistSq = lfHalfExtent * lfHalfExtent;

            if (luMinX <= luMaxX)
            {
                for (u16 luCellX = luMinX; luCellX <= luMaxX; ++luCellX)
                {
                    if (luMinY > luMaxY)
                    {
                        continue;
                    }
                    for (u16 luCellY = luMinY; luCellY <= luMaxY; ++luCellY)
                    {
                        s32 liGridUnit = (luCellX * liGridWidth + luCellY) & 0xFFFF;
                        u16 luCellCount = lpMap->mpuGridUnitPointCount[liGridUnit];
                        u16 luCellFirst = lpMap->mpuGridUnitPointIndices[liGridUnit];

                        if (luCellCount == 0)
                        {
                            continue;
                        }

                        for (u16 luSlot = 0; luSlot < luCellCount; ++luSlot)
                        {
                            u16 luMappedPoint = lpMap->mpuGridUnitMap[luCellFirst + luSlot];
                            const Vector3& lPoint = lpMap->mpPoints[luMappedPoint];

                            f32 lfDx = lPoint.x - lPosition.x;
                            f32 lfDy = lPoint.y - lPosition.y;
                            f32 lfDz = lPoint.z - lPosition.z;
                            f32 lfDistSq = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;

                            if (lfDistSq < lfBestDistSq)
                            {
                                liBest = luMappedPoint;
                                lfBestDistSq = lfDistSq;
                            }
                        }
                    }
                }
            }

            if (static_cast<u16>(liBest) != 0xFFFF)
            {
                break;
            }

            // No section found this ring -- grow the window and retry.
            lfHalfExtent += lfMinExtent;
        }

        return lpMap->mpuSections[liBest & 0xFFFF];
    }

    // ------------------------------------------------------------------------------
    // AISectionsData::FixUp @0x8267DA28 / FixDown @0x8267DAA0 -- load-time pointer
    // relocation of the AI lane graph.
    //
    // Reached from AISectionsResourceType::FixUp @0x8267DB28
    // (`mr r3,r4; mr r4,r5; b ...`), which forwards the rw::Resource; the body's first
    // instruction is `lwz r29, 0(r4)`, i.e. it reads the resource's segment-0 base out of
    // it. The forwarder here resolves that to the block pointer and passes it directly, so
    // nothing narrows the 64-bit host base (the console's 32-bit `int` delta was bug class
    // (a) waiting to happen once the slots were widened).
    //
    // Order (the asm's): the section table first, then every AISection, then the reset
    // pairs. FixDown mirrors it, walking the sections BEFORE un-rebasing the table that
    // holds them. Both slots are NULL-GUARDED on this path -- unlike the Traffic graph.
    //
    // NOTE the console does NOT check muVersion here (KU_AI_SECTIONS_DATA_VERSION == 12),
    // unlike TrafficData::FixUp's version assert. Left unchecked, faithfully.
    // ------------------------------------------------------------------------------
    void AISectionsData::FixUp(const void* lpBaseData)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);

        if (mpaSections != nullptr)
        {
            mpaSections = reinterpret_cast<AISection*>(
                reinterpret_cast<uintptr_t>(mpaSections) + luBase);

            for (u32 luSection = 0; luSection < muNumSections; ++luSection)
            {
                mpaSections[luSection].FixUp(lpBaseData);
            }
        }

        if (mpaSectionResetPairs != nullptr)
        {
            mpaSectionResetPairs = reinterpret_cast<SectionResetPair*>(
                reinterpret_cast<uintptr_t>(mpaSectionResetPairs) + luBase);
        }
    }

    void AISectionsData::FixDown(const void* lpBaseData)
    {
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);

        if (mpaSections != nullptr)
        {
            for (u32 luSection = 0; luSection < muNumSections; ++luSection)
            {
                mpaSections[luSection].FixDown(lpBaseData);
            }

            mpaSections = reinterpret_cast<AISection*>(
                reinterpret_cast<uintptr_t>(mpaSections) - luBase);
        }

        if (mpaSectionResetPairs != nullptr)
        {
            mpaSectionResetPairs = reinterpret_cast<SectionResetPair*>(
                reinterpret_cast<uintptr_t>(mpaSectionResetPairs) - luBase);
        }
    }
}
