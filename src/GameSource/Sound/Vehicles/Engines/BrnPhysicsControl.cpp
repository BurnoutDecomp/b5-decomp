#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Sound/Vehicles/Brn3dCarPosition.h"
#include "GameSource/Sound/Vehicles/Wheels/BrnWheelControl.h"
#include "GameSource/Sound/Vehicles/BrnVehicleStateManager.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Passby/BrnPassbyStateManager.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
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

// =============================================================================
// THE START-LINE REV SEQUENCE (ARTIST 0x826CC0C0 / 0x82684510 / 0x826C8708)
//
// Before a race starts the player's car does not run its live engine model: it
// PLAYS BACK one of six authored rev performances, picked at random, and hands
// back to the live model when the mode reaches E_GMS_IN_PROGRESS. The three
// pieces below are that sequence; none of them had a body in this tree, so the
// car sat at idle on every start line.
//
// The six performances are STATIC DATA in the console image, not generated: the
// picker indexes a stride-16 table of EngRevDataSet records at unk_82F2F508, and
// each record's mpDataPoints is a relocated pointer into .data. Transcribed here
// verbatim from the image (tools/re/x360rd.py), 25/20/16/39/31/31 points at a
// 0.08 s spacing, RPM 1000..9964, throttle 0..1:
//     82F2F508  { 0x19, 82F2E920, 0, 0 }      82F2F538  { 0x27, 82F2EEC8, 0, 0 }
//     82F2F518  { 0x14, 82F2EA50, 0, 0 }      82F2F548  { 0x1F, 82F2F0A0, 0, 0 }
//     82F2F528  { 0x10, 82F2EB40, 0, 0 }      82F2F558  { 0x1F, 82F2F218, 0, 0 }
// The point arrays sit end to end at the 12-byte EngineRevEntry stride
// (82F2E920 + 25*12 == 82F2EA4C, the next array starting at 82F2EA50, and so on),
// which is the independent check that the counts and the stride are right.
// =============================================================================

