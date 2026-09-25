// FX-DIRECTOR (crash parity 2026-09-24): Camera::BehaviourFixedCam is a REAL Camera::Behaviour.
// The PRODUCTION src/GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.cpp (and Behaviour.cpp)
// compiled against the revision's own header, with recording stand-ins for the out-of-line leaves, and
// driven the way the behaviour manager drives a pooled behaviour: placement-new into raw storage,
// then EVERY call through a Behaviour* (BehaviourHelper::Prepare @0x82255F48 dispatches slot 0
// Construct; PrepareBehaviours slot 1; BehaviourHelper::Update slot 2). Before the fix the class had
// no base and no vtable, and that first dispatch read a null vptr (the moment tick's AV).
//
// Checked against the ARTIST asm:
//   Construct @0x82229D20  Behaviour::Construct; VisibilityCollisionPolicy::Construct over +0x20; a
//                          SECOND `stb 1` to policy +0x1A0 (SetTestLookingAt(true)); mpParameters = 0
//   Prepare   @0x821FAD28  SetPrepared; mbPositionSet (+0x2A8) = 0; li r3, 1
//   Update    @0x82229DE0  SetTarget(player transform +0x250, AABB +0x500, entity +0x428); once:
//                          offset = velocity (+0x390) with Y zeroed * 1.0; > 400 -> Fail(7); < 25 ->
//                          NormalizeFast * 5; zero -> zAxis (+0x270) * 5; CreateLookAt(pos + offset,
//                          pos); dutch = RandomFloat(-max, max) (+0x5D4); roll by it unless tweaker;
//                          not failed -> flag bit 1; visibility interrupted -> NoCutTo 16 + !switchTo;
//                          SetFOV(mfFOV); SetTransform; ValidateTransformWithDebugInfo; tweaker -> roll
//                          by mfMaxDutch; return 1
//   GetCollisionPolicy @0x821FB588 (addi r3, r3, 0x20); GetName @0x821FAE48;
//   SetupTweaker @0x821FAD48 ("Roll" -> +0x0C [0][7], "FOV" -> +0x08 [0][6], scale 0.5)
//   Parameters::Construct (bank 0x8223DC90: type 15, name 0, FOV 70.0, max dutch 10.0)
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <type_traits>
#include <vector>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static std::string gLastAssert;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; gLastAssert = lpcText; return 0; }
    void* EndAssert() { return nullptr; }
}
}

// BehaviourSharedInfo holds a VehicleInfo by value (its RaceCarState lives in the physics TU).
void BrnPhysics::Vehicle::RaceCarState::Clear() { std::memset(static_cast<void*>(this), 0, sizeof(*this)); }

using namespace BrnDirector;
using namespace BrnDirector::Camera;
using rw::math::vpu::Matrix44Affine;
using rw::math::vpu::Vector3;

// ---- recording stand-ins for the leaves -----------------------------------------------------------
static int giPolicyConstructs = 0, giSetTargets = 0, giLookAts = 0, giRandomDraws = 0, giValidates = 0;
static int giTweakerConstructs = 0, giFailFlag = -1, giNoCutToFlag = -1, giSetFlagCalls = 0;
static Matrix44Affine gLastTargetTransform;
static AABBox gLastTargetAABB;
static u32 guLastTargetEntity = 0;
static Vector3 gLastEye, gLastLookTarget;
static f32 gfRandomMin = 0.0f, gfRandomMax = 0.0f;
struct TweakRecord { std::string mName; f32* mpfVariable; f32 mfScale; int miAxis; int miMap; };
static std::vector<TweakRecord> gTweaks;

