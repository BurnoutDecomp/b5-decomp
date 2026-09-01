#include "GameSource/Sound/Traffic/BrnTrafficState.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// =============================================================================
// BrnSound::Logic::Traffic::TrafficState -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnTrafficState.h for the
// inheritance rationale.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

// ---------------------------------------------------------------------------
// ~TrafficState  @ 0x826CB1D0  (vector deleting destructor)
//
//   stw  off_820AE1F4, 0(this)               ; install TrafficState's own vtable
//   bl   CgsSound::Logic::State::DestroyEffects   ; (this in r3) tear down effects
//   stw  off_820AA820, 0(this)               ; re-install the MemBase base vtable
//   if (flags & 1)                           ; deleting flavour
//       <sound allocator>.Free(this)         ; via off_82FFB954, vtable slot +0x14
//
// Identical shape (same off_820AE1F4 own-vtable install / off_820AA820 MemBase
// base re-install pair) to the sibling GlobalState::~GlobalState @ 0x826D2250
// (BrnGlobalState.cpp) -- both are BrnState-derived leaves whose only observable
// source-level side effect is the DestroyEffects() call on the State base (reused
// BY NAME, declaration-only in BrnState.h). The two vtable installs and the
// conditional allocator-routed free are the compiler-synthesised parts of MSVC's
// vector-deleting-destructor thunk, re-emitted here from this virtual destructor +
// the class's operator delete; off_82FFB954 (the sound allocator) is not homed in
// this group, so the host toolchain's `delete` stands in for the custom-allocator
// dispatch.
//
// FLAG: State::DestroyEffects() is declaration-only in BrnState.h (its own body is
// a separate un-homed sound-logic recon slice). It is called BY NAME here to match
// the X360 `bl` exactly; no body is fabricated for it.
// ---------------------------------------------------------------------------
TrafficState::~TrafficState()
{
    DestroyEffects();
}

// ---------------------------------------------------------------------------
// Attach  @ 0x826CB270
//
//   cmplwi cr6, r31, 0                 ; if (lpvAttachment == 0)
//   bne    cr6, <store>
//   bl     BeginAssert / FireAssert("lpvAttachment",...) / EndAssert
//   stw    r31, 0x54(r30)              ; mpTrafficEntity = lpvAttachment
//   mr     r3, r30 ; mr r4, r31
//   bl     CgsSound::Logic::State::Attach   ; base Attach(this, lpvAttachment)
//
// Null-asserts the attachment, records it in the +0x54 derived member
// (mpTrafficEntity), then chains to the base State::Attach (reused BY NAME, the
// same idiom as the committed sibling destructors' DestroyEffects() call).
// CGS_ASSERT folds the Begin/Fire/End sequence; the baked d:\p4 file/line is dropped.
// ---------------------------------------------------------------------------
void TrafficState::Attach(void* lpvAttachment)
{
    CGS_ASSERT(lpvAttachment != 0, "lpvAttachment");
    mpTrafficEntity = static_cast<const BrnTraffic::BrnTrafficIO::TrafficSoundEntity*>(lpvAttachment);
    State::Attach(lpvAttachment);
}

// ---------------------------------------------------------------------------
// GetTypeName  @ 0x826841B8
//
//   lwz r3, (off_82F2E8F4)   ; r3 = "TrafficState"
//   blr
//
// Returns the per-class RTTI type name. Mirrors the committed sibling
// GlobalState::GetTypeName (@ 0x82686808).
// ---------------------------------------------------------------------------
const char* TrafficState::GetTypeName() const
{
    return "TrafficState";
}

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound
