#ifndef BRN_SOUND_LOGIC_STREAMING_STREAMING_EFFECT_H
#define BRN_SOUND_LOGIC_STREAMING_STREAMING_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Streaming/BrnStreamingState.h"

// =============================================================================
// BrnSound::Logic::Streaming::StreamingEffect
//   GameSource/Sound/Streaming/BrnStreamingEffect.h (DWARF home) +
//   GameSource/Sound/Streaming/BrnStreamingEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// StreamingEffect is the sound-logic effect that drives a streaming voice. It owns
// (by pointer, at +0x0C) the StreamingState that holds the live stream request.
//
// This TU (ledger id class:BrnSound::Logic::Streaming) bodies ONE function:
//   BrnSound::Logic::Streaming::StreamingEffect::GetRequest  @ 0x82683B40
// The X360 symbol is rendered by IDA as the truncated "StreamingEffect_"; its two
// call sites (StreamingEffect::ProcessUpdate @ 0x826E2540 and ::Attach
// @ 0x826EE8D0) use the return value as `GetRequest().mpAttachment` (asserted at
// BrnStreamingEffect.cpp) and block-copy the returned 24-byte Request -- i.e.
// it is the GetRequest() accessor returning StreamingState::mRequest.
//
// MINIMAL FLAGGED HOME: only mpState (+0x0C) and GetRequest() are materialised; the
// rest of the StreamingEffect surface (voice wrapper, request bookkeeping, the
// RTTI/Attach/ProcessUpdate hooks) is DEFERRED to its own TUs.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Streaming
{

struct StreamingEffect
{
    // BrnStreamingEffect.cpp (DWARF assert site, "lpState") + BrnStreamingState.h:168
    // ("IsAttached()"). Asserts the owned state exists and is attached, then returns
    // a reference to the state's embedded Request. @ 0x82683B40.
    StreamingRequest& GetRequest();

    // -- FLAGGED layout (offset is an X360 fact; leading span types DEFERRED) --
    u8              maLeading[0x0C];   // +0x00..0x0B (opaque: bases / vptr / bookkeeping)
    StreamingState* mpState;           // +0x0C (asserted "lpState")
};

} // namespace Streaming
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_STREAMING_STREAMING_EFFECT_H
