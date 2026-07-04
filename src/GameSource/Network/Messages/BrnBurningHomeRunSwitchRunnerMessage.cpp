#include "types.hpp"

#include "GameSource/Network/Messages/BrnBurningHomeRunSwitchRunnerMessage.h"
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsTestConnectionMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::BurningHomeRunSwitchRunnerMessage::GetName              @ 0x8257C298
//   BrnNetwork::BurningHomeRunSwitchRunnerMessage::GetPackedMessageSize @ 0x8257C288
//   BrnNetwork::BurningHomeRunSwitchRunnerMessage::PackOrUnpack         @ 0x8257C238
//   BrnNetwork::BurningHomeRunSwitchRunnerMessage::PrepareForSend       @ 0x8257F278
//   BrnNetwork::BurningHomeRunSwitchRunnerMessage::Retrieve             @ 0x8257F2A8
//
// A reliable message carrying a single NetworkPlayerID payload (the new runner). The base
// ReliableMessage carries the wrapped reliable id (and the sending/receiving player ids),
// and this subclass adds the runner id at +0x28.

namespace BrnNetwork
{
    // Reliable message type id (asm `li r4,0x1E` in PrepareForSend).
    static const s32 KI_BURNING_HOME_RUN_SWITCH_RUNNER_MESSAGE_TYPE = 30;   // 0x1E

    // BrnNetwork::BurningHomeRunSwitchRunnerMessage::GetName @ 0x8257C298
    // lis/addi a rodata string then blr -- no member or base access. Standalone ledger
    // body (own address, NOT inlined).
    const char* BurningHomeRunSwitchRunnerMessage::GetName() const
    {
        return "Burning home run switch runner message";
    }

    // BrnNetwork::BurningHomeRunSwitchRunnerMessage::GetPackedMessageSize @ 0x8257C288
    // Re-seeds the runner id to the -1 sentinel, then tail-calls a COMDAT-folded copy of
    // the bare reliable-base size probe (the binary resolved the shared stub to
    // TestConnectionMessage::GetPackedMessageSize). `this` is reinterpreted as that sibling
    // to match the X360 ICF tail call.
    s32 BurningHomeRunSwitchRunnerMessage::GetPackedMessageSize()
    {
        mNewRunnerID = KI_INVALID_PLAYER_ID;   // stw -1, +0x28
        return reinterpret_cast<CgsNetwork::TestConnectionMessage*>(this)->GetPackedMessageSize();
    }

    // BrnNetwork::BurningHomeRunSwitchRunnerMessage::PackOrUnpack @ 0x8257C238
    // ORs the reliable base id status with the NetworkPlayerID field status (both u8,
    // 0 == success). BrnNetworkManager::PackOrUnpack is the committed static NetworkPlayerID
    // field primitive (message + field pointer; == X360 sub_82881BF0).
    BrnNetworkManager::PackOrUnpackResult BurningHomeRunSwitchRunnerMessage::PackOrUnpack()
    {
        const BrnNetworkManager::PackOrUnpackResult lxBase = CgsNetwork::ReliableMessage::PackOrUnpack();
        return BrnNetworkManager::PackOrUnpack(this, &mNewRunnerID) | lxBase;
    }

    // BrnNetwork::BurningHomeRunSwitchRunnerMessage::PrepareForSend @ 0x8257F278
    // Re-arms the reliable slot (drops a previously-pending VALID flag), stores the new
    // runner id, then stamps the reliable message with type 30 (0x1E) for the given frame.
    void BurningHomeRunSwitchRunnerMessage::PrepareForSend(u16 lu16Frame, NetworkPlayerID lNewRunnerID)
    {
        // Re-arm: drop a previously-pending VALID flag before re-stamping the slot.
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;

        mNewRunnerID = lNewRunnerID;   // stw a3, +0x28
        CgsNetwork::ReliableMessage::PrepareForSend(KI_BURNING_HOME_RUN_SWITCH_RUNNER_MESSAGE_TYPE, lu16Frame);
    }

    // BrnNetwork::BurningHomeRunSwitchRunnerMessage::Retrieve @ 0x8257F2A8
    // Copies the new-runner id out into the caller's buffer only when the inherited VALID
    // flag is set, then clears it. Returns whether anything was retrieved.
    bool BurningHomeRunSwitchRunnerMessage::Retrieve(NetworkPlayerID* lpNewRunnerID)
    {
        CGS_ASSERT(lpNewRunnerID != 0, "lpNewRunnerID");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        *lpNewRunnerID = mNewRunnerID;   // *a2 = *(+0x28)
        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        return true;
    }
}