namespace {
// 82F2E920 -- 25 points
static const PhysicsControl::EngineRevEntry KA_START_LINE_REV_0[25] = {
    { 0.0f, 1000.0f, 0.0f },
    { 0.08f, 1631.0f, 0.83f },
    { 0.16f, 2833.0f, 1.0f },
    { 0.24f, 4250.0f, 1.0f },
    { 0.32f, 5856.0f, 1.0f },
    { 0.4f, 6746.0f, 0.7f },
    { 0.48f, 6098.0f, 0.0f },
    { 0.56f, 5970.0f, 0.3f },
    { 0.64f, 7556.0f, 1.0f },
    { 0.72f, 9337.0f, 1.0f },
    { 0.8f, 9939.0f, 1.0f },
    { 0.88f, 9946.0f, 1.0f },
    { 0.96f, 9964.0f, 1.0f },
    { 1.04f, 9906.0f, 1.0f },
    { 1.12f, 9348.0f, 0.16f },
    { 1.2f, 8678.0f, 0.0f },
    { 1.28f, 7979.0f, 0.0f },
    { 1.36f, 6551.0f, 0.0f },
    { 1.44f, 5830.0f, 0.0f },
    { 1.52f, 5111.0f, 0.0f },
    { 1.6f, 3676.0f, 0.0f },
    { 1.68f, 2959.0f, 0.0f },
    { 1.76f, 2377.0f, 0.0f },
    { 1.84f, 1897.0f, 0.0f },
    { 1.92f, 1000.0f, 0.0f },
};

// 82F2EA50 -- 20 points
static const PhysicsControl::EngineRevEntry KA_START_LINE_REV_1[20] = {
    { 0.0f, 1000.0f, 0.0f },
    { 0.08f, 1052.0f, 0.1f },
    { 0.16f, 2010.0f, 0.96f },
    { 0.24f, 3285.0f, 1.0f },
    { 0.32f, 4770.0f, 1.0f },
    { 0.4f, 6427.0f, 1.0f },
    { 0.48f, 7965.0f, 0.9f },
    { 0.56f, 7571.0f, 0.3f },
    { 0.64f, 6864.0f, 0.0f },
    { 0.72f, 6219.0f, 0.1f },
    { 0.8f, 7544.0f, 0.96f },
    { 0.88f, 9154.0f, 0.9f },
    { 0.96f, 8831.0f, 0.3f },
    { 1.04f, 8154.0f, 0.0f },
    { 1.12f, 6727.0f, 0.0f },
    { 1.2f, 6008.0f, 0.0f },
    { 1.28f, 5291.0f, 0.0f },
    { 1.36f, 3853.0f, 0.0f },
    { 1.44f, 3137.0f, 0.0f },
    { 1.52f, 2193.0f, 0.0f },
};

// 82F2EB40 -- 16 points
static const PhysicsControl::EngineRevEntry KA_START_LINE_REV_2[16] = {
    { 0.0f, 1037.0f, 0.47f },
    { 0.08f, 1192.0f, 0.54f },
    { 0.16f, 1674.0f, 0.72f },
    { 0.24f, 2496.0f, 0.85f },
    { 0.32f, 3506.0f, 0.87f },
    { 0.4f, 4960.0f, 1.0f },
    { 0.48f, 6634.0f, 1.0f },
    { 0.56f, 8375.0f, 0.97f },
    { 0.64f, 8146.0f, 0.12f },
    { 0.72f, 7455.0f, 0.0f },
    { 0.8f, 6022.0f, 0.0f },
    { 0.88f, 5304.0f, 0.0f },
    { 0.96f, 3872.0f, 0.0f },
    { 1.04f, 3272.0f, 0.2f },
    { 1.12f, 2302.0f, 0.0f },
    { 1.2f, 1822.0f, 0.0f },
};

// 82F2EEC8 -- 39 points
static const PhysicsControl::EngineRevEntry KA_START_LINE_REV_3[39] = {
    { 0.0f, 1588.0f, 0.0f },
    { 0.08f, 1546.0f, 0.49f },
    { 0.16f, 1580.0f, 0.5f },
    { 0.24f, 1701.0f, 0.76f },
    { 0.32f, 1983.0f, 0.94f },
    { 0.4f, 2410.0f, 1.0f },
    { 0.48f, 2856.0f, 1.0f },
    { 0.56f, 3318.0f, 1.0f },
    { 0.64f, 3785.0f, 1.0f },
    { 0.72f, 4266.0f, 1.0f },
    { 0.8f, 4759.0f, 1.0f },
    { 0.88f, 5266.0f, 1.0f },
    { 0.96f, 5597.0f, 0.7f },
    { 1.04f, 5379.0f, 0.0f },
    { 1.12f, 5193.0f, 0.28f },
    { 1.2f, 5612.0f, 1.0f },
    { 1.28f, 6160.0f, 1.0f },
    { 1.36f, 6729.0f, 1.0f },
    { 1.44f, 7335.0f, 1.0f },
    { 1.52f, 7977.0f, 0.94f },
    { 1.6f, 7922.0f, 0.7f },
    { 1.68f, 7648.0f, 0.0f },
    { 1.76f, 7363.0f, 0.0f },
    { 1.84f, 7294.0f, 0.3f },
    { 1.92f, 7890.0f, 1.0f },
    { 2.0f, 8600.0f, 1.0f },
    { 2.08f, 9347.0f, 1.0f },
    { 2.16f, 9884.0f, 0.9f },
    { 2.24f, 9648.0f, 0.3f },
    { 2.32f, 9380.0f, 0.0f },
    { 2.4f, 7550.0f, 0.0f },
    { 2.48f, 7170.0f, 0.0f },
    { 2.56f, 5827.0f, 0.0f },
    { 2.64f, 5105.0f, 0.0f },
    { 2.72f, 3339.0f, 0.0f },
    { 2.8f, 3644.0f, 0.0f },
    { 2.88f, 2799.0f, 0.0f },
    { 2.96f, 1847.0f, 0.0f },
    { 3.04f, 1004.0f, 0.0f },
};

// 82F2F0A0 -- 31 points
static const PhysicsControl::EngineRevEntry KA_START_LINE_REV_4[31] = {
    { 0.0f, 1864.0f, 0.96f },
    { 0.08f, 3111.0f, 1.0f },
    { 0.16f, 4570.0f, 1.0f },
    { 0.24f, 6211.0f, 1.0f },
    { 0.32f, 7707.0f, 0.81f },
    { 0.4f, 8127.0f, 0.47f },
    { 0.48f, 8455.0f, 0.49f },
    { 0.56f, 8571.0f, 0.38f },
    { 0.64f, 8611.0f, 0.38f },
    { 0.72f, 8650.0f, 0.38f },
    { 0.8f, 8689.0f, 0.38f },
    { 0.88f, 8731.0f, 0.38f },
    { 0.96f, 8629.0f, 0.24f },
    { 1.04f, 8023.0f, 0.0f },
    { 1.12f, 7315.0f, 0.0f },
    { 1.2f, 6590.0f, 0.0f },
    { 1.28f, 5879.0f, 0.0f },
    { 1.36f, 5290.0f, 0.21f },
    { 1.44f, 6618.0f, 1.0f },
    { 1.52f, 6547.0f, 0.16f },
    { 1.6f, 5839.0f, 0.0f },
    { 1.68f, 7064.0f, 0.83f },
    { 1.76f, 7489.0f, 0.37f },
    { 1.84f, 6814.0f, 0.0f },
    { 1.92f, 6098.0f, 0.0f },
    { 2.0f, 5376.0f, 0.0f },
    { 2.08f, 3947.0f, 0.0f },
    { 2.16f, 3227.0f, 0.0f },
    { 2.24f, 2525.0f, 0.0f },
    { 2.32f, 2187.0f, 0.0f },
    { 2.4f, 1227.0f, 0.0f },
};

// 82F2F218 -- 31 points
static const PhysicsControl::EngineRevEntry KA_START_LINE_REV_5[31] = {
    { 0.0f, 1148.0f, 0.3f },
    { 0.08f, 2207.0f, 1.0f },
    { 0.16f, 3517.0f, 1.0f },
    { 0.24f, 5035.0f, 1.0f },
    { 0.32f, 6716.0f, 1.0f },
    { 0.4f, 8489.0f, 1.0f },
    { 0.48f, 9788.0f, 1.0f },
    { 0.56f, 9508.0f, 0.16f },
    { 0.64f, 8852.0f, 0.0f },
    { 0.72f, 8161.0f, 0.0f },
    { 0.8f, 7450.0f, 0.0f },
    { 0.88f, 6733.0f, 0.0f },
    { 0.96f, 6013.0f, 0.0f },
    { 1.04f, 6603.0f, 0.6f },
    { 1.12f, 8317.0f, 1.0f },
    { 1.2f, 8107.0f, 0.16f },
    { 1.28f, 7414.0f, 0.0f },
    { 1.36f, 7371.0f, 0.3f },
    { 1.44f, 9029.0f, 1.0f },
    { 1.52f, 9892.0f, 1.0f },
    { 1.6f, 9783.0f, 0.7f },
    { 1.68f, 9170.0f, 0.0f },
    { 1.76f, 8492.0f, 0.0f },
    { 1.84f, 7793.0f, 0.0f },
    { 1.92f, 7077.0f, 0.0f },
    { 2.0f, 6360.0f, 0.0f },
    { 2.08f, 5646.0f, 0.0f },
    { 2.16f, 4927.0f, 0.0f },
    { 2.24f, 4209.0f, 0.0f },
    { 2.32f, 3496.0f, 0.0f },
    { 2.4f, 2776.0f, 0.0f },
};

// unk_82F2F508 -- the six EngRevDataSet records the draw indexes (stride 16;
// {mnNumPoints, mpDataPoints, mfTime, mnCurrentPoint}, the last two 0 on disk).
static const PhysicsControl::EngRevDataSet KA_START_LINE_REV_SETS[PhysicsControl::KI_START_LINE_REV_SETS] = {
    PhysicsControl::EngRevDataSet(25, KA_START_LINE_REV_0, 0.0f, 0),   // 82F2E920
    PhysicsControl::EngRevDataSet(20, KA_START_LINE_REV_1, 0.0f, 0),   // 82F2EA50
    PhysicsControl::EngRevDataSet(16, KA_START_LINE_REV_2, 0.0f, 0),   // 82F2EB40
    PhysicsControl::EngRevDataSet(39, KA_START_LINE_REV_3, 0.0f, 0),   // 82F2EEC8
    PhysicsControl::EngRevDataSet(31, KA_START_LINE_REV_4, 0.0f, 0),   // 82F2F0A0
    PhysicsControl::EngRevDataSet(31, KA_START_LINE_REV_5, 0.0f, 0),   // 82F2F218
};
} // namespace


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

    // ARTIST tail @0x826CB6D0..0x826CB704 -- the SAME LCG step the picker inlines,
    // followed by the four-word copy into mEngineDataSet and `stw 0, 0x258(r31)`:
    //     ld r10, 0x20(module + 0x13590)   ; muSeed
    //     mulld/addi                        ; * KU_RANDOM_MULTIPLIER + 1, stored back
    //     v13 = 16 * (highword % 6) + unk_82F2F508
    //     *(a1+600..612) = v13[0..3]        ; mEngineDataSet
    //     *(a1+596) = 0                     ; meIntroRevingState = E_NIS_REVING_STATE_OFF
    // Every attach re-rolls which of the six authored start-line rev performances
    // this car will play, and re-arms the machine at OFF.
    meIntroRevingState = E_NIS_REVING_STATE_OFF;
    mEngineDataSet = PickStartLineRevDataSet();
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
    // ARTIST 0x826CB9D0..0x826CB9EC -- mAcceleration3d is the RAW per-frame velocity
    // DELTA, not a per-second rate:
    //     lvx128 v13, mVelocity3d.cur ; lvx128 v0, r31+0xEC == mVelocity3d.prev
    //     vsubfp v0, v13, v0 ; <mAcceleration3d.prev = cur> ; stvx128 v0, .cur
    // and 0x826CB9F0 lifts {x, z} out of it through the SAME vperm control mask
    // (unk_82CDA450 = 00010203 18191A1B ...) the position/velocity 2d lanes use.
    // The subtract is a full four-lane vsubfp, so the unused w lane travels too.
    {
        const Vector3& lrVelCur  = lrData.mVelocity3d.GetCurrent();
        const Vector3& lrVelPrev = lrData.mVelocity3d.GetPrevious();
        Vector3 lAcceleration3d = { lrVelCur.x - lrVelPrev.x,
                                    lrVelCur.y - lrVelPrev.y,
                                    lrVelCur.z - lrVelPrev.z,
                                    lrVelCur.w - lrVelPrev.w };
        lrData.mAcceleration3d.Update(lAcceleration3d);
        Vector2 lAcceleration2d = { lAcceleration3d.x, lAcceleration3d.z, 0.0f, 0.0f };
        lrData.mAcceleration2d.Update(lAcceleration2d);
    }

    // ARTIST 0x826CBA0C..0x826CBA44 -- mAccelerationMagnitude IS a per-second rate,
    // and is left ALONE on a zero time step (the inlined RwMathFPU::IsZero pair,
    // flt_820AA114 = +1.1920929e-7 and flt_82002514 = -1.1920929e-7):
    //     if (!IsZero(dt)) { prev = cur; cur = (velMag.cur - velMag.prev) / dt; }
    if (afTimeStep > 1.1920929e-7f || afTimeStep < -1.1920929e-7f)
    {
        lrData.mAccelerationMagnitude.Update(
            (lrData.mVelocityMagnitude.GetCurrent()
             - lrData.mVelocityMagnitude.GetPrevious()) / afTimeStep);
    }

    // ARTIST 0x826CBB80..0x826CBCBC -- mYaw is the SLIP ANGLE IN DEGREES between the
    // car's forward axis and the direction it is actually travelling, folded onto
    // [0, 90]:
    //     ; gate: fabs(mVelocityMagnitude.cur) > flt_82F2FD2C (0.15), else angle = 0
    //     lvx128 v13, raw + 0x330            ; RaceCarState::mLinearVelocity  (@816)
    //     <vrsqrtefp + two Newton steps>     ; normalize it
    //     lvx128 v12, raw + 0x210            ; RaceCarState::mTransform.zAxis (496+0x20)
    //     vmsum3fp128 v0, v12, v0            ; dot3(forward, velocityDirection)
    //     bl  XMVectorACos
    //     fmuls * flt_820AA0E8 (57.29578)    ; radians -> degrees
    //     if (angle > flt_82004F64 90.0) angle = flt_820025FC 180.0 - angle
    //     mYaw.Update(angle)
    // Its consumer is BrnSkidEffect.cpp:163, which had been reading a constant 0
    // because nothing in this tree ever wrote mYaw.
    // FLAG: the console reaches acos through XMVectorACos @0x821F0980, whose contract
    // is "each component should be between -1.0 and 1.0"; std::acos is UNDEFINED
    // outside that, so the dot is clamped to the domain before the call. That clamp
    // is the host libm's domain requirement, not a behavioural arm -- for a
    // normalized vector the dot only leaves [-1,1] by float rounding.
    {
        f32 lfYawDegrees = 0.0f;
        if (std::fabs(lrData.mVelocityMagnitude.GetCurrent()) > 0.15f)
        {
            const Vector3& lrVelocity = lrRaw.mLinearVelocity;
            const f32 lfLengthSquared = lrVelocity.x * lrVelocity.x
                                      + lrVelocity.y * lrVelocity.y
                                      + lrVelocity.z * lrVelocity.z;
            const f32 lfInverseLength = lfLengthSquared > 0.0f
                ? 1.0f / std::sqrt(lfLengthSquared) : 0.0f;
            const Vector3& lrForward = lrRaw.mTransform.At();
            f32 lfDot = (lrForward.x * lrVelocity.x
                       + lrForward.y * lrVelocity.y
                       + lrForward.z * lrVelocity.z) * lfInverseLength;
            lfDot = (std::max)(-1.0f, (std::min)(1.0f, lfDot));
            lfYawDegrees = std::acos(lfDot) * 57.29578f;
            if (lfYawDegrees > 90.0f)
                lfYawDegrees = 180.0f - lfYawDegrees;
        }
        lrData.mYaw.Update(lfYawDegrees);
    }
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

    // 0x826CC050..0x826CC064 -- the three primary-vtable per-frame hooks, in order.
    // Slot +12 is the bare `blr` that is folded-empty in both vtables (see the header),
    // so there is nothing between these two.
    UpdateCollisionPassbys(afTimeStep);
    UpdateStartLineReving(afTimeStep);

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
// PhysicsControl::PickStartLineRevDataSet  @ 0x82684510
//
//   lwz    r11, 0x2C(r4)                 ; mpLogicModule
//   addis  r11, r11, 1 ; addi r11, r11, 0x3590    ; + 0x13590 == &mRandomGenerator
//   ld     r10, 0x20(r11)                ; muSeed (the OLD value)
//   lis/ori/insrdi                       ; r8 = 0x5851F42D4C957F2D == KU_RANDOM_MULTIPLIER
//   mulld  r8, r10, r8 ; addi r8, r8, 1  ; next = seed * K + 1
//   srdi   r6, r10, 32                   ; the OLD seed's HIGH word
//   std    r8, 0x20(r11)
//   mulhwu/srwi/slwi/subf                ; r11 = high % 6   (the 0xAAAAAAAB reciprocal)
//   slwi   r11, r11, 4 ; add r11, r11, r9 ; entry = unk_82F2F508 + 16 * index
//   lwz x4 / stw x4                      ; copy the 16-byte EngRevDataSet out
//
// The step does NOT touch the ring buffer and the value used is the seed BEFORE
// the step -- the same shape as CgsNumeric::Random::RandomBool (which is why this
// class holds that header's friend grant rather than reaching muSeed by offset).
// ---------------------------------------------------------------------------
PhysicsControl::EngRevDataSet PhysicsControl::PickStartLineRevDataSet()
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    CGS_ASSERT(lpModule != nullptr, "lpLogicModule");

    CgsNumeric::Random& lrRandom = lpModule->GetRandomGenerator();
    const u64 luOldSeed = lrRandom.muSeed;
    lrRandom.muSeed = luOldSeed * CgsNumeric::KU_RANDOM_MULTIPLIER + 1ull;

    const u32 luDraw = static_cast<u32>(luOldSeed >> 32);
    const u32 luIndex = luDraw % static_cast<u32>(KI_START_LINE_REV_SETS);
    return KA_START_LINE_REV_SETS[luIndex];
}

