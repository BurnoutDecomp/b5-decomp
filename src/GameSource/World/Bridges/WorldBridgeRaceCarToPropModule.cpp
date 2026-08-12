#include "GameSource/World/Bridges/WorldBridgeEntityModulesToEntityModules.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // gpDebugPrint (PROPS-BOOT one-shot)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"  // RaceCarState::mLinearVelocity

// ============================================================================
// GameSource/World/Bridges/WorldBridgeRaceCarToPropModule.cpp
//
// WorldModule::BridgeRaceCarModuleToPropModule_PreScene  @ 0x827A5510  (78 instructions)
//
// The prop module's view of the CARS. PropEntityModule::PreSceneUpdate drives cell
// activation, prop-physics wake-up and the hit/smash bookkeeping off the player's world
// position, which car slot the player is, whether the player is crashing/wrecked, and the
// eight per-car linear velocities. All of it arrives through this one bridge; while it was
// an inert WorldLinkStubs gate the prop input buffer kept Construct's zeroes.
//
// ---- DWARF home ------------------------------------------------------------
// PS3 DecFIGS unity dump _compile/BrnWorldBridgesUnity.cpp:2717, whose scope hints name
// WorldBridgeEntityModulesToEntityModules.cpp:138/147/149/152 -- that is the home TU, and
// this function's declaration already lives in that TU's header (included above).
//
//   ⭐ FILE SPLIT, the WorldBridgeRaceCarToWorldModule.cpp precedent (2026-08-01):
//   WorldBridgeEntityModulesToEntityModules.cpp IS NOT MOUNTED (three still-declaration-only
//   IO accessors its two remaining bridges need). This bridge needs none of them, so it gets
//   its own mountable TU. It is kept SEPARATE from its sibling
//   WorldBridgeWorldModuleToPropModule.cpp deliberately: that one closes with zero
//   unresolved externals and must be mountable on its own merits.
//   DELETE-WHEN: those three accessors land and the parent TU can be mounted whole.
//
// ---- The console body (0x827A5510..0x827A567C) -----------------------------
// r4 = lpPropInputBuffer_PreScene (dest), r5 = lpRaceCarOutputBuffer_PreScene (src),
// r6 = lUpdateSet; r3 (the WorldModule `this`) is overwritten at 0x827A5528, never read.
//
// The update-set test is `extrwi r11,r6,8,16 ; clrlwi r11,r11,31` == bit 0x100 of the u16 --
// the SAME replay-interface selector WorldModule::Update already uses at
// BrnWorldModule.cpp:3052. It splits the function into two ASYMMETRIC arms; this is not a
// transcription slip, the console really does far less work on the replay arm:
//
//   REPLAY ARM (0x827A5648, updateSet & 0x100):
//     bl 0x8279D650  OutputBuffer_PreScene::GetReplayActiveRaceCarOutputInterface() const
//                    (read-lock; `addis r3,this,0xF ; addi r3,r3,-0x23E0` == +973856;
//                     its baked assert line 0x126 == 294 is the DWARF's :294 declaration)
//     bl 0x82277B90  IsPlayerCarActive() ; if (result == 1)
//       bl 0x823102F0  GetPlayerPosition()  ->  stvx128 dest+0x6F0   (mPlayerPos)
//     return.        -- NO car index, NO crashing/wrecked, NO velocity loop.
//
//   LIVE ARM (the fall-through):
//     bl 0x8279D500  OutputBuffer_PreScene::GetActiveRaceCarOutputInterface() const
//                    (read-lock; +960960; baked assert line 0x120 == 288 == DWARF :288)
//     bl 0x82277B90  IsPlayerCarActive() ; if (result == 1)
//       bl 0x823102F0  GetPlayerPosition()      -> stvx128 dest+0x6F0  mPlayerPos
//       bl 0x82277BF8  GetPlayerActiveRaceCarIndex()
//                                               -> stb     dest+0x78C  mu8PlayerCarIndex
//       lwz r11,0x2858(src) ; cmpwi -1 ? 0 : lbz 0x460*idx + 0x77A(src)
//                                               -> stb     dest+0x793  mbPlayerCrashing
//                    -- the INLINED IsPlayerCarCrashing() (that inline is already committed
//                       in BrnRaceCarEntityModuleOutputInterface.h with this exact shape:
//                       1914 - 816 == RaceCarState +1098 == mbCrashing).
//       lbz r11,0x28E0(src)                     -> stb     dest+0x794  mbPlayerWrecked
//                    -- the INLINED IsPlayerWrecked() (bodied out-of-line this wave).
//     for (i = 0; i < 8; ++i)                   -- runs REGARDLESS of IsPlayerCarActive
//       (two bounds tripwires, the inlined IsRaceCarActive's, at this interface's header
//        lines 0x356/0x357 == 854/855)
//       lhz r11, (src + 0x2780 + 2*i) & 1       -- maxRaceCarFlags[i] bit 0 == IsRaceCarActive(i)
//       bl 0x8227D690  GetRaceCarState(i)       -- returns &maRaceCarStates[i]
//                                                  (`mulli 0x460 ; add ; addi 0x330`)
//       lvx128 v0, r3, 0x330 ; stvx128 v0, r0, dest+0x700+16*i
//                                               -- RaceCarState +816 == mLinearVelocity,
//                                                  into maRaceCarVelocity[i]
//
// ⚠ THE VELOCITY COPY IS A WHOLE 16-BYTE QUADWORD, and that is deliberate: the w lane of
// RaceCarState::mLinearVelocity carries the car's speed in MPH (|v| * 2.2369363, the
// flt_82014648 constant the physics applies when it publishes the state), so a
// three-component copy would lose it with no visible error. Modelled as a plain Vector3 assignment,
// which IS the 16-byte type here (Vector3 is the 16-byte rw::math::vpu vector) -- exactly
// what the console's single lvx/stvx pair does. No hand-written w-lane arithmetic here:
// this bridge does not compute the MPH value, it only carries it.
//
// ⚠ NO CONSOLE OFFSET IS TRANSCRIBED BELOW. Every field named in the asm map above is
// reached through its named accessor / named member; the offsets are provenance only.
//
// LOCKING: BrnWorldModule.cpp:2015 brackets both buffers (dest write, source read), which is
// why both interface getters are the read-lock const forms.
//
// The console tail forwards the last call's register as an artifact; the logical return type
// is void (DWARF unity :2717).
// ============================================================================

