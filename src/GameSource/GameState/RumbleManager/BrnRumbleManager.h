#pragma once

// ============================================================================
// BrnGameState::RumbleManager -- the controller-rumble/force-feedback manager.
//
// Layout + member names recovered from the DecFIGS DWARF (BrnRumbleManager.h)
// with the byte placement pinned by the X360 ARTIST asm (BrnRumbleManager.cpp):
// the four inline event queues and the trailing per-driven-wheel surface/rumble
// arrays sit at the X360 offsets below.
//
//   X360 offset map (32-bit build):
//     +0x000  mSurfaceList                         (surfacelist, un-homed -- see note)
//     +0x010  mPlayJoltEffectEventQueue            (EventQueue<PlayJoltEffectEvent,4>,   252B)
//     +0x10C  mPlayRumbleEffectEventQueue          (EventQueue<PlayRumbleEffectEvent,4>, 284B)
//     +0x228  mChangeVolumeRumbleEffectEventQueue  (EventQueue<ChangeVolumeRumbleEffectEvent,4>, 268B)
//     +0x334  mStopRumbleEffectEventQueue          (EventQueue<StopRumbleEffectEvent,4>,  60B)
//     +0x370  mau8SurfaceID[4]
//     +0x374  mau8NumWheelsOnSurface[4]
//     +0x378  manRumbleID[4]
//     +0x388  mafRumbleVolume[4]
//     +0x398  mbRumbleEnabled .. +0x39F mbWheelForceFeedback  (8 trailing bool flags)
//
// The PC byte offsets differ from the X360's because CgsModule::BaseEventQueue::mpEvents
// is a pointer (4 bytes on the 32-bit X360, 8 on PC x64), so each inline queue is wider
// here and the trailing members sit later. This object is engine-internal (constructed in
// place, never serialised), so the load-bearing contract is the typed member each method
// touches, NOT a byte-exact offset (which the pointer-width difference makes unattainable
// without an ABI hack) -- the same rule as CgsInputModuleIO.h's IO buffers.
//
// The owner embeds it BY VALUE: DWARF BrnGameStateModule.h:775 `RumbleManager mRumbleManager;`
// (X360 gsm+46680 == 0xB658, the `addis 1 / addi -0x49A8` every console call site builds).
//
// METHOD SLICE (additive-grow; DWARF order, X360-attested methods only). Bodied:
//   Construct                @0x82378A70   (FX-RUMBLE / G10-D5)
//   Prepare                  @0x823648D0
//   Update                   @0x82386A98   (FX-RUMBLE / G10-D1)
//   UpdatePauseState         @0x8236E728
//   OnVehicleAggressorImpact @0x823795C8   (FX-RUMBLE / G10-D3)
//   OnVehicleVictimImpact    ICF-folded onto 0x823795C8 on the X360 (G10-D3)
//   UpdateImpacts            @0x82379370   (FX-RUMBLE / G10-D2)
//   PlayJolt                 @0x8236E7F8   (FX-RUMBLE / G10-D7)
//   BridgeRumbleToInput      @0x82364978   (FX-RUMBLE3 / G10-D4) -- the drain of all four queues
// ⛔ NOT DECLARED, deliberately -- a declaration without a body is exactly the silent link-time
// drop this campaign keeps finding:
//   UpdateSurfaceRumble @0x82378AE0 (G10-D6) -- needs Attrib::Gen::rumblesurface's 0x3C-byte
//       data layout + accessors and a surface::RumbleSurface() RefSpec accessor (surface layout
//       +0x28) in GameSource/AttribSys/Generated/classes/, then mSurfaceList retyped to
//       Attrib::Gen::surfacelist. See Update's FLAG.
// A future TU MUST GROW this class ADDITIVELY rather than redefine it -- do NOT fork.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"  // CgsInput::InputIO event payloads + EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"    // CgsModule::VariableEventQueue<13312,16> (the GameActionQueue), CgsModule::Event
#include "GameSource/BurnoutConstants.h"                            // EActiveRaceCarIndex
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"  // BrnPhysics::Vehicle::EImpactType (OnVehicle*Impact)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface (+ RaceCarCrashEvent via BrnVehicleEvents.h)
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h" // BrnPhysics::ContactSpy::ContactSpyInterface

namespace BrnGameState
{
    class RumbleManager
    {
    public:
        // @ 0x82378A70 -- DWARF BrnRumbleManager.h:52. Seed the eight tail flags (rumble ENABLED
        // and wheel force-feedback ON, everything else clear) and bind the four inline event
        // queues. Sole caller GameStateModule::Construct @0x82380388 (0x823805C8).
        void Construct();

        // @ 0x823648D0 -- reset per-driven-wheel surface/rumble state and clear the four
        // pending rumble-event queues. Returns true.
        bool Prepare();

