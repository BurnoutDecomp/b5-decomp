#include "GameSource/World/Bridges/WorldBridgeInputToAI.h"                 // BridgeInputToAIModule
#include "GameSource/World/Bridges/WorldBridgeEntityModulesToAI.h"         // BridgeTrafficModuleToAIModule_Update
#include "GameSource/World/Bridges/WorldBridgePhysicsToEntityModules.h"    // BridgePhysicsModuleToAIModule_PostPhysics
#include "GameSource/World/Bridges/WorldBridgeAIToEntityModules.h"         // BridgeAIToEntityModules_PostPhysics / BridgeAIModuleToPhysicsModule

#include "GameSource/World/BrnWorldModule.h"                               // BrnWorld::WorldModule (GetLocalPlayerActiveRaceCarIndex / GetCarControl)
#include "GameSource/World/BrnWorldSharedConstants.h"                      // BrnWorld::E_CAR_CONTROL_AI_MODULE
#include "GameSource/World/AI/BrnAIModule.h"                               // BrnAI::AIModule (PostPhysicsUpdate)
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO.h"                    // AIModuleIO::InputBuffer / InputBuffer_PostPhysics
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"       // AIModuleIO::OutputBuffer
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"   // RaceCarEntityModuleIO::InputBuffer_PostPhysics
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                         // PhysicsModuleIO::InputBuffer / OutputBuffer
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"      // Vehicle::VehicleDriverInputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"            // Vehicle::BrnPlayerDriverControls (miVehicleID) / E_DRIVER_TYPE_AI
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"   // CgsSystem::TimerStatusInterface
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT

#include <cstdio>    // std::snprintf (the one formatted assert message, see BridgeAIModuleToPhysicsModule)