namespace BrnDirector
{
namespace Camera
{
    // The console Construct's visibility-test bytes are 1/0/1 (policy +0x1A0..+0x1A2 = the embedded
    // VisibilityTest's mbTestLookingAt / mbOccluded / mbIsOnScreen); this stand-in leaves the first at 0,
    // so only the behaviour's own SetTestLookingAt(true) can raise it.
    void VisibilityCollisionPolicy::Construct()
    {
        ++giPolicyConstructs;
        mVisibilityTest.mbTestLookingAt = false;
        mVisibilityTest.mbOccluded      = false;
        mVisibilityTest.mbIsOnScreen    = true;
    }
    void VisibilityCollisionPolicy::SetTarget(Matrix44Affine lTargetTransform, AABBox lTargetAABB,
                                              CgsSceneManager::EntityId lTargetEntityId)
    {
        ++giSetTargets;
        gLastTargetTransform = lTargetTransform;
        gLastTargetAABB = lTargetAABB;
        guLastTargetEntity = static_cast<u32>(lTargetEntityId);
    }
    // The policy's two per-frame scene-query virtuals (vtable slots 0/1, @0x822402F8 / @0x82224530; bodies in
    // BrnVisibilityCollisionPolicy.cpp, not compiled here). The behaviour under test never calls them --
    // BehaviourManager's collision pass does -- so they are defined only for the policy's vtable to link.
    void VisibilityCollisionPolicy::GenerateSceneQueries(const CollisionPolicySharedInfo&, Camera&) {}
    void VisibilityCollisionPolicy::ProcessSceneQueryResults(const CollisionPolicySharedInfo&, Camera&) {}
    void ValidityAccount::SetFlag(s32 leFlag) { ++giSetFlagCalls; giFailFlag = leFlag; }
    void ValidityAccount::SetNoCutFromFlag(s32) {}
    void ValidityAccount::SetNoCutToFlag(s32 leFlag) { giNoCutToFlag = leFlag; }
    void CameraState::ClearFlag(u32) {}
    void Camera::SetFOV(f32 lfFOV) { CGS_ASSERT(lfFOV > 0.0f, "lfFOV > 0.0f"); mfFOV = lfFOV; }
    void Camera::SetTransform(const Matrix44Affine& lrTransform) { mTransform = lrTransform; }
    Matrix44Affine* Camera::ValidateTransformWithDebugInfo() { ++giValidates; return &mTransform; }
namespace Utils
{
    // A deterministic stand-in look-at: the eye in the translation row, the target in the Z row.
    Matrix44Affine CreateLookAt(Vector3 lEyePosition, Vector3 lTargetPosition)
    {
        ++giLookAts;
        gLastEye = lEyePosition;
        gLastLookTarget = lTargetPosition;
        Matrix44Affine lResult;
        lResult.xAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
        lResult.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
        lResult.zAxis = Vector3{ lTargetPosition.x - lEyePosition.x, lTargetPosition.y - lEyePosition.y,
                                 lTargetPosition.z - lEyePosition.z, 0.0f };
        lResult.wAxis = lEyePosition;
        return lResult;
    }
    void Tweaker::Construct() { ++giTweakerConstructs; }
    void Tweaker::AddMapping(const char* lpcName, f32* lpfVariableToTweak, f32 lfScale, EAxis leAxisToMapTo, EMap leMap)
    {
        gTweaks.push_back(TweakRecord{ lpcName, lpfVariableToTweak, lfScale, static_cast<int>(leAxisToMapTo),
                                       static_cast<int>(leMap) });
    }
}
}
}

namespace CgsNumeric
{
    // Draws a quarter of the way up the range, so the dutch it returns is known.
    f32 Random::RandomFloat(f32 lfMin, f32 lfMax)
    {
        ++giRandomDraws;
        gfRandomMin = lfMin;
        gfRandomMax = lfMax;
        return lfMin + (lfMax - lfMin) * 0.25f;
    }
}

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) < 1e-4f; }
static bool NearV(const Vector3& lrA, const Vector3& lrB) { return Near(lrA.x, lrB.x) && Near(lrA.y, lrB.y) && Near(lrA.z, lrB.z); }
static bool NearM(const Matrix44Affine& lrA, const Matrix44Affine& lrB)
{
    return NearV(lrA.xAxis, lrB.xAxis) && NearV(lrA.yAxis, lrB.yAxis) && NearV(lrA.zAxis, lrB.zAxis) &&
           NearV(lrA.wAxis, lrB.wAxis);
}

// The behaviour manager's view of a pooled behaviour: storage, placement-new, a Behaviour*.
alignas(16) static u8 gaSlot[sizeof(BehaviourFixedCam) + 16];
alignas(16) static u8 gaRandom[sizeof(CgsNumeric::Random) + 16];

static BehaviourFixedCam* Pool()
{
    std::memset(gaSlot, 0, sizeof(gaSlot));
    return new (gaSlot) BehaviourFixedCam();
}

