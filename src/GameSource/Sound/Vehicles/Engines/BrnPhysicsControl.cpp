#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Sound/Vehicles/Brn3dCarPosition.h"
#include "GameSource/Sound/Vehicles/Wheels/BrnWheelControl.h"
#include "GameSource/Sound/Vehicles/BrnVehicleStateManager.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"
#include "GameSource/Sound/Vehicles/BrnEngineAudioDiag.h"   // [DIAG] NOT IN THE X360 BINARY

#include <cmath>
#include <cstdio>
#include <algorithm>

// =============================================================================
// BrnSound::Vehicles::Engines::PhysicsControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnPhysicsControl.h for the base
// rationale + opaque-span layout FLAG.
//
// Recon'd function set:
//   PhysicsControl::GetEngineComponentName        @ 0x82682CA8
//   PhysicsControl::GetEngineComponentKey         @ 0x82682D10
//   PhysicsControl::GetRawPhysicsData             @ 0x82682DA0
//   PhysicsControl::PhysicsControl                @ 0x826C8890
//   PhysicsControl::`vector deleting destructor'  @ 0x826AF8B0  (-> ~PhysicsControl)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ---------------------------------------------------------------------------
// PhysicsControl::PhysicsControl  @ 0x826C8890  (default ctor)
//
// Derived ctor: the X360 INLINES the base zero-init (no `bl` to a base ctor) and
// installs the dual leaf vptrs directly, zeroes the leading scalar/DataPoint members,
// constructs the PhysicsData (@+0x40) and vehicleengine (@+0x228) sub-objects, then
// clears the intro-reving block (+0x248..+0x280) and sets the trailing flag @+0x284 = 1.
// The inlined dual-base install + base scalar zero-init is represented by the
// `: BrnEffectControl()` base-init. PhysicsData / vehicleengine are un-homed opaque
// spans, so their construction + the intro block are reproduced by zeroing the spans +
// setting the attested flag byte rather than by ctor calls (see header FLAG).
// ---------------------------------------------------------------------------
PhysicsControl::PhysicsData::PhysicsData()
    : mbJustShifted(false)
    , mfDurationInGear(0.0f)
    , mfMaxRpm(7000.0f)
    , mfIdleRpm(997.0f)
    , mfTimeSinceRespawn(0.0f)
    , IsBlueBoost(false)
    , mfBoostRemaining(0.0f)
    , mfRotation(0.0f)
{
    Matrix44Affine lIdentity;
    lIdentity.SetIdentity();
    mTransform.Flush(lIdentity);
}

PhysicsControl::PhysicsControl()
    : BrnSound::Logic::BrnEffectControl()
    , mpVehiclePhysicsData(nullptr)
    , mProcessedPhysicsData()
    , mp3dCarControl(nullptr)
    , mpWheelControl(nullptr)
    , mVehicleEngineAttributes(nullptr, nullptr)
    , mAttachInfo()
    , mfOscillator(0.0f)
    , mfAngularVelocityAccumulator(0.0f)
    , meIntroRevingState(E_NIS_REVING_STATE_OFF)
    , mEngineDataSet()
    , mEngineStartLineRPM()
    , mfDiagWitnessTimer(0.0f)      // [DIAG] NOT IN THE X360 BINARY
{
    mAttachInfo.mpVehicleAsset = nullptr;
    mAttachInfo.muVehicleIndex = 0;
    mAttachInfo.mAttachToken = 0;
}

// ---------------------------------------------------------------------------
// ~PhysicsControl  @ 0x826AF8B0  (anchor for the X360 `vector deleting destructor').
// The observable member teardown is the base BrnEffectControl dtor chain plus the
// PhysicsData / vehicleengine sub-object destructors (compiler-synthesised); this leaf
// adds nothing. The (a2 & 1) allocator-free tail is left to the host toolchain
// (off_82FFB954 not homed here).
// ---------------------------------------------------------------------------
PhysicsControl::~PhysicsControl()
{
}

