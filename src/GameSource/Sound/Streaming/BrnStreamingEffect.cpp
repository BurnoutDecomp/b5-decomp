#include "GameSource/Sound/Streaming/BrnStreamingEffect.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (lpState / IsAttached tripwires)

// =============================================================================
// BrnSound::Logic::Streaming::StreamingEffect -- out-of-line body for the single
// ledger function owned by this TU (ledger id class:BrnSound::Logic::Streaming):
//   StreamingEffect::GetRequest  @ 0x82683B40
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// See BrnStreamingEffect.h / BrnStreamingState.h for the layout and the
// minimal-flagged-home notes.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Streaming
{

// ---------------------------------------------------------------------------
// StreamingEffect::GetRequest  @ 0x82683B40
//
//   lwz   state, 0xC(this)                 ; mpState
//   if (!state) assert("lpState", BrnStreamingEffect.cpp)   ; non-gating
//   lbz   r11, 0x48(state)                 ; state->mbAttached (IsAttached())
//   if (!r11) assert("IsAttached()", BrnStreamingState.h:168)   ; non-gating
//   addi  r3, state, 0x54                  ; return &state->mRequest
//   blr
//
// Returns a reference to the owned StreamingState's embedded Request (at state
// +0x54), after asserting the state exists and is attached. Both asserts are the
// CGS_ASSERT-vacuous tripwires (non-gating). Members reached BY NAME; the +0x54
// Request offset is held in the StreamingState layout, not asserted here across
// the host's 64-bit pointer.
// ---------------------------------------------------------------------------
StreamingRequest& StreamingEffect::GetRequest()
{
    CGS_ASSERT( mpState != nullptr, "lpState" );
    CGS_ASSERT( mpState->IsAttached(), "IsAttached()" );
    return mpState->GetRequest();
}

} // namespace Streaming
} // namespace Logic
} // namespace BrnSound
