// ============================================================================
// b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkOutPlayerEvents.cpp
// ============================================================================
// SetNetworkPlayerID bodies for the three network-out player lifecycle events
// (NetworkOutPlayerAddedEvent / NetworkOutPlayerFinalisedEvent / NetworkOutPlayerRemovedEvent).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (no leak / no DWARF):
//   NetworkOutPlayerAddedEvent::SetNetworkPlayerID     @ 0x825810E0  -> stw r31, 0x10(r30)
//   NetworkOutPlayerFinalisedEvent::SetNetworkPlayerID @ 0x82581140  -> stw r31, 0(r30)
//   NetworkOutPlayerRemovedEvent::SetNetworkPlayerID   @ 0x825811A0  -> stw r31, 0(r30)
//
// All three are the same shape: compare the incoming id against -1
// (CgsNetwork::K_INVALID_PLAYER_ID) and, on equality, fire the
// "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID" assert; then store the id word at the
// member's offset (+0x10 for PlayerAdded, +0 for the other two). The streamed file/line
// literals are dropped per project convention; the guard + store are reproduced exactly.

#include "GameSource/Network/SharedIO/BrnNetworkOutPlayerEvents.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    // CgsNetwork::K_INVALID_PLAYER_ID has no committed shared home yet; the X360 compares the
    // incoming id against the literal -1 (cmpwi r31, -1). Modelled as the same local sentinel
    // other BrnNetwork TUs use (BrnGameActions.cpp). Replace with the real CgsNetwork constant
    // when that header is reconstructed (value is the same -1).
    static const NetworkPlayerID KI_INVALID_PLAYER_ID = -1;

    // X360 0x825810E0. Store the player id at +0x10, guarding against the invalid-id sentinel.
    void NetworkOutPlayerAddedEvent::SetNetworkPlayerID(NetworkPlayerID lNetworkPlayerID)
    {
        CGS_ASSERT(lNetworkPlayerID != KI_INVALID_PLAYER_ID,
                   "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
        mNetworkPlayerID = lNetworkPlayerID;
    }

    // X360 0x82581140. Store the player id at +0, guarding against the invalid-id sentinel.
    void NetworkOutPlayerFinalisedEvent::SetNetworkPlayerID(NetworkPlayerID lNetworkPlayerID)
    {
        CGS_ASSERT(lNetworkPlayerID != KI_INVALID_PLAYER_ID,
                   "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
        mNetworkPlayerID = lNetworkPlayerID;
    }

    // X360 0x825811A0. Store the player id at +0, guarding against the invalid-id sentinel.
    void NetworkOutPlayerRemovedEvent::SetNetworkPlayerID(NetworkPlayerID lNetworkPlayerID)
    {
        CGS_ASSERT(lNetworkPlayerID != KI_INVALID_PLAYER_ID,
                   "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
        mNetworkPlayerID = lNetworkPlayerID;
    }
}
}
