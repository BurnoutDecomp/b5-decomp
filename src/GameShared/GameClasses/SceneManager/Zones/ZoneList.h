#pragma once

// ZoneList.h — canonical home for CgsSceneManager::ZoneList: the scene manager's flat,
// serialisable container of convex Zones plus the shared point pool and the per-zone
// point-span tables. It is loaded as a relocatable resource blob, so it carries FixUp /
// FixDown helpers that (de)relocate its internal pointers against the load base.
//
// SOURCES: the DecFIGS DWARF ZoneList.h supplies the member set + accessor signatures;
// the X360 ARTIST asm/pseudocode is authority for the FixUp/FixDown relocation logic.
//
//   ZoneList::FixUp   @ 0x828D05B8  -- serialise-IN: add the base to the four stored
//                                      pointers, then fix up every embedded Zone.
//   ZoneList::FixDown @ 0x828D0640  -- serialise-OUT: fix down every embedded Zone (while
//                                      the array pointer is still absolute), then subtract
//                                      the base from the four stored pointers.
//
// Layout (DWARF ZoneList.h; matches the asm displacements):
//   +0x0  mpPoints           (Vector2*)   +0x4  mpZones            (Zone*)
//   +0x8  mpuZonePointStarts (u32*)       +0xC  mpiZonePointCounts (s16*)
//   +0x10 muTotalZones       (u32)        +0x14 muTotalPoints      (u32)

#include "types.hpp"
#include "rw/math/vpu/types.h"                                // rw::math::vpu::Vector2
#include "GameShared/GameClasses/SceneManager/Zones/Zone.h"   // CgsSceneManager::Zone

namespace CgsSceneManager
{
    class ZoneList
    {
    public:
        // DECLARE-ONLY: bodies live in other TUs.
        void Construct();

        // @ 0x828D05B8 / 0x828D0640. Relocate the list's internal pointers against the
        // load base (FixUp = base-relative -> absolute; FixDown = absolute -> base-relative),
        // fixing up each embedded Zone in turn.
        void FixUp(void* lpBaseAddress);
        void FixDown(void* lpBaseAddress);

        // DECLARE-ONLY: bodies live in other TUs.
        void SetPoints(rw::math::vpu::Vector2* lpPoints, u32 luTotalPoints);
        void SetZones(Zone* lpZones, u32* lpuZonePointStarts, s16* lpiZonePointCounts,
                      u32 luTotalZones);

        rw::math::vpu::Vector2* GetPoints()           { return mpPoints; }
        Zone*                   GetZones()            { return mpZones; }
        u32*                    GetZonePointStarts()  { return mpuZonePointStarts; }
        s16*                    GetZonePointCounts()  { return mpiZonePointCounts; }

        const rw::math::vpu::Vector2* GetPoints()          const { return mpPoints; }
        const Zone*                   GetZones()           const { return mpZones; }
        const u32*                    GetZonePointStarts() const { return mpuZonePointStarts; }
        const s16*                    GetZonePointCounts() const { return mpiZonePointCounts; }

        u32 GetTotalZones()  const { return muTotalZones; }
        u32 GetTotalPoints() const { return muTotalPoints; }

        // DECLARE-ONLY: point-in-zone queries (own TUs).
        const Zone* GetFirstZoneForPoint(rw::math::vpu::Vector2 lPoint, s32 liStartZone) const;
        const Zone* GetNextZoneForPoint(const Zone* lpCurrentZone,
                                        rw::math::vpu::Vector2 lPoint) const;

    private:
        rw::math::vpu::Vector2* mpPoints;            // +0x0
        Zone*                   mpZones;             // +0x4
        u32*                    mpuZonePointStarts;  // +0x8
        s16*                    mpiZonePointCounts;  // +0xC
        u32                     muTotalZones;        // +0x10
        u32                     muTotalPoints;       // +0x14
    };

} // namespace CgsSceneManager