// ============================================================================
// GameSource/World/Bridges/WorldBridgeAIModule.cpp
//
// THE FIVE AI WORLD BRIDGES + THE AI POST-PHYSICS HOOK (aiwave lane A4, 2026-09-03).
//
// Until this partfile every hop between WorldModule::Update @0x827D63E8 and the AI module was a
// one-shot-log stub in WorldLinkStubs.cpp: nothing the world produced reached BrnAI::AIModule
// (no game actions, no timer, no player controls, no traffic view, no route requests) and
// nothing the AI produced reached physics (driver controls) or the race-car module (the AI
// race-car view). Six console bodies, each at its address:
//
//   WorldModule::BridgeInputToAIModule                     @0x827AB738  (WorldBridgeInputToAI.cpp:45)
//   WorldModule::BridgeTrafficModuleToAIModule_Update      @0x827A5020
//   WorldModule::BridgePhysicsModuleToAIModule_PostPhysics @0x827A5680  (WorldBridgePhysicsToEntityModules.cpp:142)
//   WorldModule::BridgeAIToEntityModules_PostPhysics       @0x827A4F58
//   WorldModule::BridgeAIModuleToPhysicsModule             @0x827AAAA8  (WorldBridgeAIToPhysics.cpp:40) -- EXPORT HOLE
//   BrnAI::AIModule::PostPhysicsUpdate                     @0x8276E428  (BrnAIModule.cpp:738)
// plus one accessor whose console body is ALSO an export hole and had no host body at all:
//   RaceCarEntityModuleIO::InputBuffer_PostPhysics::SetAIRaceCarInterface @0x8279E470 (BrnRaceCarEntityModuleIO.h:541)
//
// FILE SPLIT, same precedent as WorldBridgeInputToPhysicsModule.cpp / WorldBridgeRaceCarToWorldModule.cpp:
// the DWARF homes are five different TUs (WorldBridgeInputToAI.cpp, WorldBridgeEntityModulesToAI.cpp,
// WorldBridgePhysicsToEntityModules.cpp, WorldBridgeAIToPhysics.cpp / WorldBridgeAIToEntityModules.cpp,
// BrnAIModule.cpp, BrnRaceCarEntityModuleIO.cpp). This wave is file-disjoint by lane, so the
// AI-facing hops land together here. DELETE-WHEN: each body is folded back into its DWARF home
// TU (PostPhysicsUpdate into BrnAIModule.cpp, SetAIRaceCarInterface into
// BrnRaceCarEntityModuleIO.cpp, the bridges into their WorldBridge*.cpp).
//
// 0x827AAAA8 IS A HOLE IN THE IDA EXPORT SET (no JSON, not in names.tsv) -- exactly like
// BridgeInputToPhysicsModule @0x827AB830 before it. WorldModule::Update's xrefs_from names it
// (`0x827AAAA8 WorldModule::BridgeAIModuleToPhysicsModule`, between the pre-physics
// DestroyIOBuffer run and BridgeCrashModuleToPhysicsModule @0x827AAC70, which fixes its extent:
// 0x1C8 bytes, 114 instructions). Disassembled with capstone against
// scratch/postfx_step9_final/envfix/work/image.bin; the rodata it names is quoted at the body:
//   0x820C9930  "d:\p4\b5_main\burnout\main\code\gamesource\unity\../World/Bridges/WorldBridgeAIToPhysics.cpp"
//   0x820C9910  "lpAIOutputBuffer != NULL"                                      (line 0x28 = 40)
//   0x820C98F0  "AI Driver Queue is too long. "  +  <GetLength()>  +  0x820C98E4 " entries!\n"  (line 0x31 = 49)
//   0x820C98A8  "What the hell is the AI doing generating non-AI controls??"  (line 0x39 = 57)
// The DWARF (BrnWorldModule.h:599) declares it a WorldModule MEMBER -- which is why the console
// reads meLocalPlayerActiveRaceCarIndex / maeCarControls straight off r3. The tree models the
// bridge layer as free functions taking that r3 as `void* lpWorldModule`; the two members are
// reached BY NAME through the WorldModule doors (GetLocalPlayerActiveRaceCarIndex, GetCarControl).
//
// The console's per-bridge CPU monitor: BridgeAIToEntityModules_PostPhysics brackets its body
// with PerfMonCpu::Start/StopMonitor(*(this + 0x5E1CA8)) -- +6167720 == mGlobalCpuMonitors
// (+6167576) + 144 == miUT_AI_Bridge -- and WorldModule::Update @0x827D8078/0x827D80A8 ALSO
// brackets the call with `lwz r3, 0(r15)` (the same id), i.e. the console nests the monitor.
// The host call site in BrnWorldModule.cpp keeps the outer bracket; the inner one is
// [FLAG PC bring-up] not modelled here (mGlobalCpuMonitors is private to WorldModule and the
// nested start/stop is instrumentation only) -- the same disposition as the _PrePhysics
// sibling in WorldBridgePropModule.cpp. DELETE-WHEN a GetGlobalCpuMonitors door lands.
// ============================================================================

