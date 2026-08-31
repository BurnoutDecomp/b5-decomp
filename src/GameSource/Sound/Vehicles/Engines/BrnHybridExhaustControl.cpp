#include "GameSource/Sound/Vehicles/Engines/BrnHybridExhaustControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnEngineControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnShiftControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnClutchControl.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"

#include <cstring>   // std::memset
#include <algorithm>

// =============================================================================
// BrnSound::Vehicles::Engines::HybridExhaustControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnHybridExhaustControl.h for the
// base rationale + opaque-crossfade-span FLAG. Recon'd function set:
//   Create(bool)                   @ 0x826B34E0  (the RTTI factory hook)
//   HybridExhaustControl()         @ 0x826AF938  (field-init ctor)
//   `vector deleting destructor'   @ 0x826AFA60  (-> ~HybridExhaustControl anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ---------------------------------------------------------------------------
// HybridExhaustControl::Create  @ 0x826B34E0   (the factory hook)
//
// The X360 allocates a 304-byte (0x130) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "HybridExhaustControl" and inline-constructs a
// HybridExhaustControl, returning it as the +4 IResourceRequester second-base view.
// `abFlavour` selects only the operator-new flavour (0/1); both arms allocate the same
// size and run the same ctor.
//
// FLAG (allocator gate): CgsSound::MemBase does NOT model operator new(size, tag,
// flavour) here, so this uses the host `new`; the +4 adjust is the static_cast to the
// IResourceRequester base. The 0x130 size is documentation only.
// ---------------------------------------------------------------------------
BrnSound::Logic::IResourceRequester* HybridExhaustControl::Create( bool abFlavour )
{
    (void)abFlavour; // selects only the MemBase operator-new flavour (0/1)
    return static_cast<BrnSound::Logic::IResourceRequester*>( new HybridExhaustControl() );
}

// ---------------------------------------------------------------------------
// HybridExhaustControl::HybridExhaustControl  @ 0x826AF938  (field-init ctor)
//
// The BrnEffectControl dual-base sub-object (both vptr stores + the base bookkeeping
// clears) is emitted by the base default ctor; this leaf then wires the declared
// members: the two vehicleengine attribute instances (constructed empty/unbound), the
// Average<3,f32> / three DataPoint<f32> (default-zeroed), the two EngineMix (zeroed),
// and the mfPercentOf* thresholds. The crossfade-mix span (Graph + Vector2[6]) is
// opaque here (see header FLAG): the X360 seeds Graph.mpaPoints -> &maCrossFadesPoints[0]
// and Graph.mu8NumOfPoints -> 6, which is a contested Graph/Vector2 binding, so the span
// is zeroed rather than fabricated with an incompatible type.
// ---------------------------------------------------------------------------
HybridExhaustControl::HybridExhaustControl()
    : mpPhysicsControl(nullptr)
    , mpEngineControl(nullptr)
    , mpShiftControl(nullptr)
    , mpClutchControl(nullptr)
    , mVehicleEngineAttributes(nullptr, nullptr)                 // vehicleengine(this+0x48,0,0)
    , mMasterVehicleEngineComponentAttributes(nullptr, nullptr)  // vehicleengine(this+0x58,0,0)
    // mAverageDeltaRPM: Average<3,f32> default ctor zeroes maPoints/muNextPoint/mfAverage
    // mPhysicsDeltaRpm / mAudioDeltaRpm / mGinsuRpm: DataPoint<f32> default -> {0,0}
    , mfPercentOfAccelThreshold(0.0f)   // X360 leaves uninitialised; safe default
    , mfPercentOfDecelThreshold(0.0f)   // X360 leaves uninitialised; safe default
    // mFinalEngineMix / mFinalEngineVolume: EngineMix default ctor -> all 0.0f
{
    // Crossfade-mix (Graph + Vector2[6]) opaque span: zeroed (see header FLAG). The X360
    // seeds Graph.mpaPoints=&maCrossFadesPoints[0] and Graph.mu8NumOfPoints=6, a contested
    // Graph/Vector2 binding NOT reproduced by a fabricated typed member.
    std::memset(mau8DecelCrossfadeMix, 0, sizeof(mau8DecelCrossfadeMix));
}

// ---------------------------------------------------------------------------
// ~HybridExhaustControl  @ 0x826AFA60  (anchor for the X360 `vector deleting destructor').
// The observable member teardown is the inherited ~BrnEffectControl dual-base chain
// plus the two vehicleengine sub-object dtors (compiler-synthesised from the declared
// members), so this leaf body is empty. The (a2 & 1) allocator-free tail is
// re-synthesised by the host toolchain (off_82FFB954 not homed here).
// ---------------------------------------------------------------------------
HybridExhaustControl::~HybridExhaustControl()
{
}

s32 HybridExhaustControl::GetController(s32 aiSlot)
{
    static const s32 kaiControllers[] = { 0, 4, 2, 3, 12 };
    if (aiSlot < 0 || aiSlot >= static_cast<s32>(sizeof(kaiControllers) / sizeof(kaiControllers[0])))
        return -1;
    if (aiSlot == 4 && GetStateId() != 1)
        return -1;
    return kaiControllers[aiSlot];
}

void HybridExhaustControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    switch (apController->GetEffectID())
    {
    case 0: mpPhysicsControl = static_cast<PhysicsControl*>(apController); break;
    case 2: mpShiftControl = static_cast<ShiftControl*>(apController); break;
    case 3: mpClutchControl = static_cast<ClutchControl*>(apController); break;
    case 4: mpEngineControl = static_cast<EngineControl*>(apController); break;
    case 1:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
        break;
    default:
        CGS_ASSERT(false, "I don't know how to handle attaching this type of EffectBase.");
        break;
    }
}

