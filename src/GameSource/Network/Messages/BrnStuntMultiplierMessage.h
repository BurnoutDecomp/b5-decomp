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
// DecFIGS DWARF, so no leaf-member layout can be grounded here. Only the two cross-TU entry
// points the BrnNetworkPlayer TU calls are declared (declared-only -- their bodies live in
// this message type's own, not-yet-reconstructed, TU). No layout is committed.
//
// SIGNATURES are X360-AUTHORITATIVE, read from the call-site register usage:
//   * PrepareForSend (called @ 0x8258276C):
//       r4 = lu16FrameCount (clrlwi r4,..,16 -> 16-bit frame id),
//       r5 = liFramesSinceStart (the CgsNetwork::TimeManager::GetFramesSinceStart() result),
//       r6 = liStuntMultiplier (the caller's multiplier argument).
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
    //   +0x28  s64 mi64MultiplierInfo   (8-byte multiplier-info blob; the s64 payload)
    //            (+0x28 u32 stunt-types [1,0x3FFFF]; +0x2C u16 flat-spins [0,16]; +0x2E u16 barrel-rolls [0,8])
    //   +0x30  s32 miFramesSinceStart   (packed [0, 0x7FFFFFFF])
    struct StuntMultiplierMessage : CgsNetwork::ReliableMessage
    {
        s64 mi64MultiplierInfo;   // +0x28  (8-byte multiplier-info blob)
        s32 miFramesSinceStart;   // +0x30

        // @ 0x8257D2E8 -- header/name accessor.
        const char*                    GetName() const;
        // @ 0x8257D210 -- zero every leaf field then chain to the base packed-size.
        s32                            GetPackedMessageSize();
        // @ 0x8257D228 -- (de)serialise the four leaf fields; OR the per-field statuses.
        CgsNetwork::PackOrUnpackResult PackOrUnpack();

        // Stamp this send-slot with the player's current multiplier sampled on the given
        // frame. @ 0x8257D2F8.
        void PrepareForSend(u16 lu16FrameCount, s32 liFramesSinceStart, s32 liStuntMultiplier);

        // Unpack the received multiplier into the two out-slots the arrived-callback hands in.
        // Returns true when a valid multiplier was retrieved. @ 0x82580020.
        bool Retrieve(s32* lpiOut, s64* lpi64Out);
    };
} // namespace BrnNetwork
