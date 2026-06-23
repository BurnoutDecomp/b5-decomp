#include "GameSource/Network/Messages/BrnLiveRevengeSyncMessage.h"

#include "GameShared/GameClasses/Network/Packeting/Messages/CgsTestConnectionMessage.h" // base packed-size delegate

// BrnNetwork::LiveRevengeSyncMessage -- definition home for the message's clear/size path.
//
// @ 0x8257B4E8  BrnNetwork::LiveRevengeSyncMessage::GetPackedMessageSize
//   The X360 body reaches the embedded relationship at this+0x28 (== sizeof(ReliableMessage),
//   so mLiveRevengeRelationship sits immediately after the 0x28-byte base) and zeroes exactly
//   the LiveRevengeRelationship clear field-set -- the 18-word mOverallStats block (+0..+0x44
//   of the relationship), mLastTimeChanged's two FILETIME halves (+0x4C/+0x50), the un-homed
//   mUniqueID byte (+0x58) and 8-byte field (+0x68), and the two trailing ints (+0x70/+0x74) --
//   then TAIL-CALLS the shared base packed-size computation
//   (b CgsNetwork::TestConnectionMessage::GetPackedMessageSize, the deduped base-message
//   size thunk; TestConnectionMessage and this message share the same 0x28-byte
//   CgsNetwork::ReliableMessage base). The clear is expressed by name through the embedded
//   relationship's own Release() (which performs that identical store set); the size is
//   returned via the inherited base computation by name (the X360 reuses the
//   TestConnectionMessage body as that base thunk).
namespace BrnNetwork
{
    s32 LiveRevengeSyncMessage::GetPackedMessageSize()
    {
        // Clear the embedded payload relationship by name (same store set as the X360
        // in-place zero of this+0x28).
        mLiveRevengeRelationship.Release();

        // Return the base reliable-message packed size (the X360 tail-call's shared thunk).
        return ReliableMessage::GetPackedMessageSize();
    }
}
