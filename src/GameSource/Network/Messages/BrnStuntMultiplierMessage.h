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
    struct StuntMultiplierMessage : CgsNetwork::ReliableMessage
    {
        // Stamp this send-slot with the player's current multiplier sampled on the given
        // frame. Declared-only (body lands with this message type's own TU).
        void PrepareForSend(u16 lu16FrameCount, s32 liFramesSinceStart, s32 liStuntMultiplier);

        // Unpack the received multiplier into the two out-slots the arrived-callback hands in.
        // Returns true when a valid multiplier was retrieved. Declared-only.
        bool Retrieve(s32* lpiOut, s64* lpi64Out);
    };
} // namespace BrnNetwork