namespace WorldModule
{

// ---------------------------------------------------------------------------------------------
// @ 0x827AB738 -- WorldModule::BridgeInputToAIModule   (WorldBridgeInputToAI.cpp:45/:46)
//
// r31 = lpAIInputBuffer (r4), r30 = lpWorldInput (r5); r3 (the WorldModule) is never read.
//   0x827AB750  r28 = lpWorldInput->GetGameActionQueue()          @0x827A3708 (R, +147572; IDA "BrnWorldIO::UpdateInputBuffer")
//   0x827AB75C  r27 = lpAIInputBuffer->GetGameActionQueue()       @0x8279C4F8 (W, +0x103BC; IDA "InputBuffer::Get")
//   0x827AB770  "lpInQueue"  :45     } both NON-gating on the console (it Appends regardless)
//   0x827AB794  "lpOutQueue" :46     }
//   0x827AB7BC  VariableEventQueue<13312,16>::Append<13312,16>(r27, *r28)            @0x823C7440
//   0x827AB7C4  GetTimerStatusInterface @0x827A37B0        -> SetTimerInterface @0x8279C8C0
//   0x827AB7D8  GetRaceCarRaceDistanceInterface @0x827A3C48 (IDA "UpdateInputBuff") -> SetRaceCarRaceDistanceInterface @0x8279C428
//   0x827AB7EC  GetTakedownEventQueue @0x827A3858 (IDA "UpdateInputBuffer_") -> SetTakedownEventQueue @0x827A9618
//   0x827AB800  GetPlayerVehicleControls @0x827A3A50       -> SetPlayerVehicleControls @0x8279C5A0
//   0x827AB814  GetRaceRouteRequestQueue @0x827A3510 (IDA "Upda", +0x4AC0) -> AppendRaceRouteRequestQueue @0x827A9560
// The pseudocode's truncated callee names are resolved by the console offset each getter
// returns, all of which BrnWorldModuleIO.h already pins to the named getters used below.
// ---------------------------------------------------------------------------------------------
void BridgeInputToAIModule(
        void* lpWorldModule,
        BrnAI::AIModuleIO::InputBuffer* lpAIInputBuffer,
        const BrnWorldIO::UpdateInputBuffer* lpWorldInput)
{
    (void)lpWorldModule;

    const BrnWorldIO::GameActionQueue* const lpInQueue = lpWorldInput->GetGameActionQueue();
    BrnAI::AIModuleIO::InputBuffer::GameActionQueue* const lpOutQueue = lpAIInputBuffer->GetGameActionQueue();

    CGS_ASSERT(lpInQueue != 0, "lpInQueue");     // :45
    CGS_ASSERT(lpOutQueue != 0, "lpOutQueue");   // :46
    // [GUARD] the console falls through into Append, which dereferences both; a NULL here
    // faults on the console too, so the early-out only changes WHERE a null dies.
    if (lpInQueue == 0 || lpOutQueue == 0)
    {
        return;
    }

    lpOutQueue->Append(*lpInQueue);

    // FLAG cross-home cast (precedent: WorldBridgeInputToPhysicsModule.cpp): BrnWorldIO models
    // the world's timer block as its own 48-byte pointer-free POD while the AI buffer holds the
    // canonical CgsSystem::TimerStatusInterface (the DWARF member type). Same X360 member, same
    // flat 0x30 block; the sizes are pinned equal so the copy can never be partial.
    static_assert(sizeof(BrnWorldIO::TimerStatusInterface) == sizeof(CgsSystem::TimerStatusInterface),
                  "world/AI timer-status blocks must be the same 48-byte X360 member");
    lpAIInputBuffer->SetTimerInterface(
        reinterpret_cast<const CgsSystem::TimerStatusInterface*>(lpWorldInput->GetTimerStatusInterface()));

    // FLAG cross-home cast, same reason: BrnWorldIO::RaceCarRaceDistanceInterface is `s32[10]`,
    // the AI buffer holds GameStateModuleIO::RaceCarRaceDistanceInterface (whose Clear the AI
    // buffer's Construct calls @0x8278AC1C). The console copies 10 words between them.
    static_assert(sizeof(BrnWorldIO::RaceCarRaceDistanceInterface)
                      == sizeof(BrnAI::AIModuleIO::InputBuffer::RaceCarRaceDistanceInterface),
                  "world/AI race-distance blocks must be the same 10-word X360 member");
    lpAIInputBuffer->SetRaceCarRaceDistanceInterface(
        reinterpret_cast<const BrnAI::AIModuleIO::InputBuffer::RaceCarRaceDistanceInterface*>(
            lpWorldInput->GetRaceCarRaceDistanceInterface()));

    // Same type on both sides (CgsModule::EventQueue<BrnGameState::TakedownEvent,8>).
    lpAIInputBuffer->SetTakedownEventQueue(lpWorldInput->GetTakedownEventQueue());

    // FLAG cross-home cast, same reason: BrnWorldIO::PlayerVehicleControls is a 60-byte blob,
    // the AI buffer holds the DWARF-canonical BrnWorld::PlayerVehicleControls (13 floats + 8 bools).
    static_assert(sizeof(BrnWorldIO::PlayerVehicleControls)
                      == sizeof(BrnAI::AIModuleIO::InputBuffer::PlayerVehicleControls),
                  "world/AI player-vehicle-controls blocks must be the same 60-byte X360 member");
    lpAIInputBuffer->SetPlayerVehicleControls(
        reinterpret_cast<const BrnAI::AIModuleIO::InputBuffer::PlayerVehicleControls*>(
            lpWorldInput->GetPlayerVehicleControls()));

    // Same type on both sides (RouteMapModuleIO::RaceRouteRequestQueue == EventQueue<RaceRouteRequest,1>).
    lpAIInputBuffer->AppendRaceRouteRequestQueue(lpWorldInput->GetRaceRouteRequestQueue());
}

// ---------------------------------------------------------------------------------------------
// @ 0x827A5020 -- WorldModule::BridgeTrafficModuleToAIModule_Update
//
// Seventeen instructions, no asserts of its own: r31 = lpAIInputBuffer (r4); r3 unused.
//   0x827A5038  lpTrafficOutputBuffer_PostScene->GetTrafficAIInterface()   @0x827A0008 (R, +16416)
//   0x827A5044  lpAIInputBuffer->SetTrafficAIInterface(...)                @0x8279C7E0 (W, XMemCpy 0xB7A0)
// ---------------------------------------------------------------------------------------------
void BridgeTrafficModuleToAIModule_Update(
        void* lpWorldModule,
        BrnAI::AIModuleIO::InputBuffer* lpAIInputBuffer,
        const BrnTraffic::BrnTrafficIO::OutputBuffer_PostScene* lpTrafficOutputBuffer_PostScene)
{
    (void)lpWorldModule;

    lpAIInputBuffer->SetTrafficAIInterface(lpTrafficOutputBuffer_PostScene->GetTrafficAIInterface());
}

// ---------------------------------------------------------------------------------------------
// @ 0x827A5680 -- WorldModule::BridgePhysicsModuleToAIModule_PostPhysics
//                 (WorldBridgePhysicsToEntityModules.cpp:142/:143)
//
// r30 = lpAIModuleInputBuffer_PostPhysics (r4), r29 = lpPhysicsModuleOutputBuffer (r5).
//   0x827A56A0  "lpAIModuleInputBuffer_PostPhysics != NULL"  :142   } NON-gating on the console
//   0x827A56C4  "lpPhysicsModuleOutputBuffer != NULL"        :143   }
//   0x827A56E8  sub_8279F8E0 == PhysicsModuleIO::OutputBuffer::GetContactSpyInterface() const
//               (read-lock twin, +998192; the baked cite is BrnPhysicsModuleIO.h:369)
//   0x827A56EC  lwz r11, 0(r3) ; stw r11, 4(r30)   -- the contact-spy handle into the AI
//               buffer's +0x04 member: InputBuffer_PostPhysics::AppendContacts (DWARF :282) inlined.
// The pseudocode's `*(a2 + 4) = *result` is that inlined copy, not a raw word store.
// ---------------------------------------------------------------------------------------------
void BridgePhysicsModuleToAIModule_PostPhysics(
        void* lpWorldModule,
        BrnAI::AIModuleIO::InputBuffer_PostPhysics* lpAIModuleInputBuffer_PostPhysics,
        const BrnPhysics::PhysicsModuleIO::OutputBuffer* lpPhysicsModuleOutputBuffer)
{
    (void)lpWorldModule;

    CGS_ASSERT(lpAIModuleInputBuffer_PostPhysics != 0, "lpAIModuleInputBuffer_PostPhysics != NULL");   // :142
    CGS_ASSERT(lpPhysicsModuleOutputBuffer != 0, "lpPhysicsModuleOutputBuffer != NULL");               // :143
    // [GUARD] the console dereferences both unconditionally right after the tripwires.
    if (lpAIModuleInputBuffer_PostPhysics == 0 || lpPhysicsModuleOutputBuffer == 0)
    {
        return;
    }

    lpAIModuleInputBuffer_PostPhysics->AppendContacts(lpPhysicsModuleOutputBuffer->GetContactSpyInterface());
}

// ---------------------------------------------------------------------------------------------
// @ 0x827A4F58 -- WorldModule::BridgeAIToEntityModules_PostPhysics
//
// r30 = lpRaceCarInputBuffer_PostPhysics (r4), r29 = lpAIOutputBuffer (r5), r31 = this + 0x5E1CA8.
//   0x827A4F74  PerfMonCpu::StartMonitor(*(this + 0x5E1CA8))   -- miUT_AI_Bridge; run by the host call site (see banner)
//   0x827A4F80  lpAIOutputBuffer->GetAIRaceCarInterface() const   @0x8279CBF8 (R, +0x165D0; export hole, disassembled:
//               bit-4 test, "Not locked for reading\n", cite BrnAIModuleIO.h:451, returns this+0x165D0)
//   0x827A4F8C  lpRaceCarInputBuffer_PostPhysics->SetAIRaceCarInterface(...)   @0x8279E470 (W; export hole, bodied below)
//   0x827A4F94  PerfMonCpu::StopMonitor(*(this + 0x5E1CA8))
// No asserts of its own.
// ---------------------------------------------------------------------------------------------
void BridgeAIToEntityModules_PostPhysics(
        void* lpWorldModule,
        BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpRaceCarInputBuffer_PostPhysics,
        const BrnAI::AIModuleIO::OutputBuffer* lpAIOutputBuffer)
{
    (void)lpWorldModule;   // X360 r3 -- only the miUT_AI_Bridge monitor id is read through it (see banner)

    lpRaceCarInputBuffer_PostPhysics->SetAIRaceCarInterface(lpAIOutputBuffer->GetAIRaceCarInterface());
}

// ---------------------------------------------------------------------------------------------
// @ 0x827AAAA8 -- WorldModule::BridgeAIModuleToPhysicsModule   (WorldBridgeAIToPhysics.cpp; EXPORT HOLE)
//
// Register map: r23 = this (r3), r30 = lpPhysicsModuleInputBuffer (r4), r31 = lpAIOutputBuffer (r5),
// r27 = the file string @0x820C9930.
//   0x827AAAC4  cmplwi r31, 0 ; bne              CGS_ASSERT(lpAIOutputBuffer != NULL)          :40  (NON-gating)
//   0x827AAAF0  bl 0x8279CA00                    r28 = lpAIOutputBuffer->GetVehicleDriverInterface() const (R, +0x15120)
//   0x827AAAFC  bl 0x8279EDD0                    r24 = lpPhysicsModuleInputBuffer->GetVehicleDriverInterface() (W)
//   0x827AAB08  bl VariableEventQueue<5040,16>::GetLength(r28)   ; cmpwi 8 ; ble 0x827AABB4
//   0x827AAB14  ..0x827AABB0  the streamed assert: "AI Driver Queue is too long. " << GetLength() << " entries!\n"   :49
//   0x827AABC0  bl VariableEventQueue<5040,16>::GetFirstEvent(r28, &lpEvent[sp+0x54], &liSize[sp+0x50]) -> r30 (type)
//   0x827AABCC  blt cr6 -> done                  (type < 0 == empty)
//   0x827AABD4  r25 = this + 0x5E1AE8            == &meLocalPlayerActiveRaceCarIndex (+6167272)
//   0x827AABDC  r29 = 0x1786BC                   (4 * 0x1786BC == 0x5E1AF0 == &maeCarControls[0], +6167280)
//   loop 0x827AABE8:
//     cmpwi r30, 1 ; beq                         CGS_ASSERT(liType == E_DRIVER_TYPE_AI, "What the hell is the AI
//                                                doing generating non-AI controls??")  :57  (NON-gating)
//     lwz r11, 0(r25) ; cmpwi -1 ; beq skip      if (meLocalPlayerActiveRaceCarIndex != INVALID)
//     lwz r11, 0(r31) ; add r29 ; slwi 2 ; lwzx r11, r11, r23 ; cmpwi 2 ; bne skip
//                                                  if (maeCarControls[lpEvent->miVehicleID] == E_CAR_CONTROL_AI_MODULE (2))
//     0x827AAC40  bl VariableEventQueue<5040,16>::AddEvent(r24, lpEvent, liType, liSize)
//     skip: bl GetNextEvent(r28, lpEvent, &lpEvent, &liSize) -> r30 ; bge loop
// The event's leading word is miVehicleID: every driver-controls record derives from
// BrnPlayerDriverControls whose first member it is (@0x00) -- the same key the sibling
// BridgeEntityModulesToPhysicsModule_PrePhysics @0x827AAEC0 uses for its `== 1` (player) filter.
// This bridge is that filter's mirror: only the cars the world marks as AI-driven get their AI
// driver events forwarded to physics, and only while the local player has an active race car.
// ---------------------------------------------------------------------------------------------
void BridgeAIModuleToPhysicsModule(
        void* lpWorldModule,
        BrnPhysics::PhysicsModuleIO::InputBuffer* lpPhysicsModuleInputBuffer,
        const BrnAI::AIModuleIO::OutputBuffer* lpAIOutputBuffer)
{
    const BrnWorld::WorldModule* const lpModule = static_cast<const BrnWorld::WorldModule*>(lpWorldModule);

    CGS_ASSERT(lpAIOutputBuffer != 0, "lpAIOutputBuffer != NULL");   // :40
    // [GUARD] the console dereferences it unconditionally at 0x827AAAF0.
    if (lpAIOutputBuffer == 0)
    {
        return;
    }

    const BrnPhysics::Vehicle::VehicleDriverInputInterface* const lpSourceDriver =
        lpAIOutputBuffer->GetVehicleDriverInterface();                              // 0x8279CA00 (R)
    BrnPhysics::Vehicle::VehicleDriverInputInterface* const lpDestDriver =
        lpPhysicsModuleInputBuffer->GetVehicleDriverInterface();                    // 0x8279EDD0 (W)

    const BrnPhysics::Vehicle::VehicleDriverInputInterface::UpdateDriverEventQueue* const lpSourceQueue =
        lpSourceDriver->GetUpdateDriverQueue();

    // :49 -- the console streams the length into the assert buffer (it calls GetLength twice:
    // once for the compare, once for the stream). 8 == BrnWorld::KI_MAX_ACTIVE_RACE_CARS.
    if (lpSourceQueue->GetLength() > 8)
    {
        char lacMessage[96];
        std::snprintf(lacMessage, sizeof(lacMessage), "AI Driver Queue is too long. %d entries!\n",
                      static_cast<int>(lpSourceQueue->GetLength()));
        CGS_ASSERT(false, lacMessage);
    }

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    for (s32 liType = lpSourceQueue->GetFirstEvent(&lpEvent, &liSize);
         liType >= 0;
         liType = lpSourceQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        // 0x827AABE8 `cmpwi r30, 1` -- NON-gating tripwire: the console fires and still processes.
        CGS_ASSERT(liType == BrnPhysics::Vehicle::E_DRIVER_TYPE_AI,
                   "What the hell is the AI doing generating non-AI controls??");                 // :57

        if (lpModule->GetLocalPlayerActiveRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_INVALID)   // 0x827AAC08..0x827AAC14
        {
            const BrnPhysics::Vehicle::BrnPlayerDriverControls* const lpControls =
                static_cast<const BrnPhysics::Vehicle::BrnPlayerDriverControls*>(lpEvent);       // 0x827AAC18 lwz r11, 0(r31)

            if (lpModule->GetCarControl(lpControls->miVehicleID) == BrnWorld::E_CAR_CONTROL_AI_MODULE)   // 0x827AAC1C..0x827AAC2C
            {
                lpDestDriver->GetUpdateDriverQueue()->AddEvent(lpEvent, liType, liSize);         // 0x827AAC40
            }
        }
    }
}

}   // namespace WorldModule

