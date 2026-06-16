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
    CheckpointData();   // X360 0x8231C2C8: block-section list left unconstructed (+40 == -1 sentinel)

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
