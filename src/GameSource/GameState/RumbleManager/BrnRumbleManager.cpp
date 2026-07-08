// ============================================================================
// BrnGameState::RumbleManager -- controller rumble / force-feedback manager.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. This pass lands the two methods that
// are fully groundable without inventing data:
//
//   Prepare          @ 0x823648D0
//   UpdatePauseState @ 0x8236E728
//
// The remaining ledger methods in this TU are BLOCKED pending recoverable inputs
// (see the block notes in the reviewer packet): OnVehicleAggressorImpact / Update /
// UpdateImpacts each memcpy a 48-byte JoltEffect template out of file-static rodata
// (unk_82CDBE10 / unk_82CDBDE0 / unk_82CDBE44+flt_82CDBE40 / unk_82030F0C) whose bytes
// are not recovered, and UpdateSurfaceRumble additionally needs the un-homed
// Attrib::Gen::surface / rumblesurface / roadsurface codegen types plus the sub_8227FB58
// / sub_8280A248 Attrib helpers. Reconstructing those without the real data/types would
// mean fabricating a rumble waveform, so they are left to a later pass.
// ============================================================================

#include "GameSource/GameState/RumbleManager/BrnRumbleManager.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h" // BrnPhysics::Vehicle::EVehicleDrivenWheel
#include "GameShared/GameClasses/Core/CgsAssert.h"                                     // CGS_ASSERT

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // @ 0x823648D0 -- BrnGameState::RumbleManager::Prepare()
    //
    // Reset the per-driven-wheel surface/rumble tracking arrays to their empty state,
    // then drop any queued rumble events. Returns true.
    // ------------------------------------------------------------------------
    bool RumbleManager::Prepare()
    {
        // The X360 walks the driven-wheel enum front-left..rear-right; the inlined
        // BrnPhysics::Vehicle::operator++(EVehicleDrivenWheel&) (BrnSimpleVehiclePhysics.h:332)
        // asserts the post-increment index stays within eNumDrivenWheels. Reproduced inline
        // here because that operator is not yet homed -- it never trips for the fixed 4-wheel
        // walk, but the asm fires it, so the assert is preserved.
        BrnPhysics::Vehicle::EVehicleDrivenWheel leWheelIndex = BrnPhysics::Vehicle::eFrontLeftWheel;
        do
        {
            mau8SurfaceID[leWheelIndex]          = 0xFF;
            mau8NumWheelsOnSurface[leWheelIndex] = 0;
            mafRumbleVolume[leWheelIndex]        = 0.0f;
            manRumbleID[leWheelIndex]            = -1;

            leWheelIndex = static_cast<BrnPhysics::Vehicle::EVehicleDrivenWheel>(leWheelIndex + 1);
            CGS_ASSERT(leWheelIndex <= BrnPhysics::Vehicle::eNumDrivenWheels,
                       "leEnumIndex <= eNumDrivenWheels");
        }
        while (leWheelIndex < BrnPhysics::Vehicle::eNumDrivenWheels);

        mPlayJoltEffectEventQueue.Clear();
        mPlayRumbleEffectEventQueue.Clear();
        mChangeVolumeRumbleEffectEventQueue.Clear();
        mStopRumbleEffectEventQueue.Clear();
        return true;
    }

    // ------------------------------------------------------------------------
    // @ 0x8236E728 -- BrnGameState::RumbleManager::UpdatePauseState()
    //
    // Latch the game-paused transition. On the rising edge (not-was-paused -> paused) the
    // rumble is paused; on the falling edge (was-paused -> unpaused) it is resumed; both edges
    // return immediately. When the paused/requested state is unchanged, the game-action queue
    // is scanned: a pause action (type 37) forces mbRumblePaused true, while a resume action
    // (type 39) mirrors the current request into mbRumblePaused.
    // ------------------------------------------------------------------------
    void RumbleManager::UpdatePauseState(bool lbPaused,
                                         CgsModule::VariableEventQueue<13312, 16>* lpGameActionQueue)
    {
        if (!mbGameWasPaused)
        {
            if (lbPaused)
            {
                mbGameWasPaused = true;
                mbRumblePaused  = true;
                return;
            }
        }
        else if (!lbPaused)
        {
            mbGameWasPaused = false;
            mbRumblePaused  = false;
            return;
        }

        mbGameWasPaused = lbPaused;

        // Game-action event-type discriminants (the queue returns the action id as the type):
        //   37 -> the pause action  (breaks the scan and forces the rumble paused)
        //   39 -> the resume action (mirrors the current pause request into mbRumblePaused)
        const CgsModule::Event* lpEvent = nullptr;
        s32 liSize = 0;
        s32 liType = lpGameActionQueue->GetFirstEvent(&lpEvent, &liSize);
        if (lpEvent != nullptr)
        {
            while (liType != 37)
            {
                if (liType == 39)
                    mbRumblePaused = lbPaused;

                liType = lpGameActionQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
                if (lpEvent == nullptr)
                    return;
            }
            mbRumblePaused = true;
        }
    }
}
