#include "GameSource/Sound/Collision/BrnCollisionState.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// =============================================================================
// BrnSound::Logic::Collision::CollisionState -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnCollisionState.h for the
// inheritance rationale. Same shape as the sibling BrnSound::Logic::GlobalState
// (BrnGlobalState.cpp @ 0x826D2250).
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// ---------------------------------------------------------------------------
// ~CollisionState  @ 0x826D3380  (scalar deleting destructor)
//
//   stw  off_820AE1F4, 0(this)               ; install CollisionState's own vtable
//   bl   CgsSound::Logic::State::DestroyEffects   ; (this in r3) tear down effects
//   stw  off_820AA820, 0(this)               ; re-install the MemBase base vtable
//   if (flags & 1)                           ; deleting flavour
//       <sound allocator>.Free(this)         ; via off_82FFB954, vtable slot +0x14
//
// The single observable source-level side effect is the DestroyEffects() call on
// the State base (inherited, called BY NAME). The two vtable installs and the
// conditional allocator-routed free are the compiler-synthesised parts of MSVC's
// deleting-destructor thunk, re-emitted from this virtual destructor + the class's
// operator delete; off_82FFB954 (the sound allocator) is not homed in this group,
// so the host toolchain's `delete` stands in for the custom-allocator dispatch.
// Byte-for-byte identical thunk to GlobalState's dtor @ 0x826D2250 (same
// off_820AE1F4 / off_820AA820 vtable literals, same DestroyEffects call, same
// off_82FFB954 vtable-slot-0x14 allocator-free tail).
//
// FLAG: State::DestroyEffects() is declaration-only in BrnState.h (its own body is
// a separate un-homed sound-logic recon slice). It is called BY NAME here to match
// the X360 `bl` exactly; no body is fabricated for it.
// ---------------------------------------------------------------------------
CollisionState::~CollisionState()
{
    DestroyEffects();
}

// ARTIST @0x826D3420. The supplied attachment is a complete OutputCollision;
// copy it into the state before entering the common State attach machine, then
// clear both lifetime samples and remember the state clock at attachment time.
void CollisionState::Attach(void* apvAttachment)
{
    CGS_ASSERT(apvAttachment != nullptr, "lpAttachment");
    if (!apvAttachment)
        return;

    mOutputCollision = *static_cast<const OutputCollision*>(apvAttachment);
    CgsSound::Logic::State::Attach(apvAttachment);
    meLifetime.Flush(E_NONE);
    mfTimeWeAttached = mfCurTime;
}

// ARTIST @0x826D57E0 is a direct tail-call to the common State update.
void CollisionState::UpdateParams(f32 afDeltaTime)
{
    CgsSound::Logic::State::UpdateParams(afDeltaTime);
}

bool CollisionState::Detach()
{
    const bool lbDetached = CgsSound::Logic::State::Detach();
    if (lbDetached)
        meLifetime.Update(E_NONE);
    return lbDetached;
}

// ARTIST CreateObject @ 0x826D3218 allocates and constructs one CollisionState;
// its allocator selector does not alter the constructed object. DecFIGS static-init
// @ 0x864250 pins the descriptor to ObjectID 0x50000, name "CollisionState", and
// registers it in State::ClassTypeInfoArray.
CgsSound::Logic::State* CollisionState::CreateObject(u32 /*auAllocator*/)
{
    return new CollisionState();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>*
CollisionState::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State> sTypeInfo(
        0x50000, "CollisionState", nullptr, &CollisionState::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>*
CollisionState::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* CollisionState::GetTypeName() const
{
    return "CollisionState";
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* const
    gpCollisionStateReg = CgsSound::Logic::State::AddToClassTypeInfoArray(
        CollisionState::GetStaticTypeInfo());

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
