#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Sound/Vehicles/Brn3dCarPosition.h"
#include "GameSource/Sound/Vehicles/Wheels/BrnWheelControl.h"
#include "GameSource/Sound/Vehicles/BrnVehicleStateManager.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"

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

    const s32 liGear = static_cast<s32>(lrRaw.mi8Gear);
    lrData.mbJustShifted = lrData.mGear.GetCurrent() != liGear;
    lrData.mGear.Update(liGear);
    lrData.mfDurationInGear = lrData.mbJustShifted ? 0.0f : lrData.mfDurationInGear + afTimeStep;

    // ARTIST keeps the PhysicsData ctor's 7000/997 normalization limits and
    // applies the authored PhysicsRpmMap cubic.  The former interim body replaced
    // these every frame with the current gear's upshift RPM and a linear map,
    // which drove the audio controller to redline much too early.
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
    lrData.mSpeedMPH.Update(lrRaw.mfSpeedMPH);
    lrData.mSpeedMPS.Update(lrRaw.mfSpeedMPH * 0.44704f);
    lrData.mDrifting.Update(lrRaw.mfAbsDriftScale);
    lrData.mfTimeSinceRespawn += afTimeStep;

    SetMixerInputValue(4, lrRaw.mbCrashing ? 0x7FFF : 0);
    SetMixerInputValue(8, lrRaw.mbIsDriveable ? 0x7FFF : 0);
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
