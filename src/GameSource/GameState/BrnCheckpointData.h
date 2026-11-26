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
    // X360 0x8231C2C8 is the IMPLICIT default constructor: DWARF BrnGameModeParams.h:62 declares
    // no constructor, and the console body (li r11,-1; stw r11,0x28(r3); blr) is exactly the
    // member Array<u32,8>() default constructor (count word -> KI_UNCONSTRUCTED). A user-declared
    // constructor that also called MarkUnconstructed() stored -1 twice.

    void Construct(LandmarkIndex luLandmarkIndex, u16 luAISectionIndex)   // inlined into AddCheckpoint
    {
        muLandmarkIndex  = luLandmarkIndex;
        muAISectionIndex = luAISectionIndex;
        mauBlockSectionIds.Construct();     // X360: trailing count word -> 0 (before the district,
                                            // AddCheckpoint 0x8236AAE4 / 0x8236AAE8)
        meDistrict       = 18;              // BrnWorld::E_DISTRICT_INVALID (BrnWorldRegion.h:36)
    }

    void                  AddBlockSectionId(u32 luBlockSectionId) { mauBlockSectionIds.Append(luBlockSectionId); } // ARTIST0x82329314
    void                  SetDistrict(s32 leDistrict) { meDistrict = leDistrict; } // ARTIST0x82329870
    LandmarkIndex         GetLandmarkIndex() const { return muLandmarkIndex; } // ARTIST0x823EAF9C
    // [stuntrace start-grid wave, 2026-08-27] BODIED, header-inline, for the same reason the
    // ctor and Construct above are: there is no BrnCheckpointData.cpp on this tree
    // (grep-verified), so a declared-only accessor is an LNK2019 for its first caller. That
    // caller is RaceCarEntityModule::SetupOpponents @0x82307DF0, where the console emits the
    // read INLINE as `lhz r31, 2(r3)` immediately after Array<CheckpointData,16>::Ge(0)
    // (asm 0x82307EAC..0x82307EB0) -- one halfword off +0x02, which is exactly this member.
    // Fold it back out-of-line if a real BrnCheckpointData.cpp ever lands.
    u16                   GetAISectionIndex() const { return muAISectionIndex; }
    s32                   GetDistrict() const { return meDistrict; } // ARTIST0x823EAFB8
    // DWARF BrnGameModeParams.h; the PS3 build emits it out of line (BrnWorldAIUnity.cpp). ARTIST
    // inlines it at its one reader, AIModule::OnModeStart: 0x82791F00 `addi r4, cp, 8` -- the
    // address of mauBlockSectionIds (+0x08). Bodied 2026-09-23 (crash parity G04-D1).
    const CgsContainers::Array<u32, 8u>* GetBlockSectionIds() const { return &mauBlockSectionIds; }

private:
    LandmarkIndex  muLandmarkIndex;     // 0x00 (s16) -- DWARF :92
    u16            muAISectionIndex;    // 0x02       -- DWARF :93
    s32            meDistrict;          // 0x04       -- DWARF :94 (real BrnWorld::EDistrict; raw s32 to avoid forking)
    CgsContainers::Array<u32, 8u> mauBlockSectionIds; // 0x08..0x2B -- DWARF :95 (u32[8]+count==36; count @ +40)
};
}

#endif