// =============================================================================================
// @ 0x8276E428 -- BrnAI::AIModule::PostPhysicsUpdate   (BrnAIModule.cpp:738)
//
// Home TU is BrnAIModule.cpp (lanes A1-A3 this wave); parked here with the bridges that feed it.
// DELETE-WHEN: BrnAIModule.cpp takes it (move the body, keep the member grow in BrnAIModule.h).
//   0x8276E444  cmplwi r31, 0 ; bne        CGS_ASSERT(lpInputBuffer != NULL)  :738  (NON-gating)
//   0x8276E470  bl CgsModule::IOBuffer::LockForRead(lpInputBuffer)
//   0x8276E474  r11 = this + 0x4EB68       == &mContactSpyInterface (+322408, DWARF BrnAIModule.h:361)
//   0x8276E484  stw 0, 0(r11)              ContactSpyInterface::Construct inlined (it IS `mpData = 0`)
//   0x8276E488  lwz r10, 4(r31) ; stw r10, 0(r11)   mContactSpyInterface = *lpInputBuffer->GetContactSpyInterface()
//   0x8276E490  bl CgsModule::IOBuffer::UnlockForRead(lpInputBuffer)
// =============================================================================================
namespace BrnAI
{
    void AIModule::PostPhysicsUpdate( const AIModuleIO::InputBuffer_PostPhysics* lpInputBuffer )
    {
        CGS_ASSERT(lpInputBuffer != 0, "lpInputBuffer != NULL");   // :738
        // [GUARD] the console LockForRead()s through it unconditionally.
        if (lpInputBuffer == 0)
        {
            return;
        }

        lpInputBuffer->LockForRead();

        mContactSpyInterface.Construct();
        mContactSpyInterface = *lpInputBuffer->GetContactSpyInterface();

        lpInputBuffer->UnlockForRead();
    }
}

