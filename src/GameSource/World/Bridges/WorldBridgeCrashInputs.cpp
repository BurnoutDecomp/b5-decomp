// =================================================================================================
// GameSource/World/Bridges/WorldBridgeCrashInputs.cpp   (crash exit wave, 2026-08-25)
//
// The four crash-module INPUT bridges plus the crash->physics output bridge. Together with
// WorldBridgeCrashPostScene.cpp (landed c3655e4a) this closes the crash module's wiring: nothing
// reached it before, and nothing left it.
//
//   WorldModule::BridgeInputToCrashModule                    @0x827ADEE8  (25 insns)
//   WorldModule::BridgeEntityModulesToCrashModule_PreScene   @0x827A5060  (33 insns)
//   WorldModule::BridgePhysicsModuleToCrashModule_PostPhysics@0x827AB8B0  (17 insns)
//   WorldModule::BridgeTrafficToCrashModule_PostPhysics      @0x827AD708  (33 insns)
//   WorldModule::BridgeCrashModuleToPhysicsModule            @0x827AAC70  (33 insns)
//
// ⚠️ WorldModule::BridgeCrashModuleToOutput STAYS GATED and is deliberately not here. It carries
// CrashIO::OutputBuffer_PostPhysics' mNetworkOutputInterface out to the world output buffer -- the
// NETWORK half, whose every producer is parked. Landing it would move nothing.
//
// DWARF homes are WorldBridgeEntityModulesToCrash.cpp / WorldBridgeCrashToEntityModules.cpp (their
// assert strings name both). Same file-split precedent as WorldBridgeCrashPostScene.cpp and
// WorldBridgePropModule.cpp. DELETE-WHEN either home TU becomes mountable whole.
//
// ⭐ THE ONE THAT MATTERS MOST IS THE SMALLEST: BridgePhysicsModuleToCrashModule_PostPhysics is
// 17 instructions and it is the ONLY route by which a RaceCarCrashEvent -- the thing
// SetRaceCarCrashing has been publishing correctly for weeks -- reaches the crash module.
// =================================================================================================

#include "GameSource/World/Bridges/WorldBridgeEntityModulesToCrash.h"
#include "GameSource/World/Bridges/WorldBridgeCrashToEntityModules.h"
#include "GameSource/World/BrnWorldModuleIO.h"
#include "GameSource/Physics/BrnPhysicsModuleIO.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"    // CgsDev::Log::gpDebugPrint

#include <cstdlib>   // getenv -- the opt-in [crash-exit] dispatch witnesses only

