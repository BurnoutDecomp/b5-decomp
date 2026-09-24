#include "GameSource/Sound/Passby/BrnPassbyState.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// =============================================================================
// BrnSound::Logic::Passby::PassbyState — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnPassbyState.h for the
// inheritance rationale and the un-recoverable-descriptor-fields FLAG.
//
// This TU's recon'd function set:
//   GetStaticTypeInfo()  @ 0x82688FC8
//   PassbyState()        @ 0x826BF5E0
//   Attach(void*)        @ 0x826D4A98
//   UpdateParams(f32)    @ 0x826D4B30
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Passby
{

// ---------------------------------------------------------------------------
// PassbyState::PassbyState  @ 0x826BF5E0  (moved from the GameShared CgsState.cpp
// rival, 2026-08-25 audio-faithfulness wave 5)
//
// Zeroes/seeds the embedded passby record field-for-field (the rival's numeric
// tail decoded onto the DWARF Passby members):
//   stvx128 vZero, +96   -> mPassbyData.mStaticPos            (16-byte vector zero)
//   stw 0,  +112         -> mPassbyData.mp3dControl
//   stfs 0, +116         -> mPassbyData.mfRelativeVelocityMagnitude
//   stw 3,  +120         -> mPassbyData.meType = 3
//   stfs 1.0, +124       -> mPassbyData.mfVolumeModifier      (flt_82001C98)
//   stb 0,  +128         -> mPassbyData.mbSuppressBoostBys
// mfTimeOutTimer (+144) is NOT stored by the X360 ctor; value-defined here on the
// host (0.0f) so the member is never read uninitialised -- no behavioural
// reliance on the pre-seed value is attested.
// ---------------------------------------------------------------------------
PassbyState::PassbyState()
    : BrnSound::Logic::BrnState()
    , mfTimeOutTimer(0.0f)
{
    mPassbyData.mStaticPos.SetZero();                       // stvx128 zero, +96
    mPassbyData.mp3dControl                 = 0;            // stw 0, +112
    mPassbyData.mfRelativeVelocityMagnitude = 0.0f;         // stfs 0, +116
    mPassbyData.meType = AttribSys::Enums::ePassbyTypes::TrafficSmall; // stw 3, +120
    mPassbyData.mfVolumeModifier            = 1.0f;         // stfs flt_82001C98, +124
    mPassbyData.mbSuppressBoostBys          = false;        // stb 0, +128
}

// ---------------------------------------------------------------------------
// GetStaticTypeInfo  @ 0x82688FC8
//
//   lis   r11, unk_82F2F95C@ha
//   addi  r3,  r11, unk_82F2F95C@l   ; r3 = &sTypeInfo
//   blr
//
// Returns the address of PassbyState's per-class static RTTI descriptor. The
// X360 builds this descriptor in rodata (at 0x82F2F95C). Here it is a
// function-local static, seeded with the canonical type name "PassbyState" and
// the real registration id recovered from the PS3 DecFIGS build:
//   ObjectID        = 0x40000  (PS3 __static_initialization_and_destruction_0
//                               @ 0x85FA1C: PassbyState::sTypeInfo.ObjectID = 0x40000)
//   mpcTypeName     = "PassbyState"
//   mpBaseTypeInfo  = nullptr  (FLAG: BrnState's descriptor chain is DEFERRED;
//                               PS3 sets baseTypeInfo = &BrnState::sTypeInfo, but
//                               BrnState's descriptor is not homed in this TU)
//   mpfnCreateObject= nullptr  (CreateObject @ 0x826D4978 is its own DEFERRED slice)
// This mirrors the committed CgsEffectBase.cpp GetStaticTypeInfo precedent: the
// observable return — a stable &sTypeInfo whose typeName is the recovered class
// name — matches, without fabricating the descriptor's id/base wiring.
//
// NOTE: BrnState.h's minimal CgsSound::Logic::ClassTypeInfo<T> has no constructor
// (aggregate members ObjectID / mpcTypeName / mpBaseTypeInfo / mpfnCreateObject),
// so the descriptor is aggregate-initialised here.
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* PassbyState::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State> sTypeInfo =
    {
        0x40000,       // ObjectID        (PS3 0x85FA1C: PassbyState id = 0x40000)
        "PassbyState", // mpcTypeName
        nullptr,       // mpBaseTypeInfo  (DEFERRED -- BrnState descriptor chain)
        &PassbyState::CreateObject,  // mpfnCreateObject @ 0x826D4978
    };
    return &sTypeInfo;
}