s32 PhysicsControl::GetController(s32 aiSlot)
{
    if (aiSlot == 0)
        return 7;
    if (aiSlot == 1)
        return 1;
    CGS_ASSERT(aiSlot < 3, "liIndex < 3");
    return -1;
}

void PhysicsControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    const s32 liControllerId = apController->GetEffectID();
    if (liControllerId == 1)
    {
        mpWheelControl = static_cast<BrnSound::Vehicles::Wheels::WheelControl*>(apController);
        return;
    }
    if (liControllerId == 7)
    {
        mp3dCarControl = static_cast<BrnSound::Vehicles::Car3DControl*>(apController);
        return;
    }
    CGS_ASSERT(false, "Cound't attach controller ");
}

void PhysicsControl::SetupLoadData()
{
    BrnSound::Vehicles::VehicleState* lpVehicleState =
        static_cast<BrnSound::Vehicles::VehicleState*>(GetStateBase());
    CGS_ASSERT(lpVehicleState != nullptr, "lpVehicleState");
    if (!lpVehicleState)
        return;

    mpVehiclePhysicsData = lpVehicleState->GetVehicleData();
    CGS_ASSERT(mpVehiclePhysicsData != nullptr, "mpVehiclePhysicsData");

    if (GetStateId() != 1)
    {
        SetAttachState(CgsSound::Logic::EffectBase::E_ATTACH_STATE_PREPARING);
        return;
    }

    for (s32 liComponent = 0; liComponent < BrnSound::Vehicles::VehicleState::E_MAX_TYPES; ++liComponent)
    {
        const char* lpcComponent = GetEngineComponentName(
            static_cast<BrnSound::Vehicles::VehicleState::EEngineComponentType>(liComponent));
        char lacBundle[64];
        char lacRegistry[64];
        const u32 luHash = static_cast<u32>(CgsResource::ID::HashString(
            reinterpret_cast<const u8*>(lpcComponent)));
        std::snprintf(lacBundle, sizeof(lacBundle), "Engines\\%08x.bundle", luHash);
        std::snprintf(lacRegistry, sizeof(lacRegistry), "%sRegistry", lpcComponent);
        LoadAsset(lacBundle, lpcComponent, BrnSound::Logic::ResourceRegistrar::E_ATTRIBSYS);
        LoadAsset(lacBundle, lacRegistry, BrnSound::Logic::ResourceRegistrar::E_DATA);

        // [DIAG] NOT IN THE X360 BINARY -- BRN_ENGINE_DIAG.
        if (BrnSound::Vehicles::EngineAudioDiagLive())
        {
            *CgsDev::Log::gpDebugPrint
                << "[engine-asset] component=" << (liComponent == 0 ? "ENGINE" : "EXHAUST")
                << " name=" << lpcComponent
                << " bundle=" << lacBundle
                << " registry=" << lacRegistry
                << "\n";
        }
    }
}

bool PhysicsControl::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    BrnSound::Vehicles::VehicleState* lpVehicleState =
        static_cast<BrnSound::Vehicles::VehicleState*>(GetStateBase());
    CGS_ASSERT(lpVehicleState != nullptr, "lpVehicleState");
    if (!lpVehicleState)
        return false;

    mpVehiclePhysicsData = lpVehicleState->GetVehicleData();
    mAttachInfo = lpVehicleState->GetAttachInfo();
    mVehicleEngineAttributes.Change(Attrib::FindCollectionWithDefault(
        0x7F161D94482CB3BFull,
        GetEngineComponentKey(BrnSound::Vehicles::VehicleState::E_EXHAUST)));
    mfOscillator.Flush(0.0f);
    mfAngularVelocityAccumulator.Flush(0.0f);
    mProcessedPhysicsData.mfTimeSinceRespawn = 0.0f;
    SetMixerInputValue(7, 0);

    if (GetStateId() == 1)
    {
        BrnSound::Vehicles::VehicleStateManager* lpManager =
            static_cast<BrnSound::Vehicles::VehicleStateManager*>(
                lpVehicleState->GetStateManager());
        CGS_ASSERT(lpManager != nullptr, "lpVehicleStateManager");
        if (lpManager)
        {
            lpManager->AddRegistry(GetEngineComponentName(BrnSound::Vehicles::VehicleState::E_ENGINE), false);
            lpManager->AddRegistry(GetEngineComponentName(BrnSound::Vehicles::VehicleState::E_EXHAUST), false);
        }
    }
    return true;
}

