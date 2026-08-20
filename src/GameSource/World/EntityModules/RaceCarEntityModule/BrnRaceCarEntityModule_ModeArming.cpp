// ============================================================================
// BrnWorld::RaceCarEntityModule -- THE GAME-MODE ARMING SLICE.
//
//   SetAllActiveCarsInGameMode   X360 0x822BE0A0
//
// ⭐ WHY THIS SLICE EXISTS. `RaceCarEntityModule::mbIsInGameMode` (+99140) is the byte the
// whole free-roam gameplay stage turns on: AttachActiveRaceCar @0x822F4DB0 copies it into
// every ActiveRaceCar (`stb r11,0x777(r31)`), and PostSceneUpdate / PostPhysicsUpdate /
// UpdateTrafficAndRaceCarNearMisses / UpdateOutputInterfaces / PlaceRaceCarOnLoad /
// IsRaceCarWrappable / SetupCarColour all early-out while it is false. In the ARTIST image
// it has exactly ONE setter -- HandlePrepareForModeAction @0x823092F0 (`*(a1+99140) = 1`) --
// and one clearer, HandleStopModeAction @0x82307A30. Both hang off game ACTION 23
// (PrepareForModeAction), whose producer is ModeManager::PrepareForMode @0x82342930.
//
// This TU is the leaf end of that chain: the tail call HandlePrepareForModeAction makes
// once the module-level flags are set.
//
// SOURCE: BURNOUT_X360_ARTIST.XEX, raw asm (0x822BE0A0..0x822BE230). The pseudocode for
// this function is misleading -- it reports `IsAttached()` as the loop's `result` and hides
// the inlined RaceCar::SetInCurrentGameMode -- so every claim below is asm-attested.
// ============================================================================
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCar.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnWorld
{

// ----------------------------------------------------------------------------
// SetAllActiveCarsInGameMode @ 0x822BE0A0.
//
// Walk all eight active-race-car slots (r30 = this + 0x1A60 == &maActiveRaceCars[0],
// stride 0x1CD0 == sizeof(ActiveRaceCar) == 7376, `li r21,8` iterations) and put every
// ATTACHED slot's global RaceCar into the current game mode.
//
// ⚠️ THE SECOND ARGUMENT IS NOT THE ONLINE FLAG. The console's only caller,
// HandlePrepareForModeAction @0x823092F0, passes `*v13` where `v13 = (a1 + 99143)` ==
// mbCarSelectAllowedInGameMode -- the same byte HandleSetupNetworkCarAction @0x82305688
// hands to SetInCurrentGameMode as its `lbCarSelectAllowed`. Naming it "online" here would
// have quietly swapped two adjacent module bytes (+99141 IS the online flag).
//
// The X360 INLINES RaceCar::SetInCurrentGameMode @0x822B3F08 rather than calling it:
//     lwz r31, 0x6F0(r30)      ; r31 = GetGlobalRaceCar()
//     lbz r11, 0xA4(r31) ...   ; assert muType < E_RACE_CAR_TYPE_COUNT   (BrnRaceCar.h:547)
//     lbz r11, 0xA4(r31) ...   ; assert muType != 3 == IsInWorld()       (BrnRaceCar.h:713)
//     stb r22, 0xA6(r31)       ; mbIsInGameMode               = 1  <-- r22 is the LITERAL 1
//     stb r20, 0xA7(r31)       ; mbCarSelectAllowedInGameMode = a2
// so the in-game flag is a hard-coded true at this site and the argument only reaches the
// car-select-allowed byte. Calling the real (reconstructed, matching) member expresses the
// same stores; the console's `lbInGameMode == true` arm is the one taken, and its
// `!lbInGameMode && mpActiveRaceCar == NULL` index-release arm is dead here by construction.
//
// The trailing range assert is `GetActiveRaceCarIndex()` bounds-checked. The asm tests it
// TWICE -- unsigned `cmplwi r11,0x80` (catches the -1 sentinel, whose byte is 0xFF) then
// sign-extended `cmpwi r11,8` -- which together are exactly "index in [0, 8)".
// ----------------------------------------------------------------------------
void RaceCarEntityModule::SetAllActiveCarsInGameMode(bool lbCarSelectAllowedInGameMode)
{
    for (s32 liActiveRaceCars = 0;
         liActiveRaceCars < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         ++liActiveRaceCars)
    {
        ActiveRaceCar& lrActiveRaceCar = maActiveRaceCars[liActiveRaceCars];

        // asm 0x822BE0F4..0x822BE104: the whole body is skipped for a detached slot.
        if (!lrActiveRaceCar.IsAttached())
        {
            continue;
        }

        // asm 0x822BE108..0x822BE134 -- GetGlobalRaceCar()'s own IsAttached() tripwire
        // (BrnActiveRaceCar.h:1089), re-emitted by the inliner at each of its three reads.
        CGS_ASSERT(lrActiveRaceCar.IsAttached(), "IsAttached()");

        RaceCar* lpGlobalRaceCar = lrActiveRaceCar.GetGlobalRaceCar();

        lpGlobalRaceCar->SetInCurrentGameMode(true, lbCarSelectAllowedInGameMode);

        CGS_ASSERT(lrActiveRaceCar.IsAttached(), "IsAttached()");

        CGS_ASSERT(lpGlobalRaceCar->GetActiveRaceCarIndex() >= E_ACTIVE_RACE_CAR_INDEX_0 &&
                       lpGlobalRaceCar->GetActiveRaceCarIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "( maActiveRaceCars[liActiveRaceCars].GetGlobalRaceCar()->"
                   "GetActiveRaceCarIndex() >= E_ACTIVE_RACE_CAR_INDEX_0) && "
                   "(maActiveRaceCars[liActiveRaceCars].GetGlobalRaceCar()->"
                   "GetActiveRaceCarIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT )");
    }
}


// ----------------------------------------------------------------------------
// SetAllCarsOnStartLine @ 0x822A4850.
//
// Put every ATTACHED active-race-car slot into leRaceStartState and clear its start-line
// boost bookkeeping. SetupOpponents @0x82307DF0 is the caller reached on the OFFLINE arm of
// HandlePrepareForModeAction, with lbIncludePlayer == true at both of its call sites.
//
// The player-slot skip is `if (liIndex != mePlayerActiveRaceCarIndex || lbIncludePlayer)`
// -- asm 0x822A48C0..0x822A48D4 reads the player index through the module offset 0x182F8
// (r24 = 0x182F8, `lwzx r11,r29,r24`) and falls through to the store block when the two
// differ OR the flag is set. So the flag only ever ADDS the player's own slot.
//
// The three stores (asm 0x822A4910..0x822A4918), all by name:
//     stfs f31, 0x734(r31)   mfTimeToStartLineBoostChange = -1.0f  (flt_820037C8, dumped)
//     stw  r23, 0x77C(r31)   meRaceStartState             = leRaceStartState
//     stb  r22, 0x780(r31)   mbIsDoingStartLineBoost      = false  (r22 is the literal 0)
//
// ⚠️ THE LOOP BOUND IS DELIBERATELY OFF-BY-ONE-LOOKING. The console increments FIRST, then
// asserts `liIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT` (BurnoutConstants.h:39 -- the
// range-guarded EActiveRaceCarIndex post-increment's own tripwire, which permits reaching
// the one-past-the-end value) and only then tests `< 8` to continue. The body therefore
// runs for indices 0..7 and the assert never fires. Written as a plain 0..8 loop here; the
// assert is the enum increment's, not this function's, so it is not re-emitted.
// ----------------------------------------------------------------------------
void RaceCarEntityModule::SetAllCarsOnStartLine(ActiveRaceCar::ERaceStartState leRaceStartState,
                                                bool lbIncludePlayer)
{
    for (s32 liActiveRaceCarIndex = 0;
         liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         ++liActiveRaceCarIndex)
    {
        const EActiveRaceCarIndex leActiveRaceCarIndex =
            static_cast<EActiveRaceCarIndex>(liActiveRaceCarIndex);

        if (!GetActiveRaceCar(leActiveRaceCarIndex)->IsAttached())
        {
            continue;
        }

        if (leActiveRaceCarIndex == mePlayerActiveRaceCarIndex && !lbIncludePlayer)
        {
            continue;
        }

        ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar(leActiveRaceCarIndex);

        CGS_ASSERT(lpActiveRaceCar->IsAttached(), "IsAttached()");

        lpActiveRaceCar->SetOnStartLine(leRaceStartState);
    }
}

} // namespace BrnWorld
