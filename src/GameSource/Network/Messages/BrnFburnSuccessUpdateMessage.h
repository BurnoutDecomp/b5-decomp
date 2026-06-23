#pragma once

// ===================================================================================
// BrnNetwork::FburnSuccessUpdateMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnFburnSuccessUpdateMessage.h
//
// SHAPE from DecFIGS DWARF (BrnFburnSuccessUpdateMessage.h:45) gated against the X360
// binary. An UNRELIABLE per-frame message (CgsNetwork::Message base, size 0x20) reporting
// a last-second challenge-success bit set plus the frame/action it applies to.
//
// LAYOUT (X360-AUTHORITATIVE offsets; Construct @ 0x82580720, PackOrUnpack @ 0x8257C898,
//          PrepareForSend @ 0x8257FAC8, Retrieve @ 0x8257FB50):
//   +0x00  (CgsNetwork::Message base, size 0x20)
//   +0x20  LastSecondChallengeSuccess mSuccessBitArray  (8 bytes; std 0 in ctor)
//   +0x28  s32 miFramesSinceStart                       (stw -1; PackOrUnpack int [0, 0x7FFFFFFF])
//   +0x2C  s32 miActionIndex                            (stw -1; PackOrUnpack int [0, 2])
//
// PackOrUnpack ORs the base virtual pack/unpack status (the X360 build resolves the
// base-stub call to a COMDAT-folded copy reported as BrnWorld::PVSDebugComponent::IsSimple,
// which returns false == 0 == success) with the 8-byte buffer field and the two ints.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"

namespace BrnGameState
{
    // Owning home for the last-second challenge-success bit set the update message carries.
    // DWARF spells it FburnChallengeSuccessUpdateAction::LastSecondChallengeSuccess; it is an
    // 8-byte bitset (the X360 ctor zero-fills it with a single 64-bit std at +0x20, and
    // PrepareForSend copies it as one 64-bit load/store). No richer committed home exists, so
    // it is modelled here by name as a 64-bit-word bit store -- the only shape the message TU
    // touches. FLAG: grow this into the real action class if/when that subsystem lands (the
    // 8-byte size must not change).
    struct FburnChallengeSuccessUpdateAction
    {
        struct LastSecondChallengeSuccess
        {
            u64 mu64Bits;   // 8 bytes
        };
    };
} // namespace BrnGameState

namespace BrnNetwork
{
    typedef BrnGameState::FburnChallengeSuccessUpdateAction::LastSecondChallengeSuccess
        FburnSuccessUpdatePayload;

    struct FburnSuccessUpdateMessage : CgsNetwork::Message
    {
        FburnSuccessUpdatePayload mSuccessBitArray;     // +0x20 (8 bytes)
        s32                       miFramesSinceStart;   // +0x28
        s32                       miActionIndex;        // +0x2C

        void                          Construct();
        void                          Destruct();
        void                          PrepareForSend(u16 lu16FrameCount,
                                                     s32 liFramesSinceStart,
                                                     s32 liActionIndex,
                                                     const FburnSuccessUpdatePayload* lpSuccessBitArray);
        bool                          Retrieve(s32* lpiFramesSinceStart,
                                              s32* lpiActionIndex,
                                              FburnSuccessUpdatePayload* lpSuccessBitArray);
        s32                           GetPackedMessageSize();
        CgsNetwork::PackOrUnpackResult PackOrUnpack();
        const char*                   GetName() const;
    };
} // namespace BrnNetwork