void PhysicsControl::UpdateParams(f32 afTimeStep)
{
    if (!mpVehiclePhysicsData)
        return;

    const BrnSound::Vehicles::VehicleData& lrRaw = *mpVehiclePhysicsData;
    PhysicsData& lrData = mProcessedPhysicsData;

    // ARTIST @ 0x826CB7B0:
    //     lwz  r26, 8(r31)        ; the VehicleState
    //     lfs  f0, 0x520(r26)     ; VehicleState::mfMaxRpm      (+1312)
    //     stfs f0, 0x5C(r31)      ; PhysicsData::mfMaxRpm       (PhysicsData+0x20)
    // -- the car's OWN max RPM (VehicleState::UpdateParams takes it from the six
    // authored physicsvehicleengineattribs GearUpRPM lanes) is re-read into the
    // PhysicsData EVERY frame. Without it the normalization divisor stayed at the
    // PhysicsData ctor's 7000.0f for every car in the game, which is the divisor
    // UnityPhysicsRpm @0x826B2860 uses for the engine note.
    BrnSound::Vehicles::VehicleState* lpVehicleState =
        static_cast<BrnSound::Vehicles::VehicleState*>(GetStateBase());
    lrData.mfMaxRpm = lpVehicleState->GetMaxRPM();

    const s32 liGear = static_cast<s32>(lrRaw.mi8Gear);
    lrData.mbJustShifted = lrData.mGear.GetCurrent() != liGear;
    lrData.mGear.Update(liGear);
    lrData.mfDurationInGear = lrData.mbJustShifted ? 0.0f : lrData.mfDurationInGear + afTimeStep;

    // ARTIST applies the authored PhysicsRpmMap cubic to the normalized RPM.
    // The IDLE limit is the PhysicsData ctor's 997 (UpdateParams never touches
    // PhysicsData+0x24); the MAX limit is NOT the ctor's 7000 -- see the
    // mfMaxRpm re-read above. An earlier interim body used the CURRENT gear's
    // upshift RPM with a linear map, which redlined too early; the console uses
    // the MAXIMUM of the six authored GearUpRPM lanes with the cubic.
    const f32 lfUnityRpm = UnityPhysicsRpm(lrRaw.mfRPM);
    lrData.mUnityRpm.Update(lfUnityRpm);
    lrData.mNormalizedRpm.Update(lfUnityRpm * 9000.0f + 1000.0f);

    // Reverse gear uses the brake control as its engine throttle in ARTIST.
    const f32 lfThrottle = liGear != 0 ? lrRaw.mfGas : lrRaw.mfBrake;
    lrData.mThrottle.Update(lfThrottle);
    lrData.mDeltaThrottle.Record(lrData.mThrottle.GetCurrent() - lrData.mThrottle.GetPrevious());
    lrData.mIsAccelerating.Update(lrRaw.mfGas > 0.15f);
    lrData.IsBoosting.Update(lrRaw.mfTimeBoosting > 0.0f);
    lrData.IsCrashing.Update(lrRaw.mbCrashing);
    lrData.IsDeforming.Update(lrRaw.mbStartedDeforming);
    lrData.mTransform.Update(lrRaw.mTransform);
    lrData.mPosition3d.Update(lrRaw.mTransform.Pos());
    Vector2 lPosition2d = { lrRaw.mTransform.Pos().x, lrRaw.mTransform.Pos().z, 0.0f, 0.0f };
    lrData.mPosition2d.Update(lPosition2d);
    lrData.mVelocity3d.Update(lrRaw.mLinearVelocity);
    Vector2 lVelocity2d = { lrRaw.mLinearVelocity.x, lrRaw.mLinearVelocity.z, 0.0f, 0.0f };
    lrData.mVelocity2d.Update(lVelocity2d);
    const f32 lfVelocity = std::sqrt(lrRaw.mLinearVelocity.x * lrRaw.mLinearVelocity.x +
                                     lrRaw.mLinearVelocity.y * lrRaw.mLinearVelocity.y +
                                     lrRaw.mLinearVelocity.z * lrRaw.mLinearVelocity.z);
    lrData.mVelocityMagnitude.Update(lfVelocity);
    // ARTIST 0x826CB8B8: `lfs f0,0x3CC(r7) ; fabs f0,f0 ; stfs f0,0x124(r31)` --
    // mSpeedMPH is the ABSOLUTE speed, and 0x826CBB5C re-reads that same absolute
    // value for the MPS conversion (flt_8200D4DC = 0.44704). Without the fabs a
    // reversing car drives the four speed ramps below to zero.
    const f32 lfAbsSpeedMPH = std::fabs(lrRaw.mfSpeedMPH);
    lrData.mSpeedMPH.Update(lfAbsSpeedMPH);
    lrData.mSpeedMPS.Update(lfAbsSpeedMPH * 0.44704f);
    // ARTIST 0x826CB7B8..0x826CB7FC -- mDrifting is RATE-LIMITED toward the raw
    // mfAbsDriftScale, not assigned from it:
    //     f11 = target - cur ; if (f11 > flt_82F2CC1C)  new = cur + flt_82F2CC1C
    //     else { f11 = cur - target ; if (f11 > flt_82F2CC18) new = cur - flt_82F2CC18 }
    // with flt_82F2CC1C = 0.04 (rise) and flt_82F2CC18 = 0.01 (fall) per frame.
    // mDrifting feeds ClutchControl, HybridExhaustControl, BoostEffect and
    // WheelControl, so an unfiltered step made every one of them jump.
    {
        const f32 lfTarget = lrRaw.mfAbsDriftScale;
        const f32 lfCurrent = lrData.mDrifting.GetCurrent();
        f32 lfNext = lfTarget;
        if (lfTarget - lfCurrent > 0.04f)
            lfNext = lfCurrent + 0.04f;
        else if (lfCurrent - lfTarget > 0.01f)
            lfNext = lfCurrent - 0.01f;
        lrData.mDrifting.Update(lfNext);
    }
    lrData.mfTimeSinceRespawn += afTimeStep;

    // -----------------------------------------------------------------------
    // The DMix control-input feed, ARTIST @ 0x826CBD08..0x826CBE84, in order.
    //
    // 0x826CBD08  lwz r3,0x30(r31) / beq / li r5,0x7FFF / li r4,7
    //             bl Nicotine::DMixIO::SetDMixInput
    // -- slot 7 is opened wide EVERY frame, straight on the DMixIO (not through
    // SetMixerInputValue). Attach @0x826CB540 seeds it to 0.
    Nicotine::DMixIO* lpDMix = GetDMixIOPtr();
    if (lpDMix)
        lpDMix->SetDMixInput(7, 0x7FFF);

    // 0x826CBD20/0x826CBD70/0x826CBDB4/0x826CBDFC -- four speed ramps off
    // mSpeedMPH, each `clamp(v, 0, LIMIT) * (1/LIMIT) * 32767.0`. The clamp is
    // the two-fsel idiom (`fsel(-v, 0, v)` = max(v,0); `fsel(L-v, v, L)` =
    // min(v,L)); the limits are flt_820ABCD8=5.0, flt_82004F5C=30.0,
    // flt_82004A18=80.0, flt_82006530=150.0, the reciprocals flt_82004744=0.2,
    // flt_820AA358=0.033333335, flt_82009B98=0.0125, flt_820B3BBC=0.0066666668,
    // and f31 = flt_820AD310 = 32767.0.
    const f32 lfSpeedMPH = lrData.mSpeedMPH.GetCurrent();
    SetMixerInputValue(0, SpeedRampMixerValue(lfSpeedMPH,   5.0f, 0.2f));
    SetMixerInputValue(1, SpeedRampMixerValue(lfSpeedMPH,  30.0f, 0.033333335f));
    SetMixerInputValue(2, SpeedRampMixerValue(lfSpeedMPH,  80.0f, 0.0125f));
    SetMixerInputValue(3, SpeedRampMixerValue(lfSpeedMPH, 150.0f, 0.0066666668f));

    // 0x826CBE44  lbz r11,0x44A(rawdata) ; subfic/subfe/clrlwi -> 0 or 0x7FFF
    SetMixerInputValue(4, lrRaw.mbCrashing ? 0x7FFF : 0);

    // 0x826CBE64  li r4,5 / lbz r5,0x524(r26) -- the VehicleState's latched
    // collision flag, passed as the RAW BYTE (0 or 1, not 0x7FFF), and cleared
    // straight afterwards (0x826CBE80 stb r25,0x524(r26)): a one-shot event.
    const s32 liCollisionLatch = lpVehicleState->GetCollisionOccured() ? 1 : 0;
    SetMixerInputValue(5, liCollisionLatch);
    if (liCollisionLatch)
        lpVehicleState->SetCollisionOccured(false);

    // 0x826CBE84  lbz r11,0x44C(rawdata) -> 0x7FFF / 0
    SetMixerInputValue(8, lrRaw.mbIsDriveable ? 0x7FFF : 0);

    // OPEN (not landed here): slot 6, ARTIST 0x826CBEA8..0x826CC04C -- the
    // director-camera engine gain. It needs two reads this tree cannot yet make
    // BY NAME: the camera's +0x140 flag word bits 3 and 4 (both set => 0x7FFF),
    // and the GameModeOutputInterface's +0xC mode word (0 or 1 => 0). Its third
    // branch is clamp((1 - dot3(mTransform.zAxis, camera.mTransform.zAxis)) *
    // 0.5, 0, 1) * 32767, latched into PhysicsData+0x1D0. Landing it needs the
    // GameModeOutputInterface typed (it is u8[0x10] opaque in
    // BrnRootSoundModuleIo.h today), so it is left unwritten rather than
    // approximated.

    EngineParamWitness(afTimeStep);
}

