#ifndef BRN_CHECKPOINT_DATA_H
#define BRN_CHECKPOINT_DATA_H

#include "types.hpp"
#include "GameSource/GameState/BrnGameStateTypes.h"     // BrnGameState::LandmarkIndex (s16 wrapper)
#include "GameShared/GameClasses/Containers/CgsArray.h" // Array<u32,8>

namespace BrnGameState
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF BrnGameModeParams.h:62 (:92-95). 44-byte stride
// (16*44 == 704 == owner+704 array-count offset; AddCheckpoint writes district@+4 / block-count@+40).
class CheckpointData
{
public:
    // X360 0x8231C2C8 -- the WHOLE console body, verbatim:
    //     0x8231C2C8  li  r11, -1
    //     0x8231C2CC  stw r11, 0x28(r3)
    //     0x8231C2D0  blr
    // One store, to +0x28 (40) == mauBlockSectionIds' trailing count word (the array is u32[8] at
    // +0x08..+0x27 with its count at +0x28, per this class's own 44-byte stride note above). -1 is
    // Array<T,N>::KI_UNCONSTRUCTED, so a default-constructed CheckpointData is deliberately left
    // in the pre-Construct state: the block-section list is NOT empty-but-usable, and the first
    // access fires "Array used before Construct/Clear was called". CheckpointData::Construct()
    // below is what flips it to a live empty list. The three scalars at +0x00/+0x02/+0x04 are
    // NOT touched by the console ctor and are left indeterminate here for the same reason.
    //
    // HEADER-INLINE, deliberately: Array<CheckpointData,16> default-constructs sixteen of these,
    // and that array is an embedded member of BOTH GameModeParams and StartGameModeParams, so the
    // definition has to be visible in every TU that declares either. There is no
    // BrnCheckpointData.cpp today; a tree-wide grep found no other definition of this ctor
    // anywhere, mounted or not. Same treatment and same reasoning as the [evt-flow E1] inlines in
    // BrnGameActions.h -- fold it back out-of-line if a real BrnCheckpointData.cpp ever lands.
    CheckpointData()
    {
        mauBlockSectionIds.MarkUnconstructed();   // stw -1, 0x28(this)
    }

    void Construct(LandmarkIndex luLandmarkIndex, u16 luAISectionIndex)   // inlined into AddCheckpoint
    {
        muLandmarkIndex  = luLandmarkIndex;
        muAISectionIndex = luAISectionIndex;
        meDistrict       = 18;              // BrnWorld::E_DISTRICT_INVALID (BrnWorldRegion.h:36)
        mauBlockSectionIds.Construct();     // X360: trailing count word -> 0
    }

    void                  AddBlockSectionId(u32 luBlockSectionId);   // declared-only
    void                  SetDistrict(s32 leDistrict);               // declared-only (real arg BrnWorld::EDistrict)
    LandmarkIndex         GetLandmarkIndex() const;                  // declared-only
    u16                   GetAISectionIndex() const;                 // declared-only
    s32                   GetDistrict() const;                       // declared-only
    const Array<u32, 8u>* GetBlockSectionIds() const;               // declared-only

private:
    LandmarkIndex  muLandmarkIndex;     // 0x00 (s16) -- DWARF :92
    u16            muAISectionIndex;    // 0x02       -- DWARF :93
    s32            meDistrict;          // 0x04       -- DWARF :94 (real BrnWorld::EDistrict; raw s32 to avoid forking)
    Array<u32, 8u> mauBlockSectionIds; // 0x08..0x2B -- DWARF :95 (u32[8]+count==36; count @ +40)
};
}

#endif
