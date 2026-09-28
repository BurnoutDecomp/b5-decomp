#include "GameSource/Sound/Vehicles/Environment/BrnReverbEffect.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnclosureControl.h"
#include "GameSource/Sound/Vehicles/Environment/BrnEnvironmentSoundDiag.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/AttribSys/Enums/eImpactTime.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SharedClasses/Trigger/BrnGenericRegion.h"

// =============================================================================
// BrnSound::Vehicles::Environment::ReverbEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Recon'd function set:
//   CreateObject(u32)              @ 0x826D1438  (the factory hook)
//   ReverbEffect()                 @ 0x826BA458  (MSVC inlined full-object ctor)
//   `vector deleting destructor'   @ 0x826BA4E8  (-> ~ReverbEffect anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

// ---------------------------------------------------------------------------
// ReverbEffect::CreateObject(u32)  @ 0x826D1438   (the factory hook)
// Allocates a 120-byte (0x78) block via CgsSound::MemBase::operator new(size, tag,
// flavour) tagged "ReverbEffect" and constructs a ReverbEffect, upcast to the primary
// EffectObject* (+4). `a1` only selects the operator-new flavour (0/1).
// FLAG (allocator gate): CgsSound::MemBase::operator new is not homed here, so this uses
// the host `new`; observable result matches. The 0x78 size is documentation only.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectObject* ReverbEffect::CreateObject( u32 /*luType*/ )
{
    return new ReverbEffect();
}

// ---------------------------------------------------------------------------
// ReverbEffect::ReverbEffect  @ 0x826BA458   (the leaf constructor)
//
// MSVC's INLINED full-object constructor: it does NOT `bl` a base ctor -- it inlines the
// BrnEffectObject dual-base zero-init + installs the two leaf vptrs directly, then
// inlines the default construction of the two embedded sub-objects.
//
// NOTE (refutes the naive read): the four scalar leaf floats mfTime/mfSpaceSize/
// mfBrightness/mfGain, meReverbState, mpEnclosureControl and mpPhysicsControl are NOT
// written by this ctor -- they are left uninitialized by the X360 code. Do NOT add
// initializers for them; doing so would fabricate stores the binary does not emit. The
// only leaf effects are the two embedded-member default constructions (InterpolateLine
// @+0x48 with mbComplete=true -> stb 1, and DataPoint @+0x68 zeroed).
// ---------------------------------------------------------------------------
ReverbEffect::ReverbEffect()
    : BrnSound::Logic::BrnEffectObject()  // installs both vptrs + zero-inits the base region (BY NAME)
    // mInterpolateReverb: default-constructed -> stores @+0x48..+0x60 (mbComplete=true -> stb 1)
    // mReverbType:        default-constructed -> zeros @+0x68/+0x6C
    // mfTime/mfSpaceSize/mfBrightness/mfGain/meReverbState/mpEnclosureControl/
    // mpPhysicsControl: intentionally UNINITIALIZED (the X360 ctor writes nothing here).
{
}

// ---------------------------------------------------------------------------
// ReverbEffect::Attach
//
// Starts from the neutral preset (no time, space 15, full brightness and gain), a
// settled level-1 interpolator, no transition, and a "no preset yet" type so the first
// update always selects one.
// ---------------------------------------------------------------------------
bool ReverbEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    CGS_ASSERT(mpEnclosureControl != nullptr, "mpEnclosureControl");
    mfTime       = 0.0f;
    mfSpaceSize  = 15.0f;
    mfBrightness = 1.0f;
    mfGain       = 1.0f;
    mInterpolateReverb.Initialize(1.0f, 1.0f, 0.0f, CgsSound::Utils::Curve::E_LINEAR);
    meReverbState = E_REVERB_STATE_NONE;
    mReverbType.Flush(AttribSys::Enums::eReverbTypes::ReverbTypeCount);

    static s32 siDiagAttach = 0;
    if (SndEnvDiagBudget(siDiagAttach))
        *CgsDev::Log::gpDebugPrint << "[sndenv] reverb attach [FLAG PC witness]\n";
    return true;
}

// Controller slot 0 is the enclosure control, slot 1 the physics control.
s32 ReverbEffect::GetController(s32 aiIndex)
{
    if (aiIndex == 0)
        return 10;
    if (aiIndex == 1)
        return 0;
    return -1;
}

void ReverbEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    const s32 liEffectId = apController->GetEffectID();
    if (liEffectId == 0)
    {
        mpPhysicsControl =
            static_cast<const BrnSound::Vehicles::Engines::PhysicsControl*>(apController);
        return;
    }
    CGS_ASSERT(liEffectId == 10, "Unexpected control.");
    if (liEffectId == 10)
        mpEnclosureControl = static_cast<const EnclosureControl*>(apController);
}

// ---------------------------------------------------------------------------
// ReverbEffect::GetActiveReverb
//
// Very slow impact time selects the super-slow-motion preset and any other impact time
// the impact preset. In normal time the sound-enclosure regions at the car decide;
// the later region in the list wins when several are active.
// ---------------------------------------------------------------------------
AttribSys::Enums::eReverbTypes::eReverbTypes ReverbEffect::GetActiveReverb() const
{
    using namespace AttribSys::Enums::eReverbTypes;
    using BrnTrigger::GenericRegion;

    const AttribSys::Enums::eImpactTime::eImpactTime leImpactTime =
        static_cast<const BrnSound::Module::SoundLogicModule*>(mpLogicModule)
            ->GetFrameInformation().meImpactTime.GetCurrent();
    if (leImpactTime == AttribSys::Enums::eImpactTime::VSlow)
        return ReverbTypeSuperSloMo;
    if (leImpactTime != AttribSys::Enums::eImpactTime::False)
        return ReverbTypeImpactTime;

    const EntityTriggerInfo& lrTriggers =
        mpEnclosureControl->GetTriggerInfo(E_TRIGGER_POSITION_AT_ENTITY);
    eReverbTypes leReverb = ReverbTypeNone;
    if (lrTriggers.IsTypeActive(GenericRegion::E_TYPE_TUNNEL))
        leReverb = ReverbTypeTunnel;
    if (lrTriggers.IsTypeActive(GenericRegion::E_TYPE_OVERPASS))
        leReverb = ReverbTypeOverpass;
    if (lrTriggers.IsTypeActive(GenericRegion::E_TYPE_BRIDGE))
        leReverb = ReverbTypeBridge;
    if (lrTriggers.IsTypeActive(GenericRegion::E_TYPE_WAREHOUSE))
        leReverb = ReverbTypeWarehouse;
    if (lrTriggers.IsTypeActive(GenericRegion::E_TYPE_LARGE_OVERHEAD_OBJECT))
        leReverb = ReverbTypeLargeOverheadObject;
    if (lrTriggers.IsTypeActive(GenericRegion::E_TYPE_NARROW_ALLEY))
        leReverb = ReverbTypeNarrowAlley;
    return leReverb;
}

// ---------------------------------------------------------------------------
// ~ReverbEffect  @ 0x826BA4E8  (anchor for the X360 `vector deleting destructor').
// This X360 body is store-for-store identical to the committed BrnEffectObject dtor
// @ 0x826AF4C8 -- the same dual-vptr settle + the same attach/detach/resources-ready
// clears. In reconstructed C++ that dual-base settle and the deleting-destructor thunk
// are compiler-synthesised from the virtual dtor declared in the header, so the leaf
// body is empty. The (a2 & 1) allocator-free tail is left to the host toolchain.
// ---------------------------------------------------------------------------
ReverbEffect::~ReverbEffect()
{
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