// ---------------------------------------------------------------------------
// [DIAG] NOT IN THE X360 BINARY -- BRN_ENGINE_DIAG.
// One line per second of game time per control: the physics -> AEMS parameter
// feed and the NINE DMix control-input slots READ BACK from the DMixIO, i.e.
// exactly what the authored AEMS patch graph sees. Reading them back (rather
// than echoing what we just wrote) is what makes a never-written slot visible.
// ---------------------------------------------------------------------------
void PhysicsControl::EngineParamWitness(f32 afTimeStep)
{
    if (!BrnSound::Vehicles::EngineAudioDiagLive())
        return;

    mfDiagWitnessTimer += afTimeStep;
    if (mfDiagWitnessTimer < 1.0f)
        return;
    mfDiagWitnessTimer = 0.0f;

    const BrnSound::Vehicles::VehicleData& lrRaw = *mpVehiclePhysicsData;
    const PhysicsData& lrData = mProcessedPhysicsData;

    *CgsDev::Log::gpDebugPrint
        << "[engine-param] state=" << GetStateId()
        << " rpm=" << lrRaw.mfRPM
        << " unity=" << lrData.mUnityRpm.GetCurrent()
        << " norm=" << lrData.mNormalizedRpm.GetCurrent()
        << " throttle=" << lrData.mThrottle.GetCurrent()
        << " gear=" << lrData.mGear.GetCurrent()
        << " mph=" << lrData.mSpeedMPH.GetCurrent()
        << " idleRpm=" << lrData.mfIdleRpm
        << " maxRpm=" << lrData.mfMaxRpm;

    Nicotine::DMixIO* lpDMix = GetDMixIOPtr();
    *CgsDev::Log::gpDebugPrint << " dmix=";
    if (!lpDMix)
    {
        *CgsDev::Log::gpDebugPrint << "<null>\n";
        return;
    }
    for (s32 liSlot = 0; liSlot <= 8; ++liSlot)
    {
        if (liSlot)
            *CgsDev::Log::gpDebugPrint << ",";
        *CgsDev::Log::gpDebugPrint << lpDMix->GetDMixInput(liSlot);
    }
    *CgsDev::Log::gpDebugPrint << "\n";
}