        // @ 0x82386A98 -- DWARF BrnRumbleManager.h:65 (PS3 mangling
        //   _ZN...6UpdateEPN8BrnWorld...RCEntityActiveRaceCarOutputInterfaceEPN9CgsModule10EventQueueIN10
        //   BrnPhysics7Vehicle17RaceCarCrashEventELi8EEE19EActiveRaceCarIndexPNS7_10ContactSpy19
        //   ContactSpyInterfaceEf -- every pointer non-const). The per-frame producer: the impact,
        //   crash-start and landing jolts. Called from GameStateModule::PreWorldUpdate @0x823A5800.
        //   lpCrashEventQueue and lfGameTimeStep are NOT read by the console body (r5 is overwritten
        //   before the first call, f1 is never read); kept for the declaration's shape.
        void Update(BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                    CgsModule::EventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent, 8>*     lpCrashEventQueue,
                    EActiveRaceCarIndex                                                    lePlayerCarIndex,
                    BrnPhysics::ContactSpy::ContactSpyInterface*                           lpContactSpyInterface,
                    f32                                                                    lfGameTimeStep);

        // @ 0x8236E728 -- track the paused/unpaused transition (mbGameWasPaused latch) and,
        // while paused, scan the game-action queue to mirror the pause request into
        // mbRumblePaused. lpGameActionQueue is the GameStateModuleIO InputBuffer's
        // GameActionQueue == CgsModule::VariableEventQueue<13312,16>.
        void UpdatePauseState(bool lbPaused, CgsModule::VariableEventQueue<13312, 16>* lpGameActionQueue);

        // @ 0x823795C8 -- DWARF BrnRumbleManager.h:87. The player slammed / shunted / traded paint
        // with a race car: one KN_RUMBLE_VEHICLE_IMPACT_PRIORITY jolt scaled by the impact type.
        // Caller ProcessGameEvents case 31 (0x823A27C8).
        void OnVehicleAggressorImpact(BrnPhysics::Vehicle::EImpactType leImpactType);

        // DWARF BrnRumbleManager.h:91. The same jolt when the PLAYER is the victim. The X360 has no
        // separate body (ProcessGameEvents 0x823A27EC `bl`s 0x823795C8 for this leg: ICF fold);
        // the PS3 twin 0x2401D4 is a thunk onto OnVehicleAggressorImpact.
        void OnVehicleVictimImpact(BrnPhysics::Vehicle::EImpactType leImpactType);

        // @ 0x82364978 -- DWARF BrnRumbleManager.h:97. Hand every queued jolt / stop / play / volume
        // request to the input module's pre-world buffer (Post*ByPlayer), publish the timer snapshot
        // and the pause / enable / force-feedback state, then empty the four queues. Sole caller
        // GameStateModule::BridgeRumbleToInput @0x8236B570 (a forward), from
        // BrnGameModule::DoUpdate_InputPreWorld @0x823C5650 under the buffer's write lock.
        void BridgeRumbleToInput(CgsInput::InputIO::PreWorldInputBuffer* lpInputInputBuffer,
                                 const CgsSystem::TimerStatusInterface*  lpTimerStatusInterface);

    private:
        // @ 0x82379370 -- DWARF BrnRumbleManager.h:137. The hardest contact on the player's run of
        // the race-car contact queue this frame -> one KN_RUMBLE_IMPACT_PRIORITY jolt.
        void UpdateImpacts(BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                           EActiveRaceCarIndex                                                    lePlayerCarIndex,
                           BrnPhysics::ContactSpy::ContactSpyInterface*                           lpContactSpyInterface);

        // @ 0x8236E7F8 -- DWARF BrnRumbleManager.h:159. Queue one jolt for player 0 on any port.
        void PlayJolt(s32 liRumblePriority, const CgsInput::InputIO::JoltEffect& lJoltEffect);

        // +0x000 -- surfacelist (Attrib-generated surface-list handle, un-homed). Held as
        // opaque storage sized to the X360 record: this TU's reconstructed methods do not
        // touch it, and its full type belongs to the Attrib codegen TU. Promote to the real
        // `surfacelist` type when that lands.
        u8 maSurfaceListStorage[16];                                                         // +0x000

        CgsModule::EventQueue<CgsInput::InputIO::PlayJoltEffectEvent, 4>          mPlayJoltEffectEventQueue;            // +0x010
        CgsModule::EventQueue<CgsInput::InputIO::PlayRumbleEffectEvent, 4>        mPlayRumbleEffectEventQueue;          // +0x10C
        CgsModule::EventQueue<CgsInput::InputIO::ChangeVolumeRumbleEffectEvent, 4> mChangeVolumeRumbleEffectEventQueue; // +0x228
        CgsModule::EventQueue<CgsInput::InputIO::StopRumbleEffectEvent, 4>        mStopRumbleEffectEventQueue;          // +0x334

        u8  mau8SurfaceID[4];             // +0x370  per-driven-wheel surface id      (0xFF == none)
        u8  mau8NumWheelsOnSurface[4];    // +0x374  wheels currently on each surface slot
        s32 manRumbleID[4];              // +0x378  live rumble id per slot           (-1 == none)
        f32 mafRumbleVolume[4];          // +0x388  live rumble volume per slot

        bool mbRumbleEnabled;             // +0x398
        bool mbRumblePaused;              // +0x399
        bool mbGameWasPaused;             // +0x39A
        bool mbInPictureParadise;         // +0x39B
        bool mbPlayerIsCrashing;          // +0x39C
        bool mbPlayerHasJustCheckedTraffic; // +0x39D
        bool mbPlayerIsInAir;             // +0x39E
        bool mbWheelForceFeedback;        // +0x39F
    };
}