// =============================================================================================
// @ 0x8279E470 -- BrnWorld::RaceCarEntityModuleIO::InputBuffer_PostPhysics::SetAIRaceCarInterface
//                 (BrnRaceCarEntityModuleIO.h:541; EXPORT HOLE -- no JSON, not in names.tsv)
//
// Declared in BrnRaceCarEntityModuleIO.h (:532) with NO body anywhere in the tree; the bridge
// above is its only caller. Home TU is BrnRaceCarEntityModuleIO.cpp (not this lane's) --
// DELETE-WHEN that TU takes it. Disassembled from the image (0x8279E470..0x8279E524, 46 insns):
//   0x8279E484  lbz r11, 0(r28) ; rlwinm 0x1d,31,31      -- status bit 3 == IsBufferLockedForWriting
//   0x8279E4D8  "Not locked for writing\n" (rodata 0x82006294) -> FireAssert(..., 0x82015620 (this header), 0x21D == 541)
//   0x8279E50C  addis r3, r28, 0xD ; addi r3, r3, 0x6B30  == this + 879408 == &mAIRaceCarInterface
//   0x8279E51C  bl memcpy(&mAIRaceCarInterface, lpInterface, 0x490)
// AIRaceCarInterface is pointer-free (Vector3[35] x2, BitArray<35> x2, Vector2 x2), so the
// console's 0x490 == host sizeof and the member copy IS the memcpy.
// =============================================================================================
namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{
    void InputBuffer_PostPhysics::SetAIRaceCarInterface( const AIRaceCarInterface* lpInterface )
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        static_assert(sizeof(AIRaceCarInterface) == 0x490,
                      "AIRaceCarInterface is the console's pointer-free 0x490-byte block");
        mAIRaceCarInterface = *lpInterface;
    }
}
}
