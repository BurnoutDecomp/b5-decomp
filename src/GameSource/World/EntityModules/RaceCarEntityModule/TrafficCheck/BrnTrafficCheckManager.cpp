#include "GameSource/World/EntityModules/RaceCarEntityModule/TrafficCheck/BrnTrafficCheckManager.h"

#include <cstddef>   // offsetof

// BrnWorld::TrafficCheckManager::Update, reconstructed from BURNOUT_X360_ARTIST.XEX
// (0x822F8F20). The shared 1536-byte variable event queue generic lives in
// CgsVariableEventQueue.h; the per-instantiation out-of-line emission is forced by
// GameShared/GameClasses/Module/VariableEventQueue_1536_16.cpp.
//
// rodata floats referenced by the X360 body:
//   flt_82014A54 == 6.0f  -- KF_TIME_MAX_TIME_BETWEEN_CHECKS (the dry-spell cap; the
//                            crashing branch pins the timer here, and the publish branch is
//                            taken only while the timer is < this cap).
//   flt_82001CC0 == 0.0f  -- the timer reset value on a passed-check event.
// Both values are X360-attested from the load constants in the asm (lfs from those rodata
// addresses). The two event type ids are X360-attested integer immediates (cmpwi 0x49 on
// the consumed type; li 0x4A on the published type).

namespace BrnWorld
{
    // the rodata constant flt_82014A54 (=6.0f) -- the maximum time allowed between two
    // successful traffic checks before the chain is broken. X360: flt_82014A54 == 6.0f.
    static const f32 KF_TIME_MAX_TIME_BETWEEN_CHECKS = 6.0f;

    // Reset value for the dry-spell timer when a check passes. X360: flt_82001CC0 == 0.0f.
    static const f32 KF_TIME_SINCE_LAST_CHECK_RESET = 0.0f;

    // Event type ids exchanged on the 1536-byte queues (X360-attested immediates).
    static const s32 KI_TRAFFIC_CHECK_PASSED_EVENT = 73;  // 0x49 -- consumed from the input.
    static const s32 KI_TRAFFIC_CHECK_CHAIN_EVENT  = 74;  // 0x4A -- published to the output.

    void TrafficCheckManager::_AssertLayout()
    {
        static_assert(offsetof(TrafficCheckManager, miCurrentCheckChain)  == 0, "miCurrentCheckChain @0");
        static_assert(offsetof(TrafficCheckManager, mfTimeSinceLastCheck) == 4, "mfTimeSinceLastCheck @4");
        static_assert(sizeof(TrafficCheckManager) == 8, "TrafficCheckManager sizeof 8");
    }

    void TrafficCheckManager::Update(const GameEventQueue* lpInput,
                                     GameEventQueue* lpOutput,
                                     bool lbLocalPlayerCrashing,
                                     f32 lfTimeStep)
    {
        mfTimeSinceLastCheck += lfTimeStep;

        if (lbLocalPlayerCrashing)
        {
            // While the local player is crashing, force the timer to the cap and break the
            // chain immediately (no publish this frame).
            mfTimeSinceLastCheck = KF_TIME_MAX_TIME_BETWEEN_CHECKS;
            miCurrentCheckChain  = 0;
            return;
        }

        // Walk every event in the input queue; each "check passed" event bumps the chain and
        // resets the dry-spell timer.
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lpInput->GetFirstEvent(&lpEvent, &liEventSize);
        while (lpEvent != 0)
        {
            if (liEventType == KI_TRAFFIC_CHECK_PASSED_EVENT)
            {
                mfTimeSinceLastCheck = KF_TIME_SINCE_LAST_CHECK_RESET;
                ++miCurrentCheckChain;
            }
            liEventType = lpInput->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }

        if (mfTimeSinceLastCheck >= KF_TIME_MAX_TIME_BETWEEN_CHECKS)
        {
            // Dry spell exceeded the cap -- the chain is broken.
            miCurrentCheckChain = 0;
        }
        else
        {
            // Still within the cap -- publish the current chain count.
            s32 liChain = miCurrentCheckChain;
            lpOutput->AddEvent(reinterpret_cast<const CgsModule::Event*>(&liChain),
                               KI_TRAFFIC_CHECK_CHAIN_EVENT,
                               (s32)sizeof(s32));
        }
    }
}