f32 PhysicsControl::UnityPhysicsRpm(f32 afPhysicsRPM) const
{
    const f32 lfRange = mProcessedPhysicsData.mfMaxRpm -
                        mProcessedPhysicsData.mfIdleRpm;
    f32 lfUnity = lfRange != 0.0f
        ? (afPhysicsRPM - mProcessedPhysicsData.mfIdleRpm) / lfRange
        : 0.0f;
    lfUnity = (std::max)(0.0f, (std::min)(1.0f, lfUnity));

    // ARTIST @ 0x826B28C0 builds [x^3,x^2,x,0], evaluates the four
    // PhysicsRpmMap vectors with Horner's rule, and returns the Y lane.
    const Matrix44 lMap = mVehicleEngineAttributes.PhysicsRpmMap();
    const f32 lfUnity2 = lfUnity * lfUnity;
    const f32 lfMapped = lMap.xAxis.y * (lfUnity2 * lfUnity) +
                         lMap.yAxis.y * lfUnity2 +
                         lMap.zAxis.y * lfUnity +
                         lMap.wAxis.y;
    return (std::max)(0.0f, (std::min)(1.0f, lfMapped));
}

// ---------------------------------------------------------------------------
// The four speed-ramp DMix values of UpdateParams, factored out because the
// console emits the same nine instructions four times with different constants:
//   fneg f13,v ; fsel f0,f13,0.0,v      -> max(v, 0)
//   fsubs f12,L,f0 ; fsel f13,f12,f0,L  -> min(that, L)
//   fmuls f0,f13,R ; fmuls f0,f0,32767.0 ; fctiwz
// (fsel(a,b,c) is a >= 0 ? b : c, so a NaN speed takes the `c` arm in both --
// the same way round as the console.)
// ---------------------------------------------------------------------------
s32 PhysicsControl::SpeedRampMixerValue(f32 afSpeedMPH, f32 afLimit, f32 afReciprocal)
{
    const f32 lfLow  = (-afSpeedMPH >= 0.0f) ? 0.0f : afSpeedMPH;
    const f32 lfHigh = ((afLimit - lfLow) >= 0.0f) ? lfLow : afLimit;
    return static_cast<s32>(lfHigh * afReciprocal * 32767.0f);
}

