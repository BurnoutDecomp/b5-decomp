#ifndef BRN_GAMESTATE_OPPONENT_DATA_H
#define BRN_GAMESTATE_OPPONENT_DATA_H
// [!] Guard renamed 2026-09-02: the old BRN_OPPONENT_DATA_H collided with
// SharedClasses/Progression/BrnOpponentData.h (CarOpponent / CarOpponentSet), so whichever of
// the two was included first silently hid the other.

#include "types.hpp"
#include "BrnCommonTypes.h"                               // CgsID
#include "SharedClasses/Progression/BrnRaceEventData.h"   // BrnProgression::EventStartGridSlot / EventRacerPersonality
#include "SharedClasses/Progression/BrnRaceBalance.h"     // BrnProgression::OpponentBalanceData

#include <cstddef>   // offsetof (the _AssertLayout pins)

namespace BrnGameState
{
// =============================================================================
// BrnGameState::OpponentData -- the per-rival event record stored by value in
// GameModeParams::maOpponentData (Array<OpponentData,7>). DWARF home is
// BrnGameModeParams.h:113 (members :134-:137, methods :119-:131); it keeps this sibling
// header as its single owner because the Array<OpponentData,7> instantiation TU
// (SharedIO/Array_OpponentData_7.cpp) already includes it here.
//
// [rival-spawn wave R, 2026-09-02] THE PAD IS GONE. The three sub-records the earlier slice
// folded into `maPad[104]` all have real owners now (EventStartGridSlot / EventRacerPersonality
// in BrnRaceEventData.h, OpponentBalanceData in BrnRaceBalance.h), and the record is
// pointer-free, so the host layout is the console's byte for byte:
//     +0x00 CgsID                 mCarModelId      (8)
//     +0x08 EventStartGridSlot    mStartGridSlot   (20)
//     +0x1C OpponentBalanceData   mRaceBalanceData (68)
//     +0x60 EventRacerPersonality mPersonality     (16)   -> sizeof 112 (0x70)
// Every one of those offsets is asm-attested by the two consumers:
//   * ModeManager::SetupOpponentData @0x82329348 builds the record on the stack at sp+0x70:
//     `std r29, 0x70(sp)` (the id), a 5-dword copy to sp+0x78 (the slot), `memcpy(sp+0x8C, ..,
//     0x44)` (the balance graph) and four stores to sp+0xD0 (the personality), then
//     Array<OpponentData,7>::Append @0x82317D90 on params+0x528 -- stride 112.
//   * RaceCarEntityModule::SetUpAIForMode @0x82301620 reads `ld r31, 0(r28)` (GetCarModelId)
//     and `lbz r11, 0x19(r28)` (GetStartGridSlot()->GetFlag(E_FLAG_CAN_DEVIATE_FROM_ROUTE):
//     0x08 + EventStartGridSlot::muFlags @0x11).
//
// METHODS: the DWARF's five. The X360 emits NO standalone symbol for any of them --
// SetupOpponentData open-codes Construct field by field and SetUpAIForMode open-codes the two
// reads -- so they are header-inline over the named members (the same precedent as
// EventStartGridSlot's accessors).
// =============================================================================
class OpponentData
{
public:
    // DWARF :119. The four-part copy SetupOpponentData performs at 0x82329674..0x823296D0.
    void Construct(CgsID lCarModelId,
                   const BrnProgression::EventStartGridSlot*    lpStartGridSlot,
                   const BrnProgression::OpponentBalanceData*   lpRaceBalanceData,
                   const BrnProgression::EventRacerPersonality* lpPersonality)
    {
        mCarModelId      = lCarModelId;
        mStartGridSlot   = *lpStartGridSlot;
        mRaceBalanceData = *lpRaceBalanceData;
        mPersonality     = *lpPersonality;
    }

    CgsID                                        GetCarModelId() const      { return mCarModelId; }       // :122
    const BrnProgression::EventStartGridSlot*    GetStartGridSlot() const   { return &mStartGridSlot; }   // :125
    const BrnProgression::OpponentBalanceData*   GetRaceBalanceData() const { return &mRaceBalanceData; } // :128
    const BrnProgression::EventRacerPersonality* GetPersonality() const     { return &mPersonality; }     // :131

    // Layout pins (offsetof on private members is a C2248 outside the class, hence the
    // in-class static function, the Apt-audit _AssertLayout precedent). The 112-byte stride is
    // what Array<OpponentData,7>::Append @0x82317D90 / ::GetItem @0x822AE310 both index with,
    // and the four offsets are the four stack offsets SetupOpponentData assembles the record at.
    static void _AssertLayout()
    {
        static_assert(sizeof(OpponentData) == 112, "BrnGameState::OpponentData is a 112-byte record");
        static_assert(offsetof(OpponentData, mCarModelId)      == 0x00, "OpponentData::mCarModelId @0x00");
        static_assert(offsetof(OpponentData, mStartGridSlot)   == 0x08, "OpponentData::mStartGridSlot @0x08");
        static_assert(offsetof(OpponentData, mRaceBalanceData) == 0x1C, "OpponentData::mRaceBalanceData @0x1C");
        static_assert(offsetof(OpponentData, mPersonality)     == 0x60, "OpponentData::mPersonality @0x60");
    }

private:
    CgsID                                 mCarModelId;       // 0x00 (DWARF :134)
    BrnProgression::EventStartGridSlot    mStartGridSlot;    // 0x08 (DWARF :135)
    BrnProgression::OpponentBalanceData   mRaceBalanceData;  // 0x1C (DWARF :136)
    BrnProgression::EventRacerPersonality mPersonality;      // 0x60 (DWARF :137)
};

// The stride pin that needs no member access (the member offsets are pinned in _AssertLayout).
static_assert(sizeof(OpponentData) == 112, "BrnGameState::OpponentData is a 112-byte record");
}

#endif