namespace WorldModule
{

namespace
{
    // [crash-exit] dispatch witnesses -- NOT X360. Opt-in on the BRN_CRASH_RESPONSE_DIAG latch
    // (0/unset == inert); one line per bridge per session, proving the un-parked legs ran.
    bool CrashBridgeWitnessOn()
    {
        static int siWitness = -1;
        if( siWitness < 0 )
        {
            const char* lpcEnv = getenv( "BRN_CRASH_RESPONSE_DIAG" );
            siWitness = ( lpcEnv != 0 && lpcEnv[0] != '0' ) ? 1 : 0;
        }
        return ( siWitness == 1 ) && ( CgsDev::Log::gpDebugPrint != 0 );
    }
}

// =================================================================================================
// BridgeInputToCrashModule @ 0x827ADEE8   (25 insns)
//
//   0x827ADF08  bl UpdateInputBuffer::GetPlayerVehicleControls
//   0x827ADF1C  lbz  r11, 0x3B(controls)      \__ the boost/bounce byte, copied straight across
//   0x827ADF20  stbx r11, crashInput, 0xAE80  /
//   0x827ADF24  GetCrashNetworkIn      -> SetNetworkInputInterface
//   0x827ADF38  GetGameActionQueue     -> SetGameActionQueue
//   0x827ADF4C  GetVehicleDriverInput  -> SetVehicleDriverInterface
//   0x827ADF60  GetTimerStatusInterface-> SetTimerStatusInterface     <-- THE SIM TIMESTEP
//
// ⭐ THE TIMER LEG IS WHAT MAKES THE CRASH COUNTDOWN MOVE. CrashModule::TickCrashes reads
// GetTimerStatusInterface()->GetSimTimerStatus()->GetCurrentTimeStep() and subtracts it from every
// RaceCarCrash's mfSecondsBeforeCleanup. Without this bridge the interface stays cleared, the step
// is 0.0f * 1.0f == 0.0f, and the countdown never advances -- a crash would be tracked and then
// linger FOREVER, which looks exactly like the "car freezes at the impact point" symptom this wave
// exists to fix. That would have been a second, subtler version of the same bug.
//
// ⚠️⚠️ NAME DISCREPANCY, RECORDED NOT SILENTLY "FIXED": the byte copied to +0xAE80 is
// `lbz 0x3B(controls)` == offset 59 == PlayerVehicleControls::mbBoostBounce (the SHOWTIME bounce
// button), NOT mbBoost at +0x3A. The crash-side member was named `mbPlayerPressingBoost` by an
// earlier wave from a Hex-Rays local, and the name is kept (nothing else reads it, and the DWARF
// spelling is unverified) -- but the SOURCE is the bounce button and the asm is unambiguous.
// [[diagnostics-that-lie]]: read what a field actually carries, not what its name says.
//
// All five console legs run, in the console's order. The network leg (0x827ADF24/30) and the
// vehicle-driver leg (0x827ADF4C/58) were parked until 2026-09-24 (crash parity FOLLOWUPS 18/19):
// their destinations were console-sized blobs, then real types whose queues were never constructed
// -- InputBuffer_PreScene had no Construct, so CreateIOBuffer<T> bound to the base IOBuffer's. Both
// now pass their real types straight through: SetNetworkInputInterface is the interface's operator=
// (bitset copy + Clear()/Append() per race car's crashing-traffic queue, both ends constructed:
// UpdateInputBuffer::Construct and InputBuffer_PreScene::Construct), SetVehicleDriverInterface the
// console's whole-object copy (0x827A2254 memcpy 0x14B0). Their only readers are the online crash
// paths (ResetCrashedNetworkRaceCars, HandleNetworkCrashingTraffic); offline both carry empty queues.
// =================================================================================================
void BridgeInputToCrashModule(
    void* lpWorldModule,
    BrnWorld::CrashIO::InputBuffer_PreScene* lpCrashInputBuffer_PreScene,
    const BrnWorldIO::UpdateInputBuffer* lpUpdateInputBuffer)
{
    (void)lpWorldModule;   // X360 r3 -- overwritten at 0x827ADEFC, never read

    // ⚠️ TWO DOCUMENTED CASTS, and the reason they are SAFE is the reason the other three legs
    // need none (they pass their real types). BrnWorldModuleIO.h models the world input buffer's timer view and player-controls
    // view as its own console-SIZED placeholder structs (`f32 maData[12]` / `u8 maData[60]`)
    // rather than as the canonical types. Both canonical types are POINTER-FREE PODs whose host
    // sizeof therefore equals the console's -- CgsSystem::TimerStatusInterface is 2 x 24-byte
    // TimerStatus == 48 == 12 words, and BrnWorld::PlayerVehicleControls is 13 f32 + 8 bool == 60
    // -- so the reinterpretation is layout-exact and the static_asserts below hold it that way.
    // The network, game-action and vehicle-driver legs wrap types that DO contain host pointers or
    // address-dependent offsets, which is why they go through their real types, never a cast. (Precedent: BrnWorldModuleIO.h:132 records that the
    // WorldBridgeInputToEntityModules consumer already reinterpret_casts one of these views.)
    static_assert( sizeof( BrnWorldIO::TimerStatusInterface ) ==
                   sizeof( CgsSystem::TimerStatusInterface ),
                   "the world's timer-status placeholder must be layout-identical to the real type" );
    static_assert( sizeof( BrnWorldIO::PlayerVehicleControls ) ==
                   sizeof( BrnWorld::PlayerVehicleControls ),
                   "the world's player-controls placeholder must be layout-identical to the real type" );

    const BrnWorld::PlayerVehicleControls* lpControls =
        reinterpret_cast<const BrnWorld::PlayerVehicleControls*>(
            lpUpdateInputBuffer->GetPlayerVehicleControls() );
    lpCrashInputBuffer_PreScene->SetPlayerPressingBoost( lpControls->mbBoostBounce );

    // 0x827ADF24 GetCrashNetworkIn -> 0x827ADF30 SetNetworkInputInterface.
    lpCrashInputBuffer_PreScene->SetNetworkInputInterface( lpUpdateInputBuffer->GetCrashNetworkInterface() );

    lpCrashInputBuffer_PreScene->SetGameActionQueue(lpUpdateInputBuffer->GetGameActionQueue());

    // 0x827ADF4C GetVehicleDriverInputInterface -> 0x827ADF58 SetVehicleDriverInterface.
    lpCrashInputBuffer_PreScene->SetVehicleDriverInterface( lpUpdateInputBuffer->GetVehicleDriverInputInterface() );

    lpCrashInputBuffer_PreScene->SetTimerStatusInterface(
        reinterpret_cast<const CgsSystem::TimerStatusInterface*>(
            lpUpdateInputBuffer->GetTimerStatusInterface() ) );

    {
        static bool s_bWitnessed = false;
        if( !s_bWitnessed && CrashBridgeWitnessOn() )
        {
            s_bWitnessed = true;
            *CgsDev::Log::gpDebugPrint
                << "[crash-exit] BridgeInputToCrashModule: all five legs ran (network 0x827ADF30,"
                   " vehicle-driver 0x827ADF58 un-parked) [FLAG PC witness]\n";
        }
    }
}

// =================================================================================================
// BridgeEntityModulesToCrashModule_PreScene @ 0x827A5060   (33 insns)
//
//   two null asserts (WorldBridgeEntityModulesToCrash.cpp:37 / :38), then
//   RaceCarEntityModuleIO::OutputBuffer_PreScene::GetActiveRaceCarOutputInterface
//   -> CrashIO::InputBuffer_PreScene::SetActiveRaceCarInterface.
//
// ⭐ This is the crash module's ONLY window onto the cars: the player slot, every RaceCarState,
// and the per-car flags word. TickCrashes, ClearupCrashes and PreSceneUpdate all read through it.
// =================================================================================================
void BridgeEntityModulesToCrashModule_PreScene(
    void* lpWorldModule,
    BrnWorld::CrashIO::InputBuffer_PreScene* lpCrashInputBuffer_PreScene,
    const BrnWorld::RaceCarEntityModuleIO::OutputBuffer_PreScene* lpRaceCarOutputBuffer_PreScene)
{
    (void)lpWorldModule;

    CGS_ASSERT( lpCrashInputBuffer_PreScene != 0, "lpCrashInputBuffer_PreScene" );        // :37
    CGS_ASSERT( lpRaceCarOutputBuffer_PreScene != 0, "lpRaceCarOutputBuffer_PreScene" );  // :38

    lpCrashInputBuffer_PreScene->SetActiveRaceCarInterface(
        lpRaceCarOutputBuffer_PreScene->GetActiveRaceCarOutputInterface() );
}

// =================================================================================================
// BridgePhysicsModuleToCrashModule_PostPhysics @ 0x827AB8B0   (17 insns)
//
//   PhysicsModuleIO::OutputBuffer::GetVehicleOutputInterface        -> SetVehicleOutputInterface
//   PhysicsModuleIO::OutputBuffer::GetVehicleManagerOutputInterface -> SetVehicleManagerOutputInterface
//
// ⭐⭐ THE CRASH ENTRY. The second leg carries VehicleManagerOutputInterface, whose
// mRaceCarCrashEventQueue (@+0x3A0) is what VehicleManager::SetRaceCarCrashing has been filling
// since the crash-entry wave landed. Seventeen instructions stood between "the game knows the
// player crashed" and "the crash module knows the player crashed".
// ⚠️ NO null asserts -- unlike its traffic sibling, this console body has none. Do not add them.
// =================================================================================================
void BridgePhysicsModuleToCrashModule_PostPhysics(
    void* lpWorldModule,
    BrnWorld::CrashIO::InputBuffer_PostPhysics* lpCrashInputBuffer_PostPhysics,
    const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer)
{
    (void)lpWorldModule;

    lpCrashInputBuffer_PostPhysics->SetVehicleOutputInterface(
        lpPhysicsModuleOutputBuffer->GetVehicleOutputInterface() );
    lpCrashInputBuffer_PostPhysics->SetVehicleManagerOutputInterface(
        lpPhysicsModuleOutputBuffer->GetVehicleManagerOutputInterface() );
}

// =================================================================================================
// BridgeTrafficToCrashModule_PostPhysics @ 0x827AD708   (33 insns)
//
//   two null asserts (WorldBridgeEntityModulesToCrash.cpp:55 / :56), then
//   BrnTrafficIO::OutputBuffer_PostPhysics::GetCrashTrafficInputInterface
//   -> CrashIO::InputBuffer_PostPhysics::SetTrafficInputInterface.
//
// Not on the race-car exit path (it feeds the parked traffic ledger), but it is a 33-instruction
// straight-line copy into a member this tree already models with the real type, so it lands with
// the rest rather than staying a gate for no reason.
// =================================================================================================
void BridgeTrafficToCrashModule_PostPhysics(
    void* lpWorldModule,
    BrnWorld::CrashIO::InputBuffer_PostPhysics* lpCrashInputBuffer_PostPhysics,
    const BrnTraffic::BrnTrafficIO::OutputBuffer_PostPhysics* lpTrafficOutputBuffer_PostPhysics)
{
    (void)lpWorldModule;

    CGS_ASSERT( lpCrashInputBuffer_PostPhysics != 0, "lpCrashInputBuffer_PostPhysics" );          // :55
    CGS_ASSERT( lpTrafficOutputBuffer_PostPhysics != 0, "lpTrafficOutputBuffer_PostPhysics" );    // :56

    lpCrashInputBuffer_PostPhysics->SetTrafficInputInterface(
        lpTrafficOutputBuffer_PostPhysics->GetCrashTrafficInputInterface() );
}

// =================================================================================================
// BridgeCrashModuleToPhysicsModule @ 0x827AAC70   (33 insns)
//
//   two null asserts (WorldBridgeCrashToEntityModules.cpp:58 / :59), then
//   CrashIO::OutputBuffer_PreScene::GetVehicleInputInterface  (the const/read-lock overload)
//   -> PhysicsModuleIO::InputBuffer::GetVehicleInputInterface -> VehicleInputInterface::Append.
//
// UN-PARKED 2026-09-24 (crash parity FOLLOWUPS 18a): G63-D2 (4bc22993) promoted
// CrashIO::OutputBuffer_PreScene::mVehicleInputInterface from the one-byte placeholder to the real
// BrnPhysics::Vehicle::VehicleInputInterface (constructed by OutputBuffer_PreScene::Construct at the
// console's 0x827CEA20), so the console's three calls stand as written, in its order:
//   0x827AACD8  OutputBuffer_PreScene::GetVehicleInputInterface() const   (0x827A2488, read-lock)
//   0x827AACE4  PhysicsModuleIO::InputBuffer::GetVehicleInputInterface()  (0x8279ED28, write-lock)
//   0x827AACEC  VehicleInputInterface::Append(crash side)                 (0x823C87C0)
// The console's only producer into the crash side is HandleNetworkCrashingTraffic (the write-lock
// getter 0x827BB678 has that single caller), so offline this merges empty queues.
// (The DWARF-home copy of this function in WorldBridgeCrashToEntityModules.cpp is unmounted.)
// =================================================================================================
void BridgeCrashModuleToPhysicsModule(
    void* lpWorldModule,
    BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
    const BrnWorld::CrashIO::OutputBuffer_PreScene* lpCrashOutput_PreScene)
{
    (void)lpWorldModule;

    CGS_ASSERT( lpPhysicsModuleInputBuffer != 0, "lpPhysicsModuleInputBuffer" );   // :58
    CGS_ASSERT( lpCrashOutput_PreScene != 0, "lpCrashOutput_PreScene" );           // :59

    const BrnPhysics::Vehicle::VehicleInputInterface* lpCrashVehicleInput =
        lpCrashOutput_PreScene->GetVehicleInputInterface();                        // 0x827AACD8
    lpPhysicsModuleInputBuffer->GetVehicleInputInterface()->Append( *lpCrashVehicleInput );   // 0x827AACE4 / 0x827AACEC

    static bool s_bWitnessed = false;
    if( !s_bWitnessed && CrashBridgeWitnessOn() )
    {
        s_bWitnessed = true;
        *CgsDev::Log::gpDebugPrint
            << "[crash-exit] BridgeCrashModuleToPhysicsModule: crash vehicle-input appended to the"
               " physics input (0x827AACEC) [FLAG PC witness]\n";
    }
}

}   // namespace WorldModule
