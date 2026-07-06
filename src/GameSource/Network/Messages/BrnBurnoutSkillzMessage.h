#pragma once

// ===================================================================================
// BrnNetwork::BurnoutSkillzMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnBurnoutSkillzMessage.h
//
// SHAPE from the DecFIGS DWARF (GameSource/Network/Messages/BrnBurnoutSkillzMessage.h:45):
//       struct BurnoutSkillzMessage : public CgsNetwork::ReliableMessage { ... }
// The X360 binary gates this: GetPackedMessageSize @ 0x8257C2F0 chains
// CgsNetwork::TestConnectionMessage::GetPackedMessageSize (bl ...TestConnectionMessage...),
// so the true immediate base is CgsNetwork::TestConnectionMessage (itself a
// ReliableMessage with no data). The DWARF's "ReliableMessage" base is the grandparent
// (Feb-2007 PS3 drift); using TestConnectionMessage keeps the GetPackedMessageSize chain
// byte-faithful and is layout-neutral
// (sizeof(TestConnectionMessage) == sizeof(ReliableMessage) == 0x28).
//
// LAYOUT (X360-authoritative, all displacements verbatim from asm):
//   +0x00  CgsNetwork::TestConnectionMessage base (size 0x28)
//          (inherited mx8Flags @ +0x19, KX8_FLAGS_VALID bit0)
//   +0x28  BrnGameState::BurnoutSkillzData mSkillzData   (56 bytes; memcpy Size 0x38)
//   +0x60  bool                            mbInitialData
//
// The +0x28 offset + 0x38-byte memcpy pin mSkillzData directly after the 0x28 base and
// confirm BurnoutSkillzData == 56 bytes (f32[14], the X360-grown array in
// BrnBurnoutSkillzData.h). mbInitialData lands at +0x60 (== 0x28 + 0x38).
//
// The X360 build models the vtable as the inherited Message::mpVTable member (no C++
// `virtual`), matching every committed message base; these are therefore plain methods.
// Reconstructed (ledger funcs for this TU):
//   GetName              @ 0x8257C330 -- inline here; returns "Burnout Skillz Message"
//   GetPackedMessageSize @ 0x8257C2F0 -- clear payload + chain base
//   PrepareForSend       @ 0x8257F6C8 -- arm the message with skillz data
//   Retrieve             @ 0x8257F780 -- drain the message on receipt
// Construct/Destruct/PackOrUnpack are bodied in their own slices; declared here so the
// hierarchy can call them by name.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsTestConnectionMessage.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnBurnoutSkillzData.h"

namespace BrnNetwork
{
    struct BurnoutSkillzMessage : CgsNetwork::TestConnectionMessage
    {
        // @ 0x8257F6C8 -- assert the inputs, then either clear a stale VALID flag or stamp
        // the initial-data marker, copy the 56-byte skillz payload, and PrepareForSend the
        // reliable message (type 31) for the given frame.
        void PrepareForSend(u16 lu16FrameCount,
                            const BrnGameState::BurnoutSkillzData* lpSkillzData,
                            bool lbInitialData);

        // @ 0x8257F780 -- if the slot is VALID, copy out the payload + initial-data flag and
        // invalidate the slot (returns true); otherwise clear the caller's data (returns false).
        bool Retrieve(BrnGameState::BurnoutSkillzData* lpSkillzData, bool* lpbInitialData);

        void Construct();
        void Destruct();

        // @ 0x8257C2F0 -- reset the payload (Clear + mbInitialData=false) and chain the
        // TestConnectionMessage base to compute the packed size.
        s32 GetPackedMessageSize();

        CgsNetwork::PackOrUnpackResult PackOrUnpack();

        // Ledger func @ 0x8257C330 -- inline header-homed accessor.
        const char* GetName() const { return "Burnout Skillz Message"; }

    private:
        BrnGameState::BurnoutSkillzData mSkillzData;    // +0x28  (56 bytes)
        bool                            mbInitialData;  // +0x60
    };
} // namespace BrnNetwork