// ---------------------------------------------------------------------------
// PhysicsControl::UpdateEngRevDataSet  @ 0x826C8708
//
//   if (!mpDataPoints || mnCurrentPoint >= mnNumPoints)
//       return mpDataPoints[mnCurrentPoint];             ; 0x826C8758.. (verbatim copy)
//   ; else bracket [cur, cur+1] and interpolate BOTH lanes over the same x pair:
//   sub_826BF998(&rpmSlope,      p[cur].mfTime, p[cur+1].mfTime, p[cur].mfRpm,      p[cur+1].mfRpm)
//   sub_826BF998(&throttleSlope, p[cur].mfTime, p[cur+1].mfTime, p[cur].mfThrottle, p[cur+1].mfThrottle)
//   mfTime += dt ; if (mfTime > p[cur+1].mfTime) mnCurrentPoint = cur + 1
//   result.mfTime     = mfTime
//   result.mfThrottle = clamp01((mfTime - x0)/(x1 - x0)) * (y1 - y0) + y0     ; 0x826C886C
//   result.mfRpm      = same with the rpm pair                               ; 0x826C8874
//
// sub_826BF998 is the four-float CgsSound::Utils::Slope ctor shape -- it stores
// {minIn, maxIn, minOut, maxOut} and nudges maxIn by 1e-6 when the input span is
// degenerate, the same guard the committed Slope(const SlopeParams&) @0x826A1F70
// carries. Reproduced inline here (as the console emits it) rather than through
// Slope, whose 4-float ctor / GetValue have no body in this tree.
// ⚠️ The clamp is the two-fsel idiom (0x826C8850..0x826C8868): fsel(-u, 0, u) then
// fsel(1 - u, u, 1), i.e. max then min, in that order.
// ---------------------------------------------------------------------------
PhysicsControl::EngineRevEntry PhysicsControl::UpdateEngRevDataSet(
    EngRevDataSet& arSet, f32 afTimeStep)
{
    const EngineRevEntry* lpPoints = arSet.mpDataPoints;
    if (lpPoints == nullptr || arSet.mnCurrentPoint >= arSet.mnNumPoints)
    {
        // Non-gating tripwire: the console reads mpDataPoints[mnCurrentPoint] on this
        // arm WITHOUT a null check (0x826C8758 `add r11, r11, r9` with r9 possibly 0),
        // and its only caller re-picks the set before every call, so the null half is
        // unreachable. Kept verbatim rather than guarded with an arm the console lacks.
        CGS_ASSERT(lpPoints != nullptr, "mpDataPoints");
        return lpPoints[arSet.mnCurrentPoint];
    }

    const EngineRevEntry& lrThis = lpPoints[arSet.mnCurrentPoint];
    const EngineRevEntry& lrNext = lpPoints[arSet.mnCurrentPoint + 1];

    // sub_826BF998 twice: the shared time span, then the two output pairs.
    f32 lfMinTime = lrThis.mfTime;
    f32 lfMaxTime = lrNext.mfTime;
    if (std::fabs(lfMaxTime - lfMinTime) < 0.000001f)
        lfMaxTime += 0.000001f;

    arSet.mfTime += afTimeStep;
    if (arSet.mfTime > lrNext.mfTime)
        arSet.mnCurrentPoint = arSet.mnCurrentPoint + 1;

    const f32 lfRaw = (arSet.mfTime - lfMinTime) / (lfMaxTime - lfMinTime);
    const f32 lfLow = (-lfRaw >= 0.0f) ? 0.0f : lfRaw;              // fsel(-u, 0, u)
    const f32 lfUnit = ((1.0f - lfLow) >= 0.0f) ? lfLow : 1.0f;     // fsel(1-u, u, 1)

    EngineRevEntry lResult;
    lResult.mfTime = arSet.mfTime;
    lResult.mfRpm = lfUnit * (lrNext.mfRpm - lrThis.mfRpm) + lrThis.mfRpm;
    lResult.mfThrottle = lfUnit * (lrNext.mfThrottle - lrThis.mfThrottle) + lrThis.mfThrottle;
    return lResult;
}

