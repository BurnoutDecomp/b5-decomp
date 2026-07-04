#include "GameSource/Network/Managers/BrnNetworkStateManager.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::StateManager::CurrentPlayerXUIDs::SetXUID  @ 0x82542540
//     (called by BrnNetwork::StateManager::StartGameMode)
//   BrnNetwork::StateManager::CurrentPlayerXUIDs::GetXUID  @ 0x825425C0
//     (called by BrnNetwork::StateManager::ProcessGuiEvents)
//
// An 8-slot, 16-byte-stride table mapping a network player id to that player's XUID.
// SetXUID inserts into the first free (miPlayerId == -1) slot; GetXUID looks a player up
// and copies out the stored 64-bit XUID. Both walk maSlots linearly and assert on
// overflow / not-found exactly as the X360 bodies do (the asm scans up to 8 slots before
// firing). Each store matches the asm width: the player id is a 32-bit `stw`, the XUID a
// single 64-bit `std` (the Hex-Rays high/low-half aliases are one std, not two stores).

namespace BrnNetwork
{
    StateManager::CurrentPlayerXUIDs*
    StateManager::CurrentPlayerXUIDs::SetXUID(s32 liPlayerId, u64 lqXUID)
    {
        s32 liSlot = 0;
        while (maSlots[liSlot].miPlayerId != KI_FREE_SLOT)
        {
            if (++liSlot >= KI_NUM_SLOTS)
            {
                CGS_ASSERT(false, "No free slots!\n");
                return this;
            }
        }

        maSlots[liSlot].miPlayerId = liPlayerId;   // stw r4, 0(slot)
        maSlots[liSlot].mu64XUID   = lqXUID;        // std r5, 8(slot)
        return this;
    }

    StateManager::CurrentPlayerXUIDs*
    StateManager::CurrentPlayerXUIDs::GetXUID(s32 liPlayerId, u64* lpXUID)
    {
        CGS_ASSERT(lpXUID != 0, "lpXUID");

        s32 liSlot = 0;
        while (maSlots[liSlot].miPlayerId != liPlayerId)
        {
            if (++liSlot >= KI_NUM_SLOTS)
            {
                CGS_ASSERT(false, "Couldn't find player!\n");
                return this;
            }
        }

        *lpXUID = maSlots[liSlot].mu64XUID;          // ld 8(slot); std 0(lpXUID)
        return this;
    }

    // BrnNetwork::StateManager::TeamSelectionFinishedCallback @ 0x82549870
    //
    // Menu-flow completion callback fired when team selection has finished. The X360 body
    // guards its two arguments then advances the state manager's state machine to 12:
    //   - result handle non-null, else the streamed assert "Team selection has failed somehow\n"
    //     (the BeginAssert + BasePriorityQueue::Clear + off_82000D00 StrStream-into-buffer
    //     inlining collapses to one CGS_ASSERT per project convention);
    //   - lpStateManager non-null, else CGS_ASSERT(..., "lpStateManager") (cpp:5663);
    //   - then `stw 12, 0x90(lpStateManager)` sets miState = 12.
    // The asserts are non-fatal, so the state store runs on every path. Returns the result
    // handle it was passed (r3 preserved as the callback's return value).
    void* StateManager::TeamSelectionFinishedCallback(void* lpResult, StateManager* lpStateManager)
    {
        CGS_ASSERT(lpResult != nullptr, "Team selection has failed somehow\n");
        CGS_ASSERT(lpStateManager != nullptr, "lpStateManager");

        lpStateManager->miState = 12;   // stw r11(=0xC), 0x90(r27)
        return lpResult;
    }
} // namespace BrnNetwork
