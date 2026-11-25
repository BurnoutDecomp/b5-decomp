#include "types.hpp"
#include "GameSource/Network/Messages/BrnStuntMultiplierMessage.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

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
// stw/lwz at +0x30.

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Network/Messages/BrnStuntMultiplierMessage.cpp".

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
            CgsNetwork::Message::PackOrUnpack(&mMultiplierInfo.mu16BarrelRolls, 0, 8) | lxTypes;
        const CgsNetwork::PackOrUnpackResult lxFlatSpins =
            CgsNetwork::Message::PackOrUnpack(&mMultiplierInfo.mu16FlatSpins, 0, 16);
        return CgsNetwork::Message::PackOrUnpack(&miFramesSinceStart, 0, 0x7FFFFFFF)
             | (lxFlatSpins | lxBarrelRolls);
    }

    void StuntMultiplierMessage::PrepareForSend(u16 lu16FrameCount, s32 liFramesSinceStart,
                                                MultiplierInfo lMultiplierInfo)
    {
        CGS_ASSERT(lu16FrameCount != CgsNetwork::KU16_INVALID_FRAME,
            CGS_ASSERT_MESSAGE_AT(
                "lu16FrameCount != KU16_INVALID_FRAME",
                __FILE__, 145 /* ARTIST 0x8257D334 */));
        CGS_ASSERT(lMultiplierInfo.muMultiplierStuntTypes > 0,
            CGS_ASSERT_MESSAGE_AT(
                "lMultiplierInfo.muMultiplierStuntTypes > 0",
                __FILE__, 146 /* ARTIST 0x8257D35C */));
        CGS_ASSERT(lMultiplierInfo.muMultiplierStuntTypes <= 0x3FFFFu,
            CGS_ASSERT_MESSAGE_AT(
                "lMultiplierInfo.muMultiplierStuntTypes <= static_cast<uint32_t>( KI_MAX_MULTIPLIER_STUNT_TYPES )",
                __FILE__, 147 /* ARTIST 0x8257D388 */));
        CGS_ASSERT(lMultiplierInfo.mu16BarrelRolls <= 8u,
            CGS_ASSERT_MESSAGE_AT(
                "lMultiplierInfo.muBarrelRolls <= static_cast<uint32_t>( KI_MAX_BARREL_ROLLS )",
                __FILE__, 148 /* ARTIST 0x8257D3B0 */));
        CGS_ASSERT(lMultiplierInfo.mu16FlatSpins <= 16u,
            CGS_ASSERT_MESSAGE_AT(
                "lMultiplierInfo.muFlatSpins <= static_cast<uint32_t>( KI_MAX_FLAT_SPINS )",
                __FILE__, 149 /* ARTIST 0x8257D3D8 */));
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0,
            CGS_ASSERT_MESSAGE_AT(
                "!CgsNetwork::ReliableMessage::IsMessageValid()",
                __FILE__, 152 /* ARTIST 0x8257D404 */));

        mMultiplierInfo    = lMultiplierInfo;      // the whole 8-byte record, +0x28
        miFramesSinceStart = liFramesSinceStart;   // +0x30
        CgsNetwork::ReliableMessage::PrepareForSend(KI_STUNT_MULTIPLIER_MESSAGE_TYPE, lu16FrameCount);
    }

    bool StuntMultiplierMessage::Retrieve(s32* lpiOut, s64* lpi64Out)
    {
        // ARTIST 0x82580030 branches to a trailing `return false` (0x82580064): the copy-out is the body
        // of the valid test.
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
        {
            memcpy(lpi64Out, &mMultiplierInfo, sizeof(mMultiplierInfo));
            *lpiOut   = miFramesSinceStart;
            mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;   // clrrwi + stb: clear VALID bit
            return true;
        }
        return false;
    }
} // namespace BrnNetwork
