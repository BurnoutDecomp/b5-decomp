#include "GameSource/Sound/Vehicles/Engines/BrnDualGinsuExhaustEffect.h"
#include "GameSource/Sound/Vehicles/Engines/BrnEngineControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnHybridExhaustControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <algorithm>

// =============================================================================
// BrnSound::Vehicles::Engines::DualGinsuExhaustEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF: DualGinsuExhaustEffect : public
// DualGinsuEffect. Recon'd function set:
//   Creat(s32)                     @ 0x826E4178  (the factory hook)
//   DualGinsuExhaustEffect()       @ 0x826E0E90  (leaf ctor)
//   `vector deleting destructor'   @ 0x826EC000  (-> ~DualGinsuExhaustEffect anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ---------------------------------------------------------------------------
// DualGinsuExhaustEffect::Creat  @ 0x826E4178   (the factory hook)
//
// The X360 allocates an 832-byte (0x340) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "DualGinsuExhaustEffect" and placement-constructs a
// DualGinsuExhaustEffect, handing it back as the +4 IResourceRequester sub-object view.
// `a1` only selects the operator-new flavour (0/1); both arms use the same size + ctor.
//
// FLAG (allocator gate): CgsSound::MemBase does NOT model operator new(size, tag,
// flavour) here, so this uses the host `new`; the +4 adjust is the static_cast to the
// IResourceRequester base. The 0x340 size is documentation only.
// ---------------------------------------------------------------------------
BrnSound::Logic::IResourceRequester* DualGinsuExhaustEffect::Creat( s32 /*aiFlavour*/ )
{
    DualGinsuExhaustEffect* lpEffect = new DualGinsuExhaustEffect();
    if ( lpEffect == nullptr )
    {
        return nullptr;
    }
    // addi r3, r3, 4 -- hand the object back as its IResourceRequester sub-object.
    return static_cast<BrnSound::Logic::IResourceRequester*>(lpEffect);
}

// ---------------------------------------------------------------------------
// DualGinsuExhaustEffect::DualGinsuExhaustEffect  @ 0x826E0E90  (leaf ctor)
//
//   bl   DualGinsuEffect::DualGinsuEffect()      ; construct the DualGinsuEffect base
//   stw  off_820B57C8, 0(r31)                    ; primary leaf vptr    (this+0)
//   stw  off_820B5794, 4(r31)                    ; IResourceRequester vptr (this+4)
//   stw  0,            0x2EC(r31)                 ; meState = E_CONSTRUCTED (0)
//   bl   CgsSound::Logic::VoiceWrapper::VoiceWrapper(r31 + 0x2F0)  ; mReverseWhineVoice
//   return this
//
// MSVC's inlined full-object constructor: `bl`s the DualGinsuEffect base ctor, installs
// the two leaf vptrs (produced structurally by the derived class's dual-vtable +
// virtual dtor), zero-seeds meState (E_CONSTRUCTED), and constructs the embedded
// VoiceWrapper member mReverseWhineVoice.
// ---------------------------------------------------------------------------
DualGinsuExhaustEffect::DualGinsuExhaustEffect()
    : DualGinsuEffect()          // bl DualGinsuEffect::DualGinsuEffect()
    , meState(E_CONSTRUCTED)     // stw 0, +0x2EC(r31)
    , mReverseWhineVoice()       // bl VoiceWrapper::VoiceWrapper(this+0x2F0)
{
}

// ---------------------------------------------------------------------------
// ~DualGinsuExhaustEffect  @ 0x826EC000  (anchor for the X360 `vector deleting destructor').
// Destroys the embedded VoiceWrapper (mReverseWhineVoice) + runs the DualGinsuEffect base
// destructor (compiler-synthesised from the declared member + base); this leaf adds
// nothing of its own. The (a2 & 1) allocator-free tail is left to the host toolchain
// (off_82FFB954 not homed here).
// ---------------------------------------------------------------------------
DualGinsuExhaustEffect::~DualGinsuExhaustEffect()
{
}

s32 DualGinsuExhaustEffect::GetController(s32 aiSlot)
{
    switch (aiSlot)
    {
    case 0: return 5; // HybridExhaustControl
    case 1: return 0; // PhysicsControl
    case 2: return 4; // EngineControl
    default: return -1;
    }
}