// ---------------------------------------------------------------------------
// PassbyState::CreateObject(u32)  @ 0x826D4978  (the factory hook)
//
// The X360 allocates the state through CgsSound::MemBase::operator new tagged
// "PassbyState" (the pool tag named in BrnPassbyState.h:35) and placement-
// constructs into it; the `u32` argument is the operator-new flavour selector,
// not `this`. Same treatment as every committed sibling (StreamingState,
// CollisionState, EmitterState): the host `new` stands in for the sound
// allocator, and the observable result -- a constructed PassbyState* handed back
// to StateManager::CreateState -- matches.
// ---------------------------------------------------------------------------
CgsSound::Logic::State* PassbyState::CreateObject(u32 /*auType*/)
{
    return new PassbyState();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* PassbyState::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

// BrnPassbyState.cpp:23 (DWARF `const float32_t KF_TIMEOUT_TIMER`): flt_820ABCD8, 5.0f.
static const f32 KF_TIMEOUT_TIMER = 5.0f;

// ---------------------------------------------------------------------------
// PassbyState::Attach(void*)  @ 0x826D4A98  (DWARF cpp:73)
//
//   assert lpvAttachment                        ; "lpvAttachment" (cpp:75, li r5, 0x4B)
//   mPassbyData = *(Passby*)lpvAttachment       ; 6 x ld/std -- the 48-byte record (+0x60)
//   mfTimeOutTimer = 0.0f                       ; stfs flt_82001CC0, 0x90
//   State::Attach(lpvAttachment)                ; bl 0x826C4B88
//
// The attachment is the manager's maPostedPassbys[i] slot, which UpdateParams recycles the
// same frame (the count is zeroed after the dispatch loop) -- hence the copy.
// ---------------------------------------------------------------------------
void PassbyState::Attach(void* apvAttachment)
{
    CGS_ASSERT(apvAttachment != nullptr, "lpvAttachment");
    mPassbyData = *static_cast<const PassbyStateManager::Passby*>(apvAttachment);
    mfTimeOutTimer = 0.0f;
    CgsSound::Logic::State::Attach(apvAttachment);
}

// ---------------------------------------------------------------------------
// PassbyState::UpdateParams(f32)  @ 0x826D4B30  (DWARF cpp:94)
//
//   State::UpdateParams(dt)                     ; bl 0x826C4850
//   mbIsAttached (+0x48):
//       mfTimeOutTimer += dt                    ; fadds
//       mfTimeOutTimer > KF_TIMEOUT_TIMER -> Detach()   ; `fcmpu ; ble` -- a NaN timer holds
// ---------------------------------------------------------------------------
void PassbyState::UpdateParams(f32 afDeltaTime)
{
    CgsSound::Logic::State::UpdateParams(afDeltaTime);
    if (!IsAttached())
        return;
    mfTimeOutTimer += afDeltaTime;
    if (mfTimeOutTimer > KF_TIMEOUT_TIMER)
        Detach();
}

const char* PassbyState::GetTypeName() const
{
    return "PassbyState";
}

// ---------------------------------------------------------------------------
// File-scope registration. WITHOUT THIS the descriptor exists but is invisible:
// CgsSound::Logic::StateManager::CreateState @0x826A595C walks the registry
// (State::GetRegisteredTypeInfo) and stops at the first null slot, so an
// unregistered PassbyState makes PrepareStates fire "Failed to find State Object"
// (CgsStateManager.cpp:283) once per requested instance and create NOTHING --
// measured 8+8 asserts on the first run that reached this code. Same shape and
// same call as the committed StreamingState / CollisionState / EmitterState
// registrations.
// ---------------------------------------------------------------------------
static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* const gpPassbyStateReg =
    CgsSound::Logic::State::AddToClassTypeInfoArray(PassbyState::GetStaticTypeInfo());

} // namespace Passby
} // namespace Logic
} // namespace BrnSound