// ---------------------------------------------------------------------------
// PhysicsControl::UpdateCollisionPassbys  @ 0x826B2628  (primary vtable slot +8)
//
// The car's own body-roll oscillator, and the COLLISION PASSBY it fires on each
// rising zero crossing of it -- the "whoosh" as a car that has just landed rolls
// past the microphone.
//
//   lwz    r11, 0x38(r31)              ; mpVehiclePhysicsData
//   lvx128 v13, r11, 0x340             ; RaceCarState::mAngularVelocity (@832)
//   vmsum3fp128 + vrsqrtefp + 2 Newton ; f29 = |mAngularVelocity|
//   lfs    f0, 0x250(r31) / stfs f13, 0x254(r31)      ; accumulator.prev = cur
//   fmadds f0, f29, f1, f0 / stfs f0, 0x250(r31)      ; accumulator.cur += |w| * dt
//   fmuls  f1, f0, flt_82001D9C (2.0) / bl sin
//   lfs    f0, 0x248(r31) / stfs f0, 0x24C(r31)       ; oscillator.prev = cur
//   frsp / stfs f0, 0x248(r31)                        ; oscillator.cur = sin(acc * 2)
//   <debug tty dump, gated on dword_82FFB85C -- 0 in the shipped image; not carried>
//   if (|w| > flt_82F2FD08 0.25
//       && mpWheelControl->mfTimeSinceLanding > flt_82F2FD04 0.5
//       && oscillator.cur != 0 && oscillator.cur >= 0
//       && oscillator.prev != 0 && oscillator.prev < 0)
//       PostPassby(env.GetStateManager(4), Passby{ Vector3(0), mp3dCarControl,
//                                                  |w|, Collision(12), false, 1.0f })
//
// +0x1DC / +0x1E0 on WheelControl are named by WheelControl::UpdateParams
// @0x826D071C..0x826D0760: on the `mIsOnGround.cur != 0` arm +0x1E0 accumulates dt
// and +0x1DC is zeroed, and on the airborne arm the reverse -- i.e. +0x1DC is
// mfTimeInAir and +0x1E0 is mfTimeSinceLanding, in the tail declaration order this
// header already carries. GetTimeSinceLanding() is the existing accessor.
//
// The manager is mapStateManagers[4]: the console loads `*(mpLogicModule + 0x2964)`,
// the Environment sits at module + 0x2950 with mpAllocator at +0 and the map at +4
// (CgsEnvironment.h), so +0x2964 is slot 4 -- and PassbyStateManager's own
// ClassTypeInfo ObjectID is 4 (BrnPassbyStateManager.cpp:238), with slot 1 landing
// on PlayerVehicleStateManager exactly as Attach's `*(module + 10584)` does.
// ⚠️ PassbyStateManager::Prepare @0x826F9748 is still a `return true` stub with no
// states, so the posted record is queued and nothing consumes it yet -- this lands
// the producer, not the sound.
// ---------------------------------------------------------------------------
void PhysicsControl::UpdateCollisionPassbys(f32 afTimeStep)
{
    const BrnSound::Vehicles::VehicleData* lpRaw = mpVehiclePhysicsData;
    const Vector3& lrAngularVelocity = lpRaw->mAngularVelocity;
    const f32 lfAngularSpeed = std::sqrt(
        lrAngularVelocity.x * lrAngularVelocity.x +
        lrAngularVelocity.y * lrAngularVelocity.y +
        lrAngularVelocity.z * lrAngularVelocity.z);

    mfAngularVelocityAccumulator.Update(
        mfAngularVelocityAccumulator.GetCurrent() + lfAngularSpeed * afTimeStep);
    mfOscillator.Update(
        std::sin(mfAngularVelocityAccumulator.GetCurrent() * 2.0f));

    if (!(lfAngularSpeed > 0.25f))
        return;
    if (mpWheelControl == nullptr)
        return;
    if (!(mpWheelControl->GetTimeSinceLanding() > 0.5f))
        return;

    // The rising zero crossing: `fcmpu beq` then `blt` on the current value, and
    // `fcmpu beq` then `bge` on the previous -- a bare `> 0` / `< 0` would let the
    // exact-zero frame through, which the console explicitly excludes on both.
    const f32 lfOscillator = mfOscillator.GetCurrent();
    const f32 lfPrevOscillator = mfOscillator.GetPrevious();
    if (lfOscillator == 0.0f || lfOscillator < 0.0f)
        return;
    if (lfPrevOscillator == 0.0f || lfPrevOscillator >= 0.0f)
        return;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    BrnSound::Logic::Passby::PassbyStateManager* lpPassbyStateManager =
        static_cast<BrnSound::Logic::Passby::PassbyStateManager*>(
            lpModule->GetEnvironment().GetStateManager(4));
    if (lpPassbyStateManager == nullptr)
        return;

    const BrnSound::Logic::Passby::PassbyStateManager::Passby lPassby(
        static_cast<const CgsSound::Logic::Cgs3dEffectControl*>(mp3dCarControl),
        lfAngularSpeed,
        AttribSys::Enums::ePassbyTypes::Collision,
        false,
        1.0f);
    lpPassbyStateManager->PostPassby(lPassby);
}

