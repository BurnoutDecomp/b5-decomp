#include "GameSource/Sound/Passby/BrnPassbyState.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

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
// mfTimeOutTimer (+144) is NOT stored by the X360 ctor.
// ---------------------------------------------------------------------------
// DWARF BrnPassbyState.cpp:45: mPassbyData through the static-position Passby constructor
// (BrnPassbyStateManager.h:102, inline; ARTIST stores its fields through this+0x60 at 0x826BF5EC);
// mfTimeOutTimer is left unset.
PassbyState::PassbyState()
    : BrnSound::Logic::BrnState()
    , mPassbyData(Vector3(__vspltisw(0)), 0.0f, AttribSys::Enums::ePassbyTypes::TrafficSmall, false, 1.0f)
{
}

// ARTIST descriptor 0x82F2F95C.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State> PassbyState::sTypeInfo(
    0x40000, "PassbyState", &BrnState::sTypeInfo, &PassbyState::CreateObject);

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* PassbyState::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

// ---------------------------------------------------------------------------
// PassbyState::CreateObject(u32)  @ 0x826D4978  (the factory hook)
//
// The X360 allocates the state through CgsSound::MemBase::operator new tagged
// "PassbyState" (the pool tag named in BrnPassbyState.h:35) and placement-
// constructs into it; the `u32` argument is the operator-new flavour selector,
// not `this`. Same treatment as every committed sibling (StreamingState,
// CollisionState, EmitterState): tagged new uses the sound
// allocator, and the observable result -- a constructed PassbyState* handed back
// to StateManager::CreateState -- matches.
// ---------------------------------------------------------------------------
CgsSound::Logic::State* PassbyState::CreateObject(u32 luAllocator)
{
    // DWARF (BrnSoundUnity: CreateObject(uint32_t allocator)) has two new-expressions; ARTIST
    // 0x826D4978 keeps both operator new calls (permanent flag false / true).
    if (luAllocator == 0)
        return new (sTypeInfo.typeName, false) PassbyState();
    else
        return new (sTypeInfo.typeName, true) PassbyState();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* PassbyState::GetTypeInfo() const
{
    return &sTypeInfo;
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
    CGS_ASSERT_AT(apvAttachment != 0,
        "lpvAttachment",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
            "../Sound/Passby/BrnPassbyState.cpp", 75 /* ARTIST 0x826D4AC4 */);

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
    return sTypeInfo.typeName;
}

} // namespace Passby
} // namespace Logic
} // namespace BrnSound