void PhysicsControl::ProcessUpdate()
{
}

// ---------------------------------------------------------------------------
// PhysicsControl::GetEngineComponentName  @ 0x82682CA8
//   const char* GetEngineComponentName(VehicleState::EEngineComponentType)
//
//   assert mpVehicleState != 0
//   return mpVehicleState->GetEngineComponentName(type)
// (VehicleState::GetEngineComponentName is declared on the reconciled DWARF
// VehicleState -- body DEFERRED to its own slice, see BrnVehicleState.h.)
// ---------------------------------------------------------------------------
const char* PhysicsControl::GetEngineComponentName(
    BrnSound::Vehicles::VehicleState::EEngineComponentType aeComponentType )
{
    const BrnSound::Vehicles::VehicleState* lpVehicleState =
        static_cast<const BrnSound::Vehicles::VehicleState*>(GetStateBase());
    CGS_ASSERT(lpVehicleState != nullptr, "lpVehicleState");
    return lpVehicleState->GetEngineComponentName(aeComponentType);
}

// ---------------------------------------------------------------------------
// PhysicsControl::GetEngineComponentKey  @ 0x82682D10
//   Attribute::Key GetEngineComponentKey(VehicleState::EEngineComponentType)
//
//   assert mpVehicleState != 0
//   return mpVehicleState->GetEngineComponentKey(type)
// The console body inlines VehicleState::GetEngineComponentKey: the key array is
// VehicleState's mEngineComponentKey at byte +0x510 with 8-byte element stride
// ((type + 0xA2) * 8 == +0x510 + type*8) and the looked-up key is asserted
// non-zero. Since the wave-5 (2026-08-25) VehicleState reconciliation the walk is
// BY NAME inside VehicleState::GetEngineComponentKey (BrnVehicleState.cpp),
// including the non-zero guard; this forwarder keeps only its own null assert.
// ---------------------------------------------------------------------------
u64 PhysicsControl::GetEngineComponentKey(
    BrnSound::Vehicles::VehicleState::EEngineComponentType aeComponentType )
{
    const BrnSound::Vehicles::VehicleState* lpVehicleState =
        static_cast<const BrnSound::Vehicles::VehicleState*>(GetStateBase());
    CGS_ASSERT(lpVehicleState != nullptr, "lpVehicleState");
    return lpVehicleState->GetEngineComponentKey(aeComponentType);
}

