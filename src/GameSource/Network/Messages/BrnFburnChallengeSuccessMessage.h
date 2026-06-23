#pragma once

// ===================================================================================
// BrnNetwork::FburnChallengeSuccessMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnFburnChallengeSuccessMessage.h
//
// SHAPE from DecFIGS DWARF (BrnFburnChallengeSuccessMessage.h:45) gated against the X360
// binary. A reliable message reporting a two-action challenge-success update for one
// freeburn frame: a pair of action scores, a pair of "succeeded this frame" flags, a
// pair of "accumulated this frame" flags, plus a frame-since-start counter.
//
// LAYOUT (X360-AUTHORITATIVE offsets; Construct @ 0x8257C940, PrepareForSend @ 0x8257CB28,
//          Retrieve @ 0x8257FB98):
//   +0x00  (CgsNetwork::ReliableMessage base, size 0x28; +0x20/+0x24 player ids -> -1)
//   +0x28  f32  mafActionScores[2]          (stfs -1.0 each at +0x28/+0x2C)
//   +0x30  s32  miFramesSinceStart          (stw 0; PackOrUnpack int [0, 0x7FFFFFFF])
//   +0x34  bool mabSuccessfulActions[2]      (stb 0 at +0x34/+0x35)
//   +0x36  bool mabAccumulationThisFrame[2]  (stb 0 at +0x36/+0x37)
//
// NOTE: miFramesSinceStart at +0x30 is X360-AUTHORITATIVE (Construct stw 0, PrepareForSend
// stw arg, Retrieve lwz -> *out, PackOrUnpack int [0, 0x7FFFFFFF]); the DWARF member list
// omitted it (and the DWARF PrepareForSend/Retrieve signatures dropped the matching int
// param). The asm is authoritative, so it is modelled here by name at +0x30 and threaded
// through PrepareForSend / Retrieve as the leading int param the asm passes.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsReliableMessage.h"

namespace BrnNetwork
{
    struct FburnChallengeSuccessMessage : CgsNetwork::ReliableMessage
    {
        f32  mafActionScores[2];          // +0x28
        s32  miFramesSinceStart;          // +0x30 (X360-authoritative; DWARF omitted it)
        bool mabSuccessfulActions[2];      // +0x34
        bool mabAccumulationThisFrame[2];  // +0x36

        void                          Construct();
        void                          Destruct();
        void                          PrepareForSend(u16 lu16FrameCount,
                                                     s32 liFramesSinceStart,
                                                     const bool* lpaSuccessfulActions,
                                                     const bool* lpaAccumulationsThisFrame,
                                                     const f32* lpaActionScores);
        bool                          Retrieve(s32* lpiFramesSinceStart,
                                              bool* lpaSuccessfulActions,
                                              bool* lpaAccumulationsThisFrame,
                                              f32* lpaActionScores);
        bool                          OldMessagesAreValid() const;
        s32                           GetPackedMessageSize();
        CgsNetwork::PackOrUnpackResult PackOrUnpack();
        const char*                   GetName() const;
    };
} // namespace BrnNetwork
