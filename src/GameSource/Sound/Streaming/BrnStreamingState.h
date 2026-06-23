#ifndef BRN_SOUND_LOGIC_STREAMING_STREAMING_STATE_H
#define BRN_SOUND_LOGIC_STREAMING_STREAMING_STATE_H

#include "types.hpp"

// =============================================================================
// BrnSound::Logic::Streaming::StreamingState (+ the Request payload it owns)
//   GameSource/Sound/Streaming/BrnStreamingState.h (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// StreamingState is the per-stream state record owned by a StreamingEffect. The
// effect's GetRequest() accessor (this TU @ 0x82683B40) asserts the state is
// attached (StreamingState::IsAttached(), BrnStreamingState.h:168 -- a bool at
// +0x48 read as a byte) and returns a reference to the state's embedded Request
// at +0x54.
//
// This is a MINIMAL FLAGGED HOME: only the two members the GetRequest accessor
// touches (the IsAttached flag at +0x48 and the Request payload at +0x54) are
// materialised, plus the leading span up to them so the offsets are honest. The
// full StreamingState surface (voice/content/load bookkeeping) is DEFERRED to its
// own TUs, which grow this header ADDITIVELY.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Streaming
{

// BrnStreamingState.h. The pending stream request carried by a StreamingState.
// GetRequest() callers read `.mpAttachment` first (asserted "GetRequest().mpAttachment",
// BrnStreamingEffect.cpp), so mpAttachment is the leading member. ProcessUpdate
// block-copies the whole Request as six 4-byte words (24 bytes) when re-issuing it,
// so the payload is 24 bytes wide.
//
// FLAG (un-DWARF'd field names beyond mpAttachment): only mpAttachment (the +0
// member the asm/callers prove) is named from evidence; the remaining payload is
// modelled as an honest 20-byte span at the asm-observed width (the 6-word copy),
// NOT as guessed concrete members. Grow additively when the Request TUs land.
struct StreamingRequest
{
    void* mpAttachment;          // +0x00 (asserted "GetRequest().mpAttachment")
    u8    maPayload[20];         // +0x04..0x17 (rest of the 6-word / 24-byte copy)
};

// BrnStreamingState.h. Per-stream state record.
//
// FLAG: MINIMAL FLAGGED HOME. Only the attached-flag (+0x48) and the embedded
// Request (+0x54) are materialised; the leading span is an honest opaque region
// at the asm-observed offsets so IsAttached()/GetRequest reach their members at
// the right place by NAME. Field types in the leading span are UNVERIFIED and
// DEFERRED.
struct StreamingState
{
    // BrnStreamingState.h:168 (DWARF assert site). True once the stream has been
    // attached to a voice. Read as a byte at +0x48 by GetRequest's IsAttached()
    // guard (`lbz r11, 0x48(state)`).
    bool IsAttached() const { return mbAttached; }

    StreamingRequest& GetRequest() { return mRequest; }

    // -- FLAGGED layout (offsets are X360 facts; leading-span types DEFERRED) --
    u8               maLeading[0x48];   // +0x00..0x47 (opaque: voice/content/load state)
    bool             mbAttached;        // +0x48 (IsAttached flag)
    u8               maGap0x49[0x0B];   // +0x49..0x53 (padding/other flags)
    StreamingRequest mRequest;          // +0x54 (returned by GetRequest)
};

} // namespace Streaming
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_STREAMING_STREAMING_STATE_H
