#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SharedClasses/Physics/Props/PropRespawnTypesEnum.h"   // eRespawnType (committed home, see below)

// BrnPhysics::Props::PropZoneData and its cell descriptors.
// Reconstructed from BURNOUT_X360_ARTIST.XEX with member names/types from the DecFIGS DWARF
// (BrnPhysicsPropZoneData.h). A prop zone owns a flat array of prop instances partitioned into
// a small grid of cells (each cell knows its prop range and how many respawn differently / do
// not respawn). PropCellId is the (x,z) grid coordinate; GetCellId maps a world XZ position to
// the owning cell, GetRespawnTypeForProp classifies a prop's respawn behaviour by walking cells.
//
// World grid constants (X360 GetCellId asm immediates): the world spans [-5000, +8000] in X
// (half-extent 5000 + 3000 extension) and [-5000, +5000] in Z (half-extent 5000); cells are
// 100 units square -- cellX = (s16)(lfX + 5000) / 100, cellZ = (s16)(lfZ + 5000) / 100.
namespace BrnPhysics
{
namespace Props
{
    // Forward decl: the per-instance prop record. PropZoneData only stores a pointer to its
    // instance array; the two TU functions here never dereference it, so the full layout
    // (its own DWARF home) is not needed.
    struct PropInstanceData;

    static const int KI_WORLD_X_HALF_EXTENT = 5000;
    static const int KI_WORLD_X_EXTENSION   = 3000;
    static const int KI_WORLD_Z_HALF_EXTENT = 5000;
    static const int KI_CELL_SIZE           = 100;

    // BrnPhysicsPropZoneData.h:60 -- packed (x,z) grid coordinate of a prop cell.
    struct PropCellId
    {
        static const u16 KU_INVALID_CELL_ID = 65535;

        u16 GetX() const { return muX; }
        u16 GetZ() const { return muZ; }

    public:
        u16 muX;
        u16 muZ;
    };

    // BrnPhysicsPropZoneData.h:107 -- one grid cell: its id plus the [start, count) range into
    // the zone's instance array and the two respawn sub-counts at the front of that range.
    struct PropCellData
    {
        s32 GetCount() const { return muCount; }
        s32 GetNumberOfRespawnDifferent() const { return muNumberOfRespawnDifferent; }
        s32 GetNumberOfDontRespawn() const { return muNumberOfDontRespawn; }

    private:
        PropCellId mId;
        u16        muStartIndex;
        u16        muCount;
        u16        muNumberOfRespawnDifferent;
        u16        muNumberOfDontRespawn;
    };

    // BrnPhysicsPropZoneData.h:156 -- the streamed prop-zone record.
    struct PropZoneData
    {
    public:
        // BrnPhysicsPropZoneData.h:211 -- classify a prop (by its index into the zone's instance
        // array) into its respawn behaviour. X360 @ 0x822BB1F0: walks cells; within each cell the
        // index falls into [respawn-different | dont-respawn | normal] sub-ranges.
        eRespawnType GetRespawnTypeForProp(s32 liPropIndex) const;

        // BrnPhysicsPropZoneData.h:216 -- map a world XZ position to its owning grid cell.
        // X360 @ 0x822BB2D0: range-asserts X/Z then computes (cellX, cellZ).
        PropCellId GetCellId(f32 lfX, f32 lfZ);

    private:
        PropCellData* maCells;
        u8            muNumCells;
        PropInstanceData* maInstances;
        u32           muSizeInBytes;
        u32           muNumberOfInstances;
        u32           muNumberOfProps;
        u16           muZoneId;
    };
}
}