// ---------------------------------------------------------------------------
// PhysicsControl::GetRawPhysicsData  @ 0x82682DA0
//   const VehicleData* GetRawPhysicsData() const
//     assert mpVehiclePhysicsData != 0 ; return mpVehiclePhysicsData
// ---------------------------------------------------------------------------
const BrnSound::Vehicles::VehicleData* PhysicsControl::GetRawPhysicsData() const
{
    CGS_ASSERT(mpVehiclePhysicsData != nullptr, "mpVehiclePhysicsData");
    return mpVehiclePhysicsData;
}

// ---------------------------------------------------------------------------
// PhysicsControl::GetStaticTypeInfo()  @ 0x82684368  (IDA-truncated "BrnSound::Vehicle")
//
//   lis   r11, unk_82F2F578@ha
//   addi  r3,  r11, unk_82F2F578@l   ; r3 = &sTypeInfo (the rodata descriptor)
//   blr
//
// Returns PhysicsControl's per-class static RTTI descriptor. The 2-instruction
// &unk_X; blr shape is the committed per-class GetStaticTypeInfo() accessor form
// (ExplosionState::GetStaticTypeInfo @ 0x82689198) -- a function-local static,
// aggregate-initialised ClassTypeInfo<EffectControl>.
//
// FLAG (confidence medium): ObjectID is unrecovered (the per-leaf registration
// static-init that would seed it was not exported) -> seeded 0 per the in-tree
// placeholder convention; baseTypeInfo (EffectControl RTTI chain) and createObject
// are DEFERRED (un-homed) -> nullptr. typeName "PhysicsControl" is inferred from the
// class identity / the adjacent GetTypeName @ 0x82684378 tag (not proven in-scope,
// mirrors the ExplosionState precedent's inferred-typeName flag).
// ---------------------------------------------------------------------------
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* PhysicsControl::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> sTypeInfo =
    {
        0,                // ObjectID         (FLAG: EffectControl-side id unrecovered)
        "PhysicsControl", // mpcTypeName      (FLAG: inferred from class / adjacent GetTypeName tag)
        nullptr,          // mpBaseTypeInfo   (DEFERRED -- EffectControl RTTI chain un-homed)
        nullptr,          // mpfnCreateObject (DEFERRED -- CreateObject not homed in this slice)
    };
    return &sTypeInfo;
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
