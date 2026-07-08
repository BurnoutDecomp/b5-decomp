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
// MINIMAL METHOD SLICE (additive-grow, flagged): only the two methods reconstructed by this
// TU pass -- Prepare() and UpdatePauseState() -- are declared. The remaining methods
// (Construct/Update/UpdateSurfaceRumble/UpdateImpacts/OnVehicle*Impact/PlayJolt/PlayRumble/
// ChangeRumbleVolume/StopRumble/BridgeRumbleToInput/Set|GetWheelForceFeedback/...) are left
// for their own reconstruction passes; a future TU MUST GROW this class ADDITIVELY (add the
// remaining DWARF-attested methods) rather than redefine it -- do NOT fork.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"  // CgsInput::InputIO event payloads + EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"    // CgsModule::VariableEventQueue<13312,16> (the GameActionQueue), CgsModule::Event

namespace BrnGameState
{
    class RumbleManager
    {
    public:
        // @ 0x823648D0 -- reset per-driven-wheel surface/rumble state and clear the four
        // pending rumble-event queues. Returns true.
        bool Prepare();

        // @ 0x8236E728 -- track the paused/unpaused transition (mbGameWasPaused latch) and,
        // while paused, scan the game-action queue to mirror the pause request into
        // mbRumblePaused. lpGameActionQueue is the GameStateModuleIO InputBuffer's
        // GameActionQueue == CgsModule::VariableEventQueue<13312,16>.
        void UpdatePauseState(bool lbPaused, CgsModule::VariableEventQueue<13312, 16>* lpGameActionQueue);

    private:
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