bool HybridExhaustControl::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return false;

    const BrnSound::Vehicles::VehicleState::EEngineComponentType leComponent =
        (GetId() & 0x7F0) == 0x60
            ? BrnSound::Vehicles::VehicleState::E_ENGINE
            : BrnSound::Vehicles::VehicleState::E_EXHAUST;
    const u64 luCollectionKey = mpPhysicsControl->GetEngineComponentKey(leComponent);
    Attrib::Collection* lpCollection = Attrib::FindCollectionWithDefault(
        0x7F161D94482CB3BFull, luCollectionKey);
    mVehicleEngineAttributes.Change(lpCollection);
    mMasterVehicleEngineComponentAttributes.Change(lpCollection);

    CGS_ASSERT(mVehicleEngineAttributes.LoopModel() != nullptr,
               "mVehicleEngineAttributes.LoopModel()");
    CGS_ASSERT(mVehicleEngineAttributes.GinsuFileAccel() != nullptr,
               "mVehicleEngineAttributes.GinsuFileAccel()");
    CGS_ASSERT(mVehicleEngineAttributes.GinsuFileDecel() != nullptr,
               "mVehicleEngineAttributes.GinsuFileDecel()");

    mPhysicsDeltaRpm.Flush(0.0f);
    mAudioDeltaRpm.Flush(0.0f);
    mGinsuRpm.Flush(mpPhysicsControl->GetPhysicsData().mNormalizedRpm.GetCurrent());
    return true;
}

void HybridExhaustControl::UpdateParams(f32 afTimeStep)
{
    if (!mpPhysicsControl)
        return;

    CGS_ASSERT(mVehicleEngineAttributes.LoopModel() != nullptr,
               "mVehicleEngineAttributes.LoopModel()");
    CGS_ASSERT(mVehicleEngineAttributes.GinsuFileAccel() != nullptr,
               "mVehicleEngineAttributes.GinsuFileAccel()");
    CGS_ASSERT(mVehicleEngineAttributes.GinsuFileDecel() != nullptr,
               "mVehicleEngineAttributes.GinsuFileDecel()");

    // ARTIST @ 0x826E3DE0: delta tracking, the virtual Ginsu-RPM mapping, then
    // the data-driven source mix.  The previous interim body skipped the middle
    // controller and fed normalized 1k..10k RPM directly to the Ginsu voices.
    UpdateDeltaRPM();
    UpdateGinsuRPM();
    UpdateMix(afTimeStep);
}

void HybridExhaustControl::UpdateDeltaRPM()
{
    const PhysicsControl::PhysicsData& lrPhysics = mpPhysicsControl->GetPhysicsData();
    mPhysicsDeltaRpm.Update(lrPhysics.mNormalizedRpm.GetCurrent() -
                            lrPhysics.mNormalizedRpm.GetPrevious());

    const f32 lfAudioDelta = mpEngineControl
        ? mpEngineControl->GetAudioRPM().GetCurrent() -
          mpEngineControl->GetAudioRPM().GetPrevious()
        : mPhysicsDeltaRpm.GetCurrent();
    mAudioDeltaRpm.Update(lfAudioDelta);

    // The ordinary (not shifting, clutch-off) branch at 0x826B36C0 records the
    // physics delta.  ShiftControl/ClutchControl replace this sample while their
    // authored transition state machines are active.
    mAverageDeltaRPM.Record(mPhysicsDeltaRpm.GetCurrent());
}

void HybridExhaustControl::UpdateGinsuRPM()
{
    const f32 lfAudioRpm = mpEngineControl
        ? mpEngineControl->GetAudioRPM().GetCurrent()
        : mpPhysicsControl->GetPhysicsData().mNormalizedRpm.GetCurrent();
    const f32 lfUnity = ((std::max)(1000.0f, (std::min)(10000.0f, lfAudioRpm)) -
                         1000.0f) * (1.0f / 9000.0f);
    const f32 lfIdleRpm = mVehicleEngineAttributes.IdleRpm();
    const f32 lfMaxRpm = mVehicleEngineAttributes.MaxRpm();
    mGinsuRpm.Update(lfIdleRpm + (lfMaxRpm - lfIdleRpm) * lfUnity);
}

void HybridExhaustControl::UpdateMix(f32 /*afTimeStep*/)
{
    const PhysicsControl::PhysicsData& lrPhysics = mpPhysicsControl->GetPhysicsData();

    // UpdateMix @ 0x826CC878 applies the authored master/component gains to
    // the loop/Ginsu mix before the effect-side dynamic-mixer gains.  Preserve
    // that data-driven volume leg while the recovered crossfade graph supplies
    // the three source weights.
    const f32 lfMaster = mVehicleEngineAttributes.MasterGain() *
                         mMasterVehicleEngineComponentAttributes.MasterCarVolume();
    const f32 lfThrottle = lrPhysics.mThrottle.GetCurrent();
    mFinalEngineMix.Loop = 1.0f;
    mFinalEngineMix.AccelGinsu = lfThrottle;
    mFinalEngineMix.DecelGinsu = 1.0f - lfThrottle;
    mFinalEngineMix.Cutoff = 25000.0f;
    mFinalEngineVolume.Loop = lfMaster * mFinalEngineMix.Loop;
    mFinalEngineVolume.AccelGinsu = lfMaster * mFinalEngineMix.AccelGinsu;
    mFinalEngineVolume.DecelGinsu = lfMaster * mFinalEngineMix.DecelGinsu;
    mFinalEngineVolume.Cutoff = 25000.0f;
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
