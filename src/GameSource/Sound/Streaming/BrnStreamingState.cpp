#include "GameSource/Sound/Streaming/BrnStreamingState.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (IsAttached tripwire)

// =============================================================================
// BrnSound::Logic::Streaming::StreamingState -- out-of-line bodies
// (ledger id class:BrnSound::Logic::Streaming::StreamingState):
//   StreamingState::StreamingState  @ 0x826B0CB0  (moved from the CgsState.cpp rival)
//   StreamingState::~StreamingState @ 0x826C9B28  (moved from the CgsState.cpp rival)
//   StreamingState::Get             @ 0x82683A00
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnStreamingState.h for the
// DWARF-reconciled layout.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Streaming
{

// ---------------------------------------------------------------------------
// StreamingState::StreamingState  @ 0x826B0CB0
//
// Zeroes the embedded request field-for-field -- the ctor's five word stores +
// one byte store land exactly on StreamRequest's members at +84..+104:
//   stw 0, +84  -> mRequest.mpAttachment
//   stw 0, +88  -> mRequest.mu32Priority
//   stfs 0,+92  -> mRequest.mfLagTolerance
//   stfs 0,+96  -> mRequest.mfTimeStamp
//   stw 0, +100 -> mRequest.mu32UniqueId
//   stb 0, +104 -> mRequest.mbDirty
// mfFadeOut (+108) is NOT stored by the X360 ctor; it is value-defined here on
// the host (0.0f) so the member is never read uninitialised -- no behavioural
// reliance on the pre-SetFadeOut value is attested.
// ---------------------------------------------------------------------------
StreamingState::StreamingState()
    : BrnSound::Logic::BrnState()
    , mfFadeOut(0.0f)
{
    mRequest.mpAttachment   = 0;      // stw 0, +84
    mRequest.mu32Priority   = 0u;     // stw 0, +88
    mRequest.mfLagTolerance = 0.0f;   // stfs 0, +92
    mRequest.mfTimeStamp    = 0.0f;   // stfs 0, +96
    mRequest.mu32UniqueId   = 0u;     // stw 0, +100
    mRequest.mbDirty        = false;  // stb 0, +104
}

// ---------------------------------------------------------------------------
// StreamingState::~StreamingState  @ 0x826C9B28  (scalar deleting destructor)
//
//   stw  off_820AE1F4, 0(this)                 ; install StreamingState's own vtable
//   bl   CgsSound::Logic::State::DestroyEffects ; (this in r3) tear down attached effects
//   stw  off_820AA820, 0(this)                  ; re-install the MemBase base vtable
//   if (flags & 1)                              ; deleting flavour
//       <sound allocator>.Free(this)            ; via off_82FFB954, vtable slot +0x14
//   return this
//
// Mirrors the committed sibling BrnSound::Logic::GlobalState scalar deleting
// destructor (BrnGlobalState.cpp @ 0x826D2250): the two vtable installs and the
// conditional allocator-routed free (off_82FFB954, vtable slot +0x14) are MSVC's
// compiler-synthesised deleting-destructor thunk, re-emitted from this virtual
// destructor + operator delete -- NOT hand-written. The single observable
// source-level side effect is the DestroyEffects() call on the State base, reused
// BY NAME. State::DestroyEffects() body is un-homed (a separate sound-logic recon
// slice, recovered call target CgsSound::Logic::State::DestroyEffects @ the
// 0x826C9B50 bl); declaration-only in CgsState.h -- no body is fabricated here.
// ---------------------------------------------------------------------------
StreamingState::~StreamingState()
{
    DestroyEffects();
}

// ---------------------------------------------------------------------------
// StreamingState::Get  @ 0x82683A00  (DWARF BrnStreamingState.h:166)
//
//   lbz    r11, 0x48(state)                  ; State::mbIsAttached (IsAttached())
//   if (!r11) assert("IsAttached()", BrnStreamingState.h:168)   ; non-gating
//   addi   r3, state, 0x54                   ; return &state->mRequest
//   blr
//
// Returns a reference to the embedded request at +0x54, after asserting the
// state is attached. The assert is the CGS_ASSERT-vacuous tripwire (Begin/Fire
// /EndAssert de-inlined by Hex-Rays collapse to one CGS_ASSERT here).
// ---------------------------------------------------------------------------
const StreamRequest& StreamingState::GetRequest() const
{
    CGS_ASSERT( IsAttached(), "IsAttached()" );
    return mRequest;
}

} // namespace Streaming
} // namespace Logic
} // namespace BrnSound
