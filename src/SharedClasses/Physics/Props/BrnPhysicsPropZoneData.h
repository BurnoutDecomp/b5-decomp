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
namespace CgsDev { struct StrStreamBase; }

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

        // ADDITIVE GROW (PropCellManager group, DWARF BrnPhysicsPropZoneData.h:64/69/78/81/90).
        // Pure addition: no member/layout/sizeof change.
        //
        // ⚠️ TWO-u16 CONVENTION (X360-authoritative). The shipped build NEVER treats a cell id
        // as one 32-bit word: PropCellManager::IsCellLoaded @0x822A4130 loads +0 and +2 with
        // separate `lhz` and compares each half, and PropCellManager::AddCells @0x822A9F7C /
        // 0x822A9F90 stores the two halves with separate `sth`. Comparing member-wise (below)
        // therefore matches the binary exactly and is endianness-independent -- a u32 view
        // would read (X<<16)|Z on the big-endian console but (Z<<16)|X on our little-endian
        // x64 host. Never fold these two fields into a u32.
        void Construct()                    { muX = KU_INVALID_CELL_ID; muZ = KU_INVALID_CELL_ID; }
        void Construct(u16 luX, u16 luZ)    { muX = luX; muZ = luZ; }
        bool operator==(const PropCellId& lrOther) const
        {
            return (muX == lrOther.muX) && (muZ == lrOther.muZ);
        }
        bool operator!=(const PropCellId& lrOther) const { return !(*this == lrOther); }
        bool IsValid() const
        {
            return (muX != KU_INVALID_CELL_ID) && (muZ != KU_INVALID_CELL_ID);
        }

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

        // ADDITIVE GROW (PropCellManager::AddCells group, DWARF :119/:125). Both are
        // header inlines the X360 folds into AddCells @0x822A9E90 (`lhz 4(cell)` and
        // `lhz 0(cell)`/`lhz 2(cell)`). No layout change.
        s32 GetStartIndex() const { return muStartIndex; }
        const PropCellId& GetId() const { return mId; }

    private:
        PropCellId mId;
        u16        muStartIndex;
        u16        muCount;
        u16        muNumberOfRespawnDifferent;
        u16        muNumberOfDontRespawn;
    };

    // BrnPhysicsPropZoneData.h:156 -- the streamed prop-zone record.
    //
    // SERIALISED (platform-4) FORM: the host layout below IS the on-disk x64 form
    // (porter: tools/assets/bundles/world_type_transcode.py transcode_propinstancedata):
    // 0x28 header {maCells u64 @0, muNumCells @8, maInstances u64 @0x10, muSizeInBytes
    // @0x18, muNumberOfInstances @0x1C, muNumberOfProps @0x20, muZoneId @0x24}, the
    // 80-byte pointer-free instance records 16-aligned at +0x30, then the 12-byte
    // cells and the reserved zero tail. X360 header was 0x1C {u32 maCells @0, muNumCells
    // @4, u32 maInstances @8, muSizeInBytes @0xC, counts @0x10/0x14, muZoneId @0x18}.
    struct PropZoneData
    {
        // The resource-type handler reads muSizeInBytes and relocates maCells /
        // maInstances during FixUp; grant it friendship rather than exposing the
        // private data (the StaticSoundMap precedent).
        friend class PropInstanceDataResourceType;

    public:
        // BrnPhysicsPropZoneData.h:211 -- classify a prop (by its index into the zone's instance
        // array) into its respawn behaviour. X360 @ 0x822BB1F0: walks cells; within each cell the
        // index falls into [respawn-different | dont-respawn | normal] sub-ranges.
        eRespawnType GetRespawnTypeForProp(s32 liPropIndex) const;

        // BrnPhysicsPropZoneData.h:216 -- map a world XZ position to its owning grid cell.
        // X360 @ 0x822BB2D0: range-asserts X/Z then computes (cellX, cellZ).
        // CONST-QUALIFIED 2026-08-12 (prop-spawn wave): the DWARF renders this without
        // a trailing const, but PropZoneManager::LoadProp -- whose own DWARF signature
        // takes `const PropZoneData *` -- calls it on that const pointer, so the shipped
        // declaration must be const (the body is pure: four range tripwires and two
        // divisions, no member touched). Widening only; every existing caller still binds.
        PropCellId GetCellId(f32 lfX, f32 lfZ) const;

        // ADDITIVE GROW (PropZoneManager::LoadZone group): the three trivial header-inline
        // getters LoadZone (0x822FC168) reads off the streamed record. The X360 LoadZone
        // body loads them directly (lhz 0x18 == muZoneId, lwz 0x14 == muNumberOfProps,
        // lwz 0x10 == muNumberOfInstances) -- they are inline accessors folded at the call
        // site. Pure addition (no layout/sizeof change; the members already exist).
        u16 GetZoneId() const             { return muZoneId; }
        u32 GetNumberOfInstances() const  { return muNumberOfInstances; }
        u32 GetNumberOfProps() const      { return muNumberOfProps; }

        // ADDITIVE GROW (PropCellManager::AddCells group, DWARF :227/:231). Header
        // inlines the X360 folds into AddCells @0x822A9E90 (`lbz 4(zone)` == muNumCells,
        // `lwz 0(zone)` + 12*i == &maCells[i]). No layout change.
        s32 GetNumCells() const { return muNumCells; }
        const PropCellData* GetCellData(s32 liCellIndex) const { return &maCells[liCellIndex]; }

        // ADDITIVE GROW (PropZoneManager::LoadProp group). LoadProp @0x822F2EF0 addresses
        // the prop's serialised record as `maInstances + 80 * liZoneDataPropIndex`
        // (`lwz r10,8(zone)` == maInstances, `slwi/add/slwi r11,...,4` == *80). Returning
        // the ARRAY BASE (rather than an element reference) keeps this header free of a
        // dependency on the complete PropInstanceData layout -- callers that dereference
        // include BrnPhysicsPropInstanceData.h themselves and index BY NAME, so the
        // 80-byte stride comes from sizeof(PropInstanceData), never a transcribed constant.
        // Pure addition (no layout/sizeof change).
        const PropInstanceData* GetInstances() const { return maInstances; }

    private:
        PropCellData* maCells;
        u8            muNumCells;
        PropInstanceData* maInstances;
        u32           muSizeInBytes;
        u32           muNumberOfInstances;
        u32           muNumberOfProps;
        u16           muZoneId;
    };

    // ADDITIVE GROW (PropCellManager group, DWARF BrnPhysicsPropZoneData.h:793).
    // X360 @0x822A3FD0 -- stream a cell id into the debug log; called by
    // PropCellManager::ActivateCell / DeactivateCell's "Activating cell: " /
    // "Deactivating cell: " trace lines. Declaration only; the body belongs to this
    // header's own TU.
    CgsDev::StrStreamBase& operator<<(CgsDev::StrStreamBase& lrStream, PropCellId lCellId);
}
}