namespace WorldModule
{

// The BrnUpdateSet bit that selects the REPLAY active-race-car interface over the live one.
// X360: `extrwi r11,r6,8,16 ; clrlwi r11,r11,31` (bit 0x100 of the u16). Same selector as
// WorldModule::Update's own player-position latch (BrnWorldModule.cpp:3052).
static const BrnUpdateSet KU_UPDATESET_USE_REPLAY_INTERFACE = 0x100;

// ----------------------------------------------------------------------------
// Never called. Pins the structural facts this bridge relies on.
//
//   * Vector3 IS the 16-byte quadword the console's single lvx/stvx pair moves. If it
//     ever became a 12-byte {x,y,z} the velocity copy below would drop the w lane --
//     which carries the car's speed in MPH -- with nothing to notice.
//   * RaceCarState::mLinearVelocity is the member at the console's +0x330 (816), the
//     offset the asm uses to identify it. Unlike the two IO buffers, RaceCarState is a
//     pure-POD physics snapshot with no pointers, so its host layout DOES still match
//     the console and the offset can be pinned rather than merely commented.
//   * the velocity array has one slot per active race car (the console's 8-iteration
//     loop, `cmpwi r31, 8`).
// No offsetof pins on either IO buffer: every field there goes through a named
// accessor and both deliberately depart from the console layout on x64.
// ----------------------------------------------------------------------------
static void _AssertLayout()
{
    static_assert(sizeof(Vector3) == 16,
                  "Vector3 must be the 16-byte quadword: maRaceCarVelocity's w lane carries speed in MPH");
    static_assert(offsetof(BrnPhysics::Vehicle::RaceCarState, mLinearVelocity) == 816,
                  "X360 RaceCarState::mLinearVelocity @+0x330 (the bridge's `lvx128 v0, r3, 0x330`)");
    static_assert(E_ACTIVE_RACE_CAR_INDEX_COUNT == 8,
                  "X360 velocity loop runs 8 iterations (`cmpwi r31, 8`)");
}

// @ 0x827A5510
void BridgeRaceCarModuleToPropModule_PreScene(
    void* lpWorldModule,
    BrnWorld::PropEntityIO::InputBuffer_PreScene* lpPropInputBuffer_PreScene,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene,
    BrnUpdateSet lUpdateSet)
{
    (void)lpWorldModule;   // X360 r3 -- overwritten at 0x827A5528, never read

    CGS_ASSERT(lpPropInputBuffer_PreScene != 0, "lpPropInputBuffer_PreScene != NULL");
    CGS_ASSERT(lpRaceCarOutputBuffer_PreScene != 0, "lpRaceCarOutputBuffer_PreScene != NULL");

    if ( ( lUpdateSet & KU_UPDATESET_USE_REPLAY_INTERFACE ) != 0 )
    {
        // The replay arm: position only (see the banner -- the console really does stop here).
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
            lpReplayActiveRaceCarInterface =
                lpRaceCarOutputBuffer_PreScene->GetReplayActiveRaceCarOutputInterface();

        if ( lpReplayActiveRaceCarInterface->IsPlayerCarActive() )
        {
            lpPropInputBuffer_PreScene->SetPlayerPosition(
                lpReplayActiveRaceCarInterface->GetPlayerPosition() );
        }
        return;
    }

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
        lpActiveRaceCarInterface =
            lpRaceCarOutputBuffer_PreScene->GetActiveRaceCarOutputInterface();

    if ( lpActiveRaceCarInterface->IsPlayerCarActive() )
    {
        lpPropInputBuffer_PreScene->SetPlayerPosition(
            lpActiveRaceCarInterface->GetPlayerPosition() );
        lpPropInputBuffer_PreScene->SetPlayerCarIndex(
            static_cast<u8>( lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex() ) );
        lpPropInputBuffer_PreScene->SetPlayerCarCrashing(
            lpActiveRaceCarInterface->IsPlayerCarCrashing() );
        lpPropInputBuffer_PreScene->SetPlayerWrecked(
            lpActiveRaceCarInterface->IsPlayerWrecked() );
    }

    // The eight per-car linear velocities. Runs whether or not the player car is active --
    // the console's loop sits outside the IsPlayerCarActive block.
    for ( s32 liActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
          liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
          ++liActiveRaceCarIndex )
    {
        const EActiveRaceCarIndex leRaceCarEnum =
            static_cast<EActiveRaceCarIndex>( liActiveRaceCarIndex );

        if ( lpActiveRaceCarInterface->IsRaceCarActive( leRaceCarEnum ) )
        {
            const Vector3 lLinearVelocity =
                lpActiveRaceCarInterface->GetRaceCarState( leRaceCarEnum )->mLinearVelocity;
            lpPropInputBuffer_PreScene->SetRaceCarVelocity( liActiveRaceCarIndex,
                                                           lLinearVelocity );
        }
    }

    // DIAGNOSTIC (prop-spawn wave 2026-08-12) -- NOT in the X360 binary. Deliberately NOT
    // gated on gxMessageFilterFlags. One-shot: fires the first frame the player car is
    // actually active, i.e. the first frame the prop module gets a real player position.
    // Boot-log grep: "PROPS-BOOT racecar->prop bridge".
    {
        static bool sbLoggedFirstPlayer = false;
        if ( !sbLoggedFirstPlayer && CgsDev::Log::gpDebugPrint != 0 &&
             lpActiveRaceCarInterface->IsPlayerCarActive() )
        {
            sbLoggedFirstPlayer = true;
            const Vector3 lPlayerPosition = lpActiveRaceCarInterface->GetPlayerPosition();
            *CgsDev::Log::gpDebugPrint
                << "PROPS-BOOT racecar->prop bridge first player publish: carIndex "
                << static_cast<s32>( lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex() )
                << " pos " << lPlayerPosition.x << "," << lPlayerPosition.y
                << "," << lPlayerPosition.z << "\n";
        }
    }
}

}   // namespace WorldModule
