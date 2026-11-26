#include "GameSource/Sound/Streaming/BrnStreamingState.h"
#include "GameSource/Sound/Streaming/BrnStreamingEffect.h"
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
// StreamingState::StreamingState  @ 0x826B0CB0  (DWARF BrnStreamingState.cpp:50)
//
// The base BrnState ctor, then mRequest's header-inline StreamRequest() zeroes the request
// (+0x54..+0x68). mfFadeOut (+0x6C) is not stored: ARTIST's ctor ends at the mbDirty byte.
// ---------------------------------------------------------------------------
StreamingState::StreamingState()
    : BrnSound::Logic::BrnState()
{
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
// The body is empty: ~State (CgsState.cpp) stores State's vftable and calls DestroyEffects once
// (every State-derived scalar deleting destructor in ARTIST makes exactly that one call, e.g.
// GlobalState 0x826D2278 after storing 0x820AE1F4, State's vftable).
StreamingState::~StreamingState()
{
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

void StreamingState::Attach(void* apvAttachment)
{
    CGS_ASSERT(apvAttachment != 0, "lpvAttachment");
    if (!apvAttachment)
        return;
    mRequest = *static_cast<const StreamRequest*>(apvAttachment);
    CgsSound::Logic::State::Attach(mRequest.mpAttachment);
}

bool StreamingState::Detach()
{
    if (mpHeadEffectObject)
    {
        StreamingEffect* lpEffect = static_cast<StreamingEffect*>(mpHeadEffectObject);
        if (lpEffect->GetElapsedTime() < 0.25f)
            return false;
        if (lpEffect->IsBusy())
            return false;
    }
    return CgsSound::Logic::State::Detach();
}

StreamingStateManager* StreamingState::GetStreamingStateManager() const
{
    return static_cast<StreamingStateManager*>(mpStateManager);
}

CgsSound::Logic::State* StreamingState::CreateObject(u32 luType)
{
    if (luType == 0)
        return new (sTypeInfo.typeName, false) StreamingState();
    else
        return new (sTypeInfo.typeName, true) StreamingState();
}

// ARTIST 0x82F2E83C.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State> StreamingState::sTypeInfo(0x60000, "StreamingState", &BrnState::sTypeInfo, &StreamingState::CreateObject);

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* StreamingState::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* StreamingState::GetTypeInfo() const
{
    return &sTypeInfo;
}

const char* StreamingState::GetTypeName() const
{
    return sTypeInfo.typeName;
}

} // namespace Streaming
} // namespace Logic
} // namespace BrnSound
