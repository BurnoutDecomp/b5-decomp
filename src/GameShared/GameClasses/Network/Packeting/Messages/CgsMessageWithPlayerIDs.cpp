#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessageWithPlayerIDs.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::MessageWithPlayerIDs::GetSendingPlayerID @ 0x82541050
//   (called by PlayerManager::AcceptMessage / Ack/NackNeedsSending,
//    ReliableMessageManager::MessageIsDuplicate, NetworkPlayer::OnMessageUnpackedCallback,
//    TrafficManager::_HullSyncMessageArrivedCallback, ...)
//
// Returns mSendingPlayerID (+0x20). When it is negative (the KI_INVALID_PLAYER_ID == -1
// sentinel, i.e. an unset id), the X360 build first streams a diagnostic line into a global
// ostream-like debug-log sink (off_82F335C8):
//     gDebugLog << "We have received a message with sending player index " << mSendingPlayerID
//               << ", the message is of type " << mi8Type << "\n";
// (the message-type byte mi8Type is the inherited Message::mi8Type at +0x1A, read via extsb)
// and then asserts mSendingPlayerID >= 0.
//
// off_82F335C8 is an un-homed global debug-log stream this TU cannot model by name, so the
// logging side-channel is reduced to the assert it guards (the assert is the observable
// contract) -- the same precedent as CgsNetwork::Message::GetGameID @ 0x8286F3E8.
// FLAGGED: the off_82F335C8 stream-logging side effect is dropped (un-homed data-global);
// the negative-id assert is preserved store-for-store.

namespace CgsNetwork
{

MessageWithPlayerIDs::NetworkPlayerID MessageWithPlayerIDs::GetSendingPlayerID() const
{
    CGS_ASSERT(mSendingPlayerID >= 0, "mSendingPlayerID >= 0");
    return mSendingPlayerID;
}

}
