#include "GameSource/Sound/Streaming/BrnStreamingState.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (IsAttached tripwire)

// =============================================================================
// BrnSound::Logic::Streaming::StreamingState -- out-of-line body for the single
// ledger function owned by this TU (ledger id class:BrnSound::Logic::Streaming::StreamingState):
//   StreamingState::Get  @ 0x82683A00
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// See BrnStreamingState.h for the layout and the minimal-flagged-home notes.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Streaming
{

// ---------------------------------------------------------------------------
// StreamingState::Get  @ 0x82683A00
//
//   lbz    r11, 0x48(state)                  ; state->mbAttached (IsAttached())
//   if (!r11) assert("IsAttached()", BrnStreamingState.h:168)   ; non-gating
//   addi   r3, state, 0x54                   ; return &state->mRequest
//   blr
//
// Returns a reference to the embedded Request at +0x54, after asserting the
// state is attached. The assert is the CGS_ASSERT-vacuous tripwire (Begin/Fire
// /EndAssert de-inlined by Hex-Rays collapse to one CGS_ASSERT here).
// ---------------------------------------------------------------------------
StreamingRequest& StreamingState::GetRequest()
{
    CGS_ASSERT( IsAttached(), "IsAttached()" );
    return mRequest;
}

} // namespace Streaming
} // namespace Logic
} // namespace BrnSound
