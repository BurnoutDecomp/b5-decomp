// ---------------------------------------------------------------------------
// GameSource/GameState/RoadRules/BrnRoadRulesManager.cpp
// ---------------------------------------------------------------------------
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"  // BrnGameState::StreetManager (single home for GetStreetData)
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "SharedClasses/StreetData/BrnStreetData.h"       // BrnStreetData::StreetData / Road

// BrnGameState::StreetManager (incl. its GetStreetData() accessor) now lives in its single
// canonical home GameSource/GameState/StreetData/BrnGameStateStreetManager.h (included above).
// The former file-local `struct StreetManager` shim was removed from this TU: it ODR-collided
// with the new header once a second TU (the road-rules debug component) needed the type.

// extern const CgsID K_INVALID_ID == 0 (X360 'return 0' on no-current-road).
const CgsID BrnGameState::RoadRulesManager::K_INVALID_ID = 0;

// 0x82327438
// DWARF: CgsID GetCurrentRoadID() const;  (the Hex-Rays 'int' return + 4-byte
// read of *(Road+16) is a decompiler truncation of the 8-byte CgsID load).
CgsID BrnGameState::RoadRulesManager::GetCurrentRoadID() const
{
    // *(this+32) == -1  ->  no current road.
    if ( miLastRoadIndex == -1 )
    {
        return K_INVALID_ID;
    }

    // *(this+20): asserted present (baked "mpStreetManager").
    CGS_ASSERT( mpStreetManager, "mpStreetManager" );

    // ResourcePtr<StreetData>::operator-> wrapped by StreetManager::GetStreetData().
    const BrnStreetData::StreetData* lpStreetData = mpStreetManager->GetStreetData();
    const BrnStreetData::Road*       lpRoad       = lpStreetData->GetRoad( miLastRoadIndex );

    // baked "lpRoad".
    CGS_ASSERT( lpRoad, "lpRoad" );

    // X360 reads *(Road+16) == Road::mId; the inlined Road::GetId().
    return lpRoad->GetId();
}

// ----------------------------------------------------------------------------
// IsRoadLimitRegionValid @ 0x82335268
//
// ⭐ [gateui] REAL (2026-08-20, round 3). Was declaration-only, and it is one of the two
// RoadRulesManager symbols that gate the BrnTriggerQueryManager.cpp mount
// (build_game_exe.bat:2369-2375's list) -- its only caller in the image is
// TriggerQueryManager::UpdateTriggers @0x82391FD8, which asserts on the answer once per track:
// "Road limit region <id>(<limit>) is broken\nDid you build triggers and forget to build RoadRules?"
//
// Answers "does any Road in the loaded StreetData claim this road-limit id?" -- a linear scan of
// StreetData::mpaRoads comparing lLimitId against each Road's TWO road-limit ids, returning true
// on the first hit.
//
// ⚠️ lRegionId IS GENUINELY UNUSED, and that is the console's shape, not a dropped read. The asm
// saves only r3 (this) and r5 (the limit id) -- `0x82335274 mr r28, r3` / `0x82335278 mr r27, r5`
// -- and r4 is never moved, spilled or read anywhere in the body. It exists so the CALLER's assert
// message can name both ids. Kept in the signature (the DWARF/mangled shape is
// `bool (CgsID, CgsID) const`) and explicitly voided.
//
// Reads, all off the asm rather than the pseudocode (Hex-Rays renders the 8-byte big-endian CgsID
// loads as 4-byte reads at +28/+36 -- they are `ld` at +0x18/+0x20):
//   0x82335280  lwz r11, 0x14(this)        ; mpStreetManager
//   0x82335284  addi r3, r11, 0x1CC8       ; +7368 == the ResourcePtr<StreetData>, i.e.
//   0x82335288  bl  StreetData_::oper      ;   StreetManager::GetStreetData()
//   0x8233528C  lwz r11, 0x20(r3)          ; StreetData::miRoadCount   -> GetRoadCount()
//   0x823352E8  lwz r11, 0x10(r30)         ; StreetData::mpaRoads      \ the inlined GetRoad(i),
//   0x823352EC  add r11, r11, r29          ;   + i*0x40 (road stride)  / whose bounds assert
//   0x823352D4  li  r5, 0x26D              ;   BrnStreetData.h:621 fires here -- reproduced by
//                                          ;   calling the tree's own inline GetRoad()
//   0x823352F0  ld  r10, 0x18(r11)         ; Road::miRoadLimitId0 -> GetRoadLimitId0()
//   0x823352FC  ld  r11, 0x20(r11)         ; Road::miRoadLimitId1 -> GetRoadLimitId1()
//   0x823352F4/0x82335300  cmpld           ; 64-bit compares against the limit id
// ⓘ This is the FIRST attested call site for Road::GetRoadLimitId1() -- BrnStreetData.h:324 notes
// it had none and was bodied by symmetry. The `ld r11, 0x20` above attests it now (that header is
// outside this owner's lane, so the note is left for its owner to fold in).
//
// The return is the console's own expression: `miRoadCount != liRoadIndex`, i.e. TRUE when the
// loop broke early on a match (0x82335338-0x82335344 `subf`/`cntlzw`/`extrwi 1,26`/`xori 1` is the
// compiler's !=0 test on that difference). A zero road count therefore answers false, which is
// what makes the caller's assert fire when RoadRules was never built.
// NO assert on mpStreetManager here -- unlike GetCurrentRoadID above, the console does not check it.
// ----------------------------------------------------------------------------
bool BrnGameState::RoadRulesManager::IsRoadLimitRegionValid(CgsID lRegionId, CgsID lLimitId) const
{
    (void)lRegionId;   // see the banner: r4 is never read by the console body

    // The console re-fetches the StreetData through the ResourcePtr on EVERY loop test (five
    // separate `StreetData_::oper` calls in 0xE0 bytes) -- that is the compiler evaluating the
    // source's `mpStreetManager->GetStreetData()->GetRoadCount()` in the loop condition, so the
    // condition is written the same way here.
    BrnStreetData::RoadIndex liRoadIndex = 0;

    while (liRoadIndex < mpStreetManager->GetStreetData()->GetRoadCount())
    {
        const BrnStreetData::Road* lpRoad = mpStreetManager->GetStreetData()->GetRoad(liRoadIndex);

        if (lLimitId == lpRoad->GetRoadLimitId0())
            break;
        if (lLimitId == lpRoad->GetRoadLimitId1())
            break;

        ++liRoadIndex;
    }

    return liRoadIndex != mpStreetManager->GetStreetData()->GetRoadCount();
}