// ---------------------------------------------------------------------------
// PhysicsControl::UpdateStartLineReving  @ 0x826CC0C0  (primary vtable slot +16)
//
// A three-state machine over meIntroRevingState, gated on the CURRENT game-mode
// state published in the GameModeOutputInterface's +0xC word (see
// BrnRootSoundModuleIo.h): E_GMS_COUNTDOWN(0) / E_GMS_INTRO(1) are the start line,
// E_GMS_IN_PROGRESS(2) and beyond are the race.
//
//   OFF (0)        0x826CC3D8: state = OFF; if (mode is 0 or 1) { state = STARTLINE;
//                  mEngineDataSet = PickStartLineRevDataSet(); }
//   STARTLINE (1)  0x826CC220: state = STARTLINE; re-pick the performance when the
//                  current one is null or exhausted; step it and DRIVE THE PHYSICS
//                  DATA from the authored sample -- mThrottle and mNormalizedRpm are
//                  overwritten, mDeltaThrottle is Flush'd to the throttle delta and
//                  mGear is Flush'd to 1. When the mode has reached >= 2, step the
//                  performance once more, arm mEngineStartLineRPM as a 0.5 s
//                  (flt_82F2CC24 500.0 * flt_82013F90 0.001, floor flt_82002138 0.01)
//                  curve-2 ramp from that sample's RPM to the live normalized RPM,
//                  and go to RESUMING.
//   RESUMING (2)   0x826CC180: run that ramp each frame with its finish re-aimed at
//                  the live normalized RPM, publish it into mNormalizedRpm, and drop
//                  to OFF once the ramp completes.
//
// ⚠️ THE GATE IS FED BY A PARKED PRODUCER. Nothing in this tree writes
// GameStateModuleIO::OutputBuffer + 176344 (BrnModeManager_WorldTick.cpp:728), so
// the mode word reads 0 == E_GMS_COUNTDOWN permanently and this machine sits in
// STARTLINE. See the report; the layout half of that park is lifted by the typed
// GameModeOutputInterface this file now reads.
// ---------------------------------------------------------------------------
void PhysicsControl::UpdateStartLineReving(f32 afTimeStep)
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    CGS_ASSERT(lpModule != nullptr, "lpLogicModule");
    const BrnSound::Module::Io::RootInputBuffer::GameModeOutputInterface*
        lpGameModeInterface = lpModule->GetBrnInputStructure()->GetGameModeInterface();
    CGS_ASSERT(lpGameModeInterface != nullptr, "lpGameModeInterface");
    const s32 liModeState = lpGameModeInterface->miCurrentGameModeState;

    PhysicsData& lrData = mProcessedPhysicsData;

    if (meIntroRevingState < E_NIS_REVING_STATE_STARTLINE)           // 0x826CC3D8
    {
        meIntroRevingState = E_NIS_REVING_STATE_OFF;
        if (liModeState == 1 || liModeState == 0)
        {
            meIntroRevingState = E_NIS_REVING_STATE_STARTLINE;
            mEngineDataSet = PickStartLineRevDataSet();
        }
        return;
    }

    if (meIntroRevingState == E_NIS_REVING_STATE_STARTLINE)          // 0x826CC220
    {
        meIntroRevingState = E_NIS_REVING_STATE_STARTLINE;
        if (mEngineDataSet.mpDataPoints == nullptr
            || mEngineDataSet.mnCurrentPoint >= mEngineDataSet.mnNumPoints)
        {
            mEngineDataSet = PickStartLineRevDataSet();
        }

        const EngineRevEntry lSample = UpdateEngRevDataSet(mEngineDataSet, afTimeStep);

        // 0x826CC2A8 / 0x826CC2C0: Update(GetPrevious()) on both DataPoints -- the
        // console's own swap-then-push, so the sample lands as the current value and
        // the genuine previous frame's value survives as the previous.
        lrData.mThrottle.Update(lrData.mThrottle.GetPrevious());
        lrData.mNormalizedRpm.Update(lrData.mNormalizedRpm.GetPrevious());
        lrData.mThrottle.Update(lSample.mfThrottle);
        // 0x826CC2E4..0x826CC308: every mDeltaThrottle slot takes the same delta and
        // the cursor resets -- Average<5,f32>::Flush.
        lrData.mDeltaThrottle.Flush(lrData.mThrottle.GetCurrent() - lrData.mThrottle.GetPrevious());
        lrData.mNormalizedRpm.Update(lSample.mfRpm);
        // 0x826CC320: `stw r29(1), 0x44/0x48(r31)` -- both halves of mGear take 1.
        lrData.mGear.Flush(1);

        if (liModeState == 1 || liModeState == 0)
            return;

        // 0x826CC334: mode >= E_GMS_IN_PROGRESS -- arm the hand-back ramp.
        const EngineRevEntry lHandover = UpdateEngRevDataSet(mEngineDataSet, afTimeStep);
        const f32 lfUnityRpm = UnityPhysicsRpm(GetRawPhysicsData()->mfRPM);
        f32 lfLength = 500.0f * 0.001f;                 // flt_82F2CC24 * flt_82013F90
        if (!(lfLength > 0.0f))
            lfLength = 0.01f;                           // flt_82002138
        mEngineStartLineRPM.mfLength = lfLength;
        mEngineStartLineRPM.mfFinish = lfUnityRpm * 9000.0f + 1000.0f;
        mEngineStartLineRPM.mbComplete = false;
        mEngineStartLineRPM.mfStart = lHandover.mfRpm;
        mEngineStartLineRPM.mfCurrentValue = lHandover.mfRpm;
        mEngineStartLineRPM.mfElapsedTime = 0.0f;
        mEngineStartLineRPM.meCurveTypes = static_cast<CgsSound::Utils::Curve::ECurveType>(2);
        meIntroRevingState = E_NIS_REVING_STATE_RESUMING;
        return;
    }

    if (static_cast<s32>(meIntroRevingState) >= 3)                   // 0x826CC178
        return;

    // RESUMING (2)                                                  // 0x826CC180
    if (mEngineStartLineRPM.IsFinished())
    {
        meIntroRevingState = E_NIS_REVING_STATE_OFF;
        return;
    }

    const f32 lfLiveNormalizedRpm =
        UnityPhysicsRpm(GetRawPhysicsData()->mfRPM) * 9000.0f + 1000.0f;
    mEngineStartLineRPM.mfFinish = lfLiveNormalizedRpm;
    mEngineStartLineRPM.Update(afTimeStep);
    if (mEngineStartLineRPM.IsFinished())
        mEngineStartLineRPM.mfCurrentValue = lfLiveNormalizedRpm;

    lrData.mNormalizedRpm.Update(lrData.mNormalizedRpm.GetPrevious());
    lrData.mNormalizedRpm.Update(mEngineStartLineRPM.GetValueFloat());
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
