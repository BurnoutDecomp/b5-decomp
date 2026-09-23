#pragma once

// ===================================================================================
// BrnNetwork::StuntMultiplierMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnStuntMultiplierMessage.h
//
// A RELIABLE per-event message carrying one player's current stunt multiplier (plus the
// frame the multiplier was sampled on). Derives from CgsNetwork::ReliableMessage.
//
// This is an X360-ONLY message type: it is present in BURNOUT_X360_ARTIST.XEX (referenced
// by BrnNetwork::BrnNetworkPlayer's SendStuntMultiplierMessage @ 0x82582720 and the
// _StuntMultiplierMessageArrivedCallback @ 0x82594668) but does NOT appear anywhere in the
// reference type dumps, so the leaf layout below is read from the message's own bodies
// (BrnStuntMultiplierMessage.cpp).
//
// SIGNATURES are X360-AUTHORITATIVE, read from the call-site register usage:
//   * PrepareForSend: the 16-bit frame id, the frames-since-start stamp, then the whole
//       8-byte MultiplierInfo record by value (one 64-bit register).
//   * Retrieve (called @ 0x825946B8):
//       r4 -> s32* out (a 4-byte stack slot, var_40),
//       r5 -> s64* out (an 8-byte stack slot, var_38); returns a bool success.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"

namespace BrnNetwork
{
    // The X360 reliable-message type id for stunt-multiplier messages (ReliableMessage::
    // PrepareForSend's leType arg; li r5,0x2A in PrepareForSend @ 0x8257D2F8).
    const s32 KI_STUNT_MULTIPLIER_MESSAGE_TYPE = 42;

    // Leaf layout (after the 0x28 ReliableMessage base). The X360 stores the whole 8-byte
    // multiplier-info blob with one std at +0x28 and the frames-since-start with a stw at +0x30:
    //   +0x28  MultiplierInfo mMultiplierInfo (8 bytes, 4-aligned: the class is 0x34 long)
    //            (+0x28 u32 stunt-types [1,0x3FFFF]; +0x2C u16 flat-spins [0,16]; +0x2E u16 barrel-rolls [0,8])
    //   +0x30  s32 miFramesSinceStart   (packed [0, 0x7FFFFFFF])
    struct StuntMultiplierMessage : CgsNetwork::ReliableMessage
    {
        // The 8-byte multiplier record the message carries. The console moves it as one
        // 64-bit word; here it crosses the s64 interface below by a byte copy.
        struct MultiplierInfo
        {
            u32 muMultiplierStuntTypes;   // +0x00
            u16 mu16FlatSpins;            // +0x04
            u16 mu16BarrelRolls;          // +0x06
        };

        MultiplierInfo mMultiplierInfo;       // +0x28
        s32            miFramesSinceStart;    // +0x30

        // @ 0x8257D2E8 -- header/name accessor.
        const char*                    GetName() const override;
        // @ 0x8257D210 -- zero every leaf field then chain to the base packed-size.
        s32                            GetPackedMessageSize() override;
        // @ 0x8257D228 -- (de)serialise the four leaf fields; OR the per-field statuses.
        CgsNetwork::PackOrUnpackResult PackOrUnpack() override;

        // Stamp this send-slot with the player's current multiplier record sampled on the
        // given frame; the record is stored whole at +0x28.
        void PrepareForSend(u16 lu16FrameCount, s32 liFramesSinceStart, MultiplierInfo lMultiplierInfo);

        // Unpack the received multiplier into the two out-slots the arrived-callback hands in.
        // Returns true when a valid multiplier was retrieved. @ 0x82580020.
        bool Retrieve(s32* lpiOut, s64* lpi64Out);
    };

    // Console size (the RegisterMessageType length), checked on a 32-bit build.
    static_assert(sizeof(void*) != 4 || offsetof(StuntMultiplierMessage, mMultiplierInfo) == 0x28,
                  "StuntMultiplierMessage::mMultiplierInfo @ +0x28");
    static_assert(sizeof(void*) != 4 || offsetof(StuntMultiplierMessage, miFramesSinceStart) == 0x30,
                  "StuntMultiplierMessage::miFramesSinceStart @ +0x30");
    static_assert(sizeof(void*) != 4 || sizeof(StuntMultiplierMessage) == 0x34, "sizeof(StuntMultiplierMessage) == 0x34");
} // namespace BrnNetwork
