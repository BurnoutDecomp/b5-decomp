#include "types.hpp"
#include "GameSource/Network/Messages/BrnStuntMultiplierMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring>   // memcpy (the 8-byte record crosses the s64 interface)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::StuntMultiplierMessage::GetName              @ 0x8257D2E8
//   BrnNetwork::StuntMultiplierMessage::GetPackedMessageSize @ 0x8257D210
//   BrnNetwork::StuntMultiplierMessage::PackOrUnpack         @ 0x8257D228
//   BrnNetwork::StuntMultiplierMessage::PrepareForSend       @ 0x8257D2F8
//   BrnNetwork::StuntMultiplierMessage::Retrieve             @ 0x82580020
//
// A RELIABLE per-event message carrying one player's stunt-multiplier payload (an 8-byte
// multiplier-info blob at +0x28) plus the frames-since-start stamp (+0x30). The X360 body
// stores/reads the whole 8-byte blob with a single std/ld at +0x28 and the frame word with a
// stw/lwz at +0x30; the sub-field asserts in PrepareForSend are keyed on the s32 the committed
// caller (BrnNetworkPlayer::SendStuntMultiplierMessage) passes (see the header note).

namespace BrnNetwork
{
    const char* StuntMultiplierMessage::GetName() const
    {
        return "Stunt Multiplier Message";
    }

    s32 StuntMultiplierMessage::GetPackedMessageSize()
    {
        mMultiplierInfo.muMultiplierStuntTypes = 0;   // +0x28
        mMultiplierInfo.mu16BarrelRolls        = 0;   // +0x2E
        mMultiplierInfo.mu16FlatSpins          = 0;   // +0x2C
        miFramesSinceStart                     = 0;   // +0x30
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    CgsNetwork::PackOrUnpackResult StuntMultiplierMessage::PackOrUnpack()
    {
        const CgsNetwork::PackOrUnpackResult lxBase = CgsNetwork::ReliableMessage::PackOrUnpack();
        const CgsNetwork::PackOrUnpackResult lxTypes =
            CgsNetwork::PackOrUnpackUInt(this, &mMultiplierInfo.muMultiplierStuntTypes, 1, 0x3FFFF) | lxBase;
        const CgsNetwork::PackOrUnpackResult lxBarrelRolls =
            CgsNetwork::PackOrUnpackU16(this, &mMultiplierInfo.mu16BarrelRolls, 0, 8) | lxTypes;
        const CgsNetwork::PackOrUnpackResult lxFlatSpins =
            CgsNetwork::PackOrUnpackU16(this, &mMultiplierInfo.mu16FlatSpins, 0, 16);
        return CgsNetwork::PackOrUnpackInt(this, &miFramesSinceStart, 0, 0x7FFFFFFF)
             | (lxFlatSpins | lxBarrelRolls);
    }

    void StuntMultiplierMessage::PrepareForSend(u16 lu16FrameCount, s32 liFramesSinceStart,
                                                s32 liStuntMultiplier)
    {
        CGS_ASSERT(lu16FrameCount != CgsNetwork::KU16_INVALID_FRAME,
                   "lu16FrameCount != KU16_INVALID_FRAME");
        CGS_ASSERT(liStuntMultiplier > 0,
                   "lMultiplierInfo.muMultiplierStuntTypes > 0");
        CGS_ASSERT(static_cast<u32>(liStuntMultiplier) <= 0x3FFFFu,
                   "lMultiplierInfo.muMultiplierStuntTypes <= static_cast<uint32_t>( KI_MAX_MULTIPLIER_STUNT_TYPES )");
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0,
                   "!CgsNetwork::ReliableMessage::IsMessageValid()");

        const s64 li64MultiplierInfo = liStuntMultiplier;   // the whole 8-byte record
        memcpy(&mMultiplierInfo, &li64MultiplierInfo, sizeof(mMultiplierInfo));
        miFramesSinceStart = liFramesSinceStart;  // stw r27, +0x30
        CgsNetwork::ReliableMessage::PrepareForSend(KI_STUNT_MULTIPLIER_MESSAGE_TYPE, lu16FrameCount);
    }

    bool StuntMultiplierMessage::Retrieve(s32* lpiOut, s64* lpi64Out)
    {
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        memcpy(lpi64Out, &mMultiplierInfo, sizeof(mMultiplierInfo));
        *lpiOut   = miFramesSinceStart;
        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;   // clrrwi + stb: clear VALID bit
        return true;
    }
} // namespace BrnNetwork