void DualGinsuExhaustEffect::AttachController(
    CgsSound::Logic::EffectBase* apController)
{
    const s32 liControllerId = apController->GetEffectID();
    switch (liControllerId)
    {
    case 0:
        mpPhysicsControl = static_cast<PhysicsControl*>(apController);
        break;
    case 4:
        mpEngineControl = static_cast<EngineControl*>(apController);
        break;
    case 5:
        mpHybridControl = static_cast<HybridExhaustControl*>(apController);
        break;
    default:
        CGS_ASSERT(false, "Cound't attach controller");
        break;
    }
}

bool DualGinsuExhaustEffect::Attach()
{
    switch (meState)
    {
    case E_CONSTRUCTED:
    case E_DETACHED:
        meState = E_ATTACHING_BASE_CLASS;
        // ARTIST falls through and advances the base effect in the same tick.
    case E_ATTACHING_BASE_CLASS:
        if (!DualGinsuEffect::Attach())
            return false;
        meState = E_CONSTRUCTING_VOICE;
        // ARTIST constructs the reverse-whine voice immediately once the base attaches.
    case E_CONSTRUCTING_VOICE:
    {
        CgsSound::Logic::VoiceWrapper::CreateParams lParams;
        lParams.mpLogicModule = GetLogicModule();
        lParams.mFactoryName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("~GenericRwacFactory::SK_NAME~"));
        lParams.mVoiceSpecName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("LoopVoiceSpec"));
        lParams.mContentSpecName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash(
                "gamedb://burnout5/Burnout/Sound/GlobalWaves/ReverseWhine.wav.WaveFile?ID=584565"));
        lParams.mSlotName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("~PlayerVoice::SK_PLAYER_SLOT_NAME~"));
        lParams.mSendName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("Send01"));
        lParams.mSubMixVoiceID = mSubmixIdent;
        lParams.miSendIndex = 0;
        mReverseWhineVoice.Create(lParams);
        mReverseWhineVoice.Play(0);
        meState = E_ATTACHED;
        return true;
    }
    case E_ATTACHED:
        return true;
    default:
        CGS_ASSERT(false, "Unhandled state");
        return false;
    }
}

bool DualGinsuExhaustEffect::Detach()
{
    switch (meState)
    {
    case E_ATTACHED:
        mReverseWhineVoice.Stop();
        mReverseWhineVoice.Release();
        meState = E_DETACHING_BASE_CLASS;
        // ARTIST falls through and detaches the base in the same tick.
    case E_DETACHING_BASE_CLASS:
        if (!DualGinsuEffect::Detach())
            return false;
        meState = E_DETACHED;
        return true;
    case E_DETACHED:
        return true;
    default:
        CGS_ASSERT(false, "Unhandled state");
        return false;
    }
}

void DualGinsuExhaustEffect::ProcessUpdate()
{
    DualGinsuEffect::ProcessUpdate();

    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return;

    static const u32 KU_SEND_NAME = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    static const u32 KU_PITCH_NAME = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash(
            "~GenericRwacPlayerVoice::SK_PLAYER_PARAMETER_PITCH~"));
    static const f32 KF_MAX_REVERSE_SPEED = 200.0f;
    static const f32 KF_MIN_REVERSE_PITCH = 0.8f;
    static const f32 KF_MAX_REVERSE_PITCH = 1.2f;

    const f32 lfGain = GetRWACMixerOutputValue(6, Nicotine::DMixIO::DMX_VOL);
    mReverseWhineVoice.SetGain(0, lfGain, &KU_SEND_NAME);

    const f32 lfSpeed = (std::min)(
        (std::max)(mpPhysicsControl->GetPhysicsData().mVelocityMagnitude.GetCurrent(),
                   0.0f),
        KF_MAX_REVERSE_SPEED);
    const f32 lfPitch = KF_MIN_REVERSE_PITCH +
        (lfSpeed / KF_MAX_REVERSE_SPEED) *
        (KF_MAX_REVERSE_PITCH - KF_MIN_REVERSE_PITCH);
    mReverseWhineVoice.SetParameter(0, lfPitch, &KU_PITCH_NAME);
    mReverseWhineVoice.Update();
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