int main()
{
    Check(std::is_base_of<Behaviour, BehaviourFixedCam>::value && std::is_polymorphic<BehaviourFixedCam>::value,
          "BehaviourFixedCam derives from Camera::Behaviour (DWARF BrnBehaviourFixedCam.h:52) and has a vtable");

    // ---- Parameters::Construct + SetParameters -------------------------------------------------
    static BehaviourFixedCam::Parameters lParams;
    std::memset(static_cast<void*>(&lParams), 0xCD, sizeof(lParams));
    lParams.Construct();
    Check(lParams.GetType() == static_cast<u32>(eBehaviourFixedCam) && lParams.GetDebugName() == nullptr &&
          lParams.mfFOV == 70.0f && lParams.mfMaxDutch == 10.0f,
          "Parameters::Construct: type 15, no debug name, FOV 70.0, max dutch 10.0 (bank 0x8223DC90 +9012..+9024)");
    lParams.SetDebugName("Fixed Cam Default");

    static BehaviourSharedInfo lInfo;
    static BrnDirector::Camera::Camera lCamera;
    lInfo.mpRandom = reinterpret_cast<CgsNumeric::Random*>(gaRandom);
    BrnPhysics::Vehicle::RaceCarState& lrPlayer = lInfo.mPlayerInfo.mRaceCarState;
    lrPlayer.mTransform.xAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
    lrPlayer.mTransform.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
    lrPlayer.mTransform.zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
    lrPlayer.mTransform.wAxis = Vector3{ 100.0f, 5.0f, 200.0f, 1.0f };
    lrPlayer.mEntityId.muValue = 0x1234;
    lInfo.mPlayerInfo.mAABB.mMin = Vector3{ -1.0f, -0.5f, -2.0f, 0.0f };
    lInfo.mPlayerInfo.mAABB.mMax = Vector3{ 1.0f, 0.5f, 2.0f, 0.0f };
    BehaviourSharedPrepareReleaseInfo lPrepareInfo = {};

    // ---- 1. slot 0 / slot 1 through a Behaviour* ------------------------------------------------
    BehaviourFixedCam* lpFixed = Pool();
    Behaviour* lpBehaviour = lpFixed;
    lpBehaviour->Construct();
    Check(giPolicyConstructs == 1, "Construct (slot 0, dispatched through Behaviour*): VisibilityCollisionPolicy::Construct");
    Check(lpFixed->mCollisionPolicy.mVisibilityTest.mbTestLookingAt,
          "Construct: SetTestLookingAt(true) after the policy's Construct (the second stb 1 to policy +0x1A0)");
    Check(lpFixed->mpParameters == nullptr && !lpFixed->mbIsPrepared && !lpFixed->mbHasFailed,
          "Construct: mpParameters = 0 and the base's Construct (not prepared, not failed)");
    Check(std::string(lpBehaviour->GetName()) == "BehaviourFixedCam", "GetName (slot 7) @0x821FAE48");
    Check(lpBehaviour->GetCollisionPolicy() == &lpFixed->mCollisionPolicy,
          "GetCollisionPolicy (slot 5) returns the embedded policy (addi r3, r3, 0x20)");

    lpFixed->SetParameters(&lParams);
    Check(lpFixed->mpParameters == &lParams && std::string(lpFixed->GetDebugParametersName()) == "Fixed Cam Default",
          "SetParameters: stores the block (+0x2A0) and its debug name (+0x10)");

    lpFixed->mbPositionSet = true;
    Check(lpBehaviour->Prepare(lPrepareInfo) && lpFixed->mbIsPrepared && !lpFixed->mbPositionSet,
          "Prepare (slot 1): SetPrepared, mbPositionSet = 0 (the next Update plants the shot), true");

    // ---- 2. Update: a normal shot (|offset|^2 = 100, in [25, 400]) ------------------------------
    lrPlayer.mLinearVelocity = Vector3{ 10.0f, 3.0f, 0.0f, 0.0f };
    const bool lbUpdated = lpBehaviour->Update(lCamera, lInfo);
    Check(lbUpdated, "Update (slot 2) returns true");
    Check(giSetTargets == 1 && NearM(gLastTargetTransform, lrPlayer.mTransform) && guLastTargetEntity == 0x1234 &&
          NearV(gLastTargetAABB.mMin, lInfo.mPlayerInfo.mAABB.mMin) && NearV(gLastTargetAABB.mMax, lInfo.mPlayerInfo.mAABB.mMax),
          "Update: the policy's target is the player's transform, AABB and entity id");
    Check(giLookAts == 1 && NearV(gLastEye, Vector3{ 110.0f, 5.0f, 200.0f, 0.0f }) &&
          NearV(gLastLookTarget, lrPlayer.mTransform.wAxis),
          "Update: eye = car + velocity with Y zeroed (10,0,0), looking at the car");
    Check(giRandomDraws == 1 && gfRandomMin == -10.0f && gfRandomMax == 10.0f && Near(lpFixed->mfDutch, -5.0f),
          "Update: mfDutch = RandomFloat(-mfMaxDutch, mfMaxDutch)");
    const Matrix44Affine lPlanted = rw::math::vpu::Mult(rw::math::vpu::MakeRotationZ(-5.0f * 0.017453292f),
                                                        Utils::CreateLookAt(Vector3{ 110.0f, 5.0f, 200.0f, 0.0f },
                                                                            lrPlayer.mTransform.wAxis));
    --giLookAts;
    Check(NearM(lpFixed->mBaseTranform, lPlanted) && NearM(lCamera.mTransform, lPlanted),
          "Update: the planted transform is rolled by the dutch about Z (Mult(RotZ, lookAt)) and published");
    Check(lCamera.mfFOV == 70.0f && giValidates == 1, "Update: Camera::SetFOV(mfFOV) and ValidateTransformWithDebugInfo");
    Check((lCamera.mState_uFlags & 2) != 0 && giFailFlag == -1, "Update: not failed -> camera state flag bit 1, no Fail");
    Check(lpFixed->mbPositionSet && giNoCutToFlag == -1, "Update: mbPositionSet latched; visibility not interrupted");

    // A second frame holds the shot: no new look-at, no new draw.
    lrPlayer.mLinearVelocity = Vector3{ 3.0f, 0.0f, 3.0f, 0.0f };
    lpBehaviour->Update(lCamera, lInfo);
    Check(giLookAts == 1 && giRandomDraws == 1 && NearM(lCamera.mTransform, lPlanted),
          "Update: once planted, the shot holds (no new look-at or dutch draw)");

    // ---- 3. the three offset arms -----------------------------------------------------------------
    struct Arm { Vector3 mVelocity; Vector3 mEye; bool mbFail; const char* mpcName; };
    const Arm laArms[] = {
        { Vector3{ 30.0f, 0.0f, 0.0f, 0.0f }, Vector3{ 130.0f, 5.0f, 200.0f, 0.0f }, true,
          "Update: |offset|^2 900 > 400 -> Fail(7) 'Position too far from subject'; the shot is still planted" },
        { Vector3{ 1.0f, 0.0f, 0.0f, 0.0f }, Vector3{ 105.0f, 5.0f, 200.0f, 0.0f }, false,
          "Update: |offset|^2 1 < 25 -> NormalizeFast(offset) * 5" },
        { Vector3{ 0.0f, 7.0f, 0.0f, 0.0f }, Vector3{ 100.0f, 5.0f, 205.0f, 0.0f }, false,
          "Update: a purely vertical velocity zeroes -> the car's zAxis * 5" },
    };
    for (const Arm& lrArm : laArms)
    {
        giFailFlag = -1;
        giLookAts = 0;
        lpFixed = Pool();
        lpBehaviour = lpFixed;
        lpBehaviour->Construct();
        lpFixed->SetParameters(&lParams);
        lpBehaviour->Prepare(lPrepareInfo);
        lrPlayer.mLinearVelocity = lrArm.mVelocity;
        lpBehaviour->Update(lCamera, lInfo);
        Check(giLookAts == 1 && NearV(gLastEye, lrArm.mEye) && (giFailFlag == 7) == lrArm.mbFail &&
              lpFixed->mbHasFailed == lrArm.mbFail, lrArm.mpcName);
    }
    Check((lCamera.mState_uFlags & 2) != 0, "Update: camera flag bit 1 raised by the healthy shots");

    // ---- 4. visibility interrupted -> NoCutTo 16, cannot switch to -------------------------------
    lpFixed = Pool();
    lpBehaviour = lpFixed;
    lpBehaviour->Construct();
    lpFixed->SetParameters(&lParams);
    lpBehaviour->Prepare(lPrepareInfo);
    lpFixed->mbCanSwitchToMeNow = true;
    lpFixed->mCollisionPolicy.mVisibilityTest.mbOccluded = true;
    giNoCutToFlag = -1;
    lrPlayer.mLinearVelocity = Vector3{ 10.0f, 0.0f, 0.0f, 0.0f };
    lpBehaviour->Update(lCamera, lInfo);
    Check(giNoCutToFlag == 16 && !lpFixed->mbCanSwitchToMeNow,
          "Update: visibility interrupted (policy +0x1A1) -> NoCutTo 16 'Subject left frame', mbCanSwitchToMeNow = 0");
    lpFixed->mCollisionPolicy.mVisibilityTest.mbOccluded = false;
    lpFixed->mCollisionPolicy.mVisibilityTest.mbTestLookingAt = true;
    lpFixed->mCollisionPolicy.mVisibilityTest.mbIsOnScreen = false;
    lpFixed->mbCanSwitchToMeNow = true;
    giNoCutToFlag = -1;
    lpBehaviour->Update(lCamera, lInfo);
    Check(giNoCutToFlag == 16 && !lpFixed->mbCanSwitchToMeNow,
          "Update: visibility interrupted (+0x1A0 && !+0x1A2) -> NoCutTo 16");
    lpFixed->mCollisionPolicy.mVisibilityTest.mbIsOnScreen = true;
    lpFixed->mbCanSwitchToMeNow = true;
    giNoCutToFlag = -1;
    lpBehaviour->Update(lCamera, lInfo);
    Check(giNoCutToFlag == -1 && lpFixed->mbCanSwitchToMeNow, "Update: +0x1A0 with +0x1A2 set is NOT interrupted");

    // ---- 5. tweaker attached: the dutch is not baked in; the camera shows the MAX dutch ------------
    lpFixed = Pool();
    lpBehaviour = lpFixed;
    lpBehaviour->Construct();
    lpFixed->SetParameters(&lParams);
    lpBehaviour->Prepare(lPrepareInfo);
    lpFixed->SetTweakerAttached(true);
    lrPlayer.mLinearVelocity = Vector3{ 10.0f, 0.0f, 0.0f, 0.0f };
    lpBehaviour->Update(lCamera, lInfo);
    const Matrix44Affine lUnrolled = Utils::CreateLookAt(Vector3{ 110.0f, 5.0f, 200.0f, 0.0f }, lrPlayer.mTransform.wAxis);
    Check(NearM(lpFixed->mBaseTranform, lUnrolled), "Update: tweaker attached (+0x0A) -> the planted shot is not rolled");
    Check(NearM(lCamera.mTransform, rw::math::vpu::Mult(rw::math::vpu::MakeRotationZ(10.0f * 0.017453292f), lUnrolled)),
          "Update: tweaker attached -> the camera shows the shot rolled by mfMaxDutch");

    // ---- 6. SetupTweaker (slot 6) --------------------------------------------------------------
    gTweaks.clear();
    lpBehaviour->SetupTweaker(*reinterpret_cast<Utils::Tweaker*>(gaRandom));
    Check(giTweakerConstructs == 1 && gTweaks.size() == 2, "SetupTweaker: Tweaker::Construct, then two mappings");
    Check(gTweaks.size() == 2 && gTweaks[0].mName == "Roll" && gTweaks[0].mpfVariable == &lParams.mfMaxDutch &&
          gTweaks[0].mfScale == 0.5f && gTweaks[0].miAxis == Utils::Tweaker::E_AXIS_UPPER_TRIGGERS &&
          gTweaks[0].miMap == Utils::Tweaker::E_MAP_NORMAL,
          "SetupTweaker: \"Roll\" -> mfMaxDutch, 0.5, [NORMAL][UPPER_TRIGGERS] (+0x8C)");
    Check(gTweaks.size() == 2 && gTweaks[1].mName == "FOV" && gTweaks[1].mpfVariable == &lParams.mfFOV &&
          gTweaks[1].mfScale == 0.5f && gTweaks[1].miAxis == Utils::Tweaker::E_AXIS_LOWER_TRIGGERS &&
          gTweaks[1].miMap == Utils::Tweaker::E_MAP_NORMAL,
          "SetupTweaker: \"FOV\" (rodata 0x820051C0) -> mfFOV, 0.5, [NORMAL][LOWER_TRIGGERS] (+0x78)");

    Check(gAsserts == 0, "no assert fired");

    std::printf("FxDirectorFixedCam: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
