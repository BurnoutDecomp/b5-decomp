// L1 (owner's list 2026-09-28, item 2 "the camera is behind walls / below the map", piece 7): the car-attached camera
// collision policy BrnDirector::Camera::CollisionPolicyAttachedToVehicle against the console's own answers
// (tests/L1AttachedPolicyData.h, the ARTIST words run on emu64 by scratch/OWNERLIST_0927/L1/emu/
// gen_attached_policy.py). run_l1_attached_policy.py compiles this against the revision's BrnCollisionPolicy.h and
// includes the revision's BrnCollisionPolicyAttachedToVehicle.cpp as l1_attached_policy.inc.
//
// GROUP C -- Construct @0x82224890, the store set. The console's Construct is run from a policy filled with
//   KU_L1_PATTERN; the data lists every field it stores and the FINAL bytes it left (big-endian). Here the policy is
//   placement-constructed, re-filled with the pattern past the host vptr, and Constructed; then
//     C1/C4  the base's failure latch is clear (the console's `stb 0, 4`; on the host the latch sits after the
//            8-byte vptr, so it is checked by name),
//     C2/C5  every field the console stores holds the console's final value (a 4-byte field compared as a
//            big-endian word on the console and a host word here; a byte as a byte),
//     C3/C6  every other byte of +0x10..+0x24F still holds the pattern (Construct writes nothing the console
//            does not).
//   for Construct(false) and Construct(true). The layout is the console's from +0x10 on (the class's own
//   _AssertLayout pins it), which is what lets the bytes be compared at the console offsets.
//
// GROUP P -- the scene-query pair, the plain arm (mbUseFrustrumResolver 0): per case the policy is Constructed from
//   the same pattern, tuned (flags, pitch mover, radius, heights), attached to one car, and run for three frames of
//     GenerateSceneQueries @0x82252690 -> the car-to-camera box answered (or left waiting) -> ProcessSceneQueryResults
//     @0x82252888 (ResolveCollisions @0x82224948, UpdateMinElevation @0x822405B8, UpdateRadius @0x8220E4D0)
//   through the CollisionPolicy base pointer, as the director's scene-query pass calls it. Every other function the
//   policy calls is a fixture below that records its arguments and applies the SAME small deterministic effect as the
//   generator's hook (so the frames test the policy's own branches, NaN polarities, roundings, argument order and
//   state, not its callees -- GetPitchAboutPointRads is the flagged de-optimised form, and the others have their own
//   tests). Checks, over all frames:
//     P1  every call, in order, with every argument (bit for bit; a NaN lane by class)
//     P2  the camera after GenerateSceneQueries          P3  the camera after ProcessSceneQueryResults
//     P4  the policy after the frame: mPitchMover, mfMaxRadius, mfTrafficCollisionResolution,
//         mbResetVehicleCollision, the car-to-camera box's state
//     P5  the asserts fired in the frame (the console's inlined GetPackage tripwire on an unanswered box)
//   A revision without the DWARF layout cannot build group P (L1_NO_LAYOUT): its five checks are counted failed.
//
// GROUP F (piece 6a) -- the scene-query pair, the FRUSTUM arm (mbUseFrustrumResolver 1): the same three-frame run with
//   the policy's FrustrumCollisionResolver in the loop -- GenerateSceneQueries @0x82252540, CalculateFrustumLineTests
//   @0x8220DDE8 (XMVectorTan @0x821F0788), RequestFrustumLineTests @0x8223FD70, ProcessSceneQueryResults @0x822242F8
//   -- with the camera's field of view, aspect ratio and near-clip state set per frame, each frustum box answered or
//   left waiting between the halves, and sometimes the camera's custom near clip cleared between them. The corner
//   resolve Utils::ResolveLineTestNearestUsingDisplacementAndVector @0x8220CEB0 is a recorder (id 11, its body has its
//   own test: run_l1_camera_utils.py), and Camera::GetNearClipDistance is the console's inline (the test does not link
//   Camera.cpp). A third of the cases construct with mbDoVehicleCollision, with the traffic resolution kept at or
//   under 0.01 (ResolveVehicleCollisions @0x82223890 is piece 6b and not reached).
//     F1  every call, in order, with every argument   F2  the camera after GenerateSceneQueries, with its near clip
//     F3  the camera after ProcessSceneQueryResults
//     F4  the policy after the frame: as P4, plus the four frustum boxes' states and the resolver's
//         mVehicleResolveVector
//     F5  the asserts fired in the frame (CalculateFrustumLineTests' IsValid assert on a NaN camera row)
//
// GROUP V (piece 6b) -- the chase cam's traffic push-out: group F's run with the policy constructed with
//   mbDoVehicleCollision and the traffic resolution above 0.01, so FrustrumCollisionResolver::ResolveVehicleCollisions
//   @0x82223890 runs (ResolveVehicleCollision @0x8220DBC8, GetHeightAboveTraffic @0x821F9098, XMVectorCos @0x821F06B0,
//   XMVectorATan @0x821F0A70, the inlined XMVectorSinCos -- all interpreted on emu64) over each case's world: the used
//   race cars (0 is the attached car) and up to 12 traffic vehicles. V1..V5 check what F1..F5 check; the eye's lift,
//   its pitch about its own x axis and mVehicleResolveVector are in V2 / V4, the angle's three NaN tripwires in V5.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <new>
#include <vector>
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"
#include "GameSource/Director/Camera/Utils/BrnCameraSmoothMover.h"
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"
#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficDirectorInterfaces.h"

#include "L1AttachedPolicyData.h"

static int giAsserts = 0;
namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++giAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

// The revision's BrnCollisionPolicyAttachedToVehicle.cpp.
#include "l1_attached_policy.inc"

using BrnDirector::Camera::CollisionPolicyAttachedToVehicle;

static int giChecks = 0;
static int giFailures = 0;

static void Check(bool lbOk, const char* lpcName)
{
    ++giChecks;
    if (!lbOk)
        ++giFailures;
    std::printf("%s  %s\n", lbOk ? "PASS" : "FAIL", lpcName);
}

static u32 BigEndianWord(const u8* lpu)
{
    return (static_cast<u32>(lpu[0]) << 24) | (static_cast<u32>(lpu[1]) << 16) | (static_cast<u32>(lpu[2]) << 8)
         | static_cast<u32>(lpu[3]);
}

static u32 HostWord(const void* lpv)
{
    u32 lu;
    std::memcpy(&lu, lpv, 4);
    return lu;
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 FromBits(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static bool IsNanBits(u32 lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }
static bool WordMatches(u32 luConsole, u32 luPc) { return IsNanBits(luConsole) ? IsNanBits(luPc) : luConsole == luPc; }

// ============================================================================
// GROUP C
// ============================================================================
alignas(16) static u8 gaPolicyBuffer[sizeof(CollisionPolicyAttachedToVehicle) + 16];

static CollisionPolicyAttachedToVehicle* ConstructFromPattern(bool lbArgument)
{
    std::memset(gaPolicyBuffer, KU_L1_PATTERN, sizeof(gaPolicyBuffer));
    CollisionPolicyAttachedToVehicle* lpPolicy = new (gaPolicyBuffer) CollisionPolicyAttachedToVehicle;
    // The host vptr fills +0x00..+0x07; everything after it goes back to the pattern (a member's own default
    // initialisation, if any, must not count as Construct's).
    std::memset(gaPolicyBuffer + sizeof(void*), KU_L1_PATTERN, sizeof(CollisionPolicyAttachedToVehicle) - sizeof(void*));
    lpPolicy->Construct(lbArgument);
    return lpPolicy;
}

static void CheckConstruct(bool lbArgument, const L1ConstructStore* lpStores, u32 luCount, const char* lpcTag)
{
    CollisionPolicyAttachedToVehicle* lpPolicy = ConstructFromPattern(lbArgument);
    const u8* lpBytes = gaPolicyBuffer;

    char lacName[160];
    std::snprintf(lacName, sizeof(lacName), "%s: the base's failure latch is clear (console `stb 0, 4`)", lpcTag);
    Check(!lpPolicy->HasFailed(), lacName);

    bool labStored[0x250] = {};
    int  liBad = 0;
    for (u32 i = 0; i < luCount; ++i)
    {
        const L1ConstructStore& lrStore = lpStores[i];
        if (lrStore.muOffset < 0x10u)
            continue;   // the base sub-object (the console's vptr + latch): checked by name above
        for (u32 b = 0; b < lrStore.muWidth; ++b)
            labStored[lrStore.muOffset + b] = true;
        bool lbOk = true;
        if (lrStore.muWidth == 1u)
        {
            lbOk = lpBytes[lrStore.muOffset] == lrStore.mauBytes[0];
        }
        else
        {
            for (u32 w = 0; w < lrStore.muWidth; w += 4u)
                lbOk = lbOk && HostWord(lpBytes + lrStore.muOffset + w) == BigEndianWord(lrStore.mauBytes + w);
        }
        if (!lbOk)
        {
            ++liBad;
            std::printf("  %s +0x%03X/%u: console", lpcTag, lrStore.muOffset, lrStore.muWidth);
            for (u32 b = 0; b < lrStore.muWidth; ++b)
                std::printf(" %02X", lrStore.mauBytes[b]);
            std::printf("  host(LE)");
            for (u32 b = 0; b < lrStore.muWidth; ++b)
                std::printf(" %02X", lpBytes[lrStore.muOffset + b]);
            std::printf("\n");
        }
    }
    std::snprintf(lacName, sizeof(lacName), "%s: every field the console stores holds the console's value (%d wrong)",
                  lpcTag, liBad);
    Check(liBad == 0, lacName);

    int liTouched = 0;
    for (u32 o = 0x10u; o < 0x250u; ++o)
    {
        if (!labStored[o] && lpBytes[o] != KU_L1_PATTERN)
        {
            if (liTouched < 8)
                std::printf("  %s +0x%03X written (%02X) where the console writes nothing\n", lpcTag, o, lpBytes[o]);
            ++liTouched;
        }
    }
    std::snprintf(lacName, sizeof(lacName), "%s: no byte the console leaves is written (%d written)", lpcTag,
                  liTouched);
    Check(liTouched == 0, lacName);
}

// ============================================================================
// GROUP P -- the fixtures (the generator's hooks, effect for effect)
// ============================================================================
namespace
{
    std::vector<u32>                        gCalls;
    const u32*                              gpuPitchAnswers = 0;   // the frame's two GetPitchAboutPointRads answers
    int                                     giPitchNext = 0;
    const void*                             gpPolicy = 0;
    const void*                             gpCamera = 0;
    size_t                                  guCameraSize = 0;
    const BrnDirector::SceneQueryInterface* gpInterface = 0;

    u32 Rel(const void* lpv)
    {
        const u8* lp       = static_cast<const u8*>(lpv);
        const u8* lpPolicy = static_cast<const u8*>(gpPolicy);
        const u8* lpCamera = static_cast<const u8*>(gpCamera);
        if (lp >= lpPolicy && lp < lpPolicy + 0x400)
            return static_cast<u32>(lp - lpPolicy);
        if (lp >= lpCamera && lp < lpCamera + guCameraSize)
            return 0x10000u + static_cast<u32>(lp - lpCamera);
        return 0xFFFFFFFFu;
    }

    void Emit(u32 luId, std::initializer_list<u32> lArgs)
    {
        gCalls.push_back(luId);
        gCalls.push_back(static_cast<u32>(lArgs.size()));
        for (u32 lu : lArgs)
            gCalls.push_back(lu);
    }

    void EmitVector(std::vector<u32>& lrOut, const Vector3& lrV)
    {
        lrOut.push_back(Bits(lrV.x));
        lrOut.push_back(Bits(lrV.y));
        lrOut.push_back(Bits(lrV.z));
        lrOut.push_back(Bits(lrV.w));
    }

    void EmitList(u32 luId, const std::vector<u32>& lrArgs)
    {
        gCalls.push_back(luId);
        gCalls.push_back(static_cast<u32>(lrArgs.size()));
        gCalls.insert(gCalls.end(), lrArgs.begin(), lrArgs.end());
    }
}

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    // 0 -- the row's answer for this call.
    f32 GetPitchAboutPointRads(Vector3 lCentre, Vector3 lPoint)
    {
        const u32 luAnswer = (gpuPitchAnswers != 0 && giPitchNext < 2) ? gpuPitchAnswers[giPitchNext] : Bits(0.0f);
        ++giPitchNext;
        std::vector<u32> lArgs;
        EmitVector(lArgs, lCentre);
        EmitVector(lArgs, lPoint);
        lArgs.push_back(luAnswer);
        EmitList(0u, lArgs);
        return FromBits(luAnswer);
    }

    // 1 -- M.w.y += the elevation's lane 0 (one f32 add).
    void ApplyPitchAboutPointRads(Matrix44Affine& lTransformInOut, Vector3 lCentreOfRotation,
                                  VecFloat lElevationRads)
    {
        std::vector<u32> lArgs;
        lArgs.push_back(Rel(&lTransformInOut));
        EmitVector(lArgs, lCentreOfRotation);
        for (int i = 0; i < 4; ++i)
            lArgs.push_back(Bits(lElevationRads.maLanes[i]));
        EmitList(1u, lArgs);
        lTransformInOut.wAxis.y = lTransformInOut.wAxis.y + lElevationRads.maLanes[0];
    }

    // 5 -- an intersection moves the position onto the hit (x, y, z).
    bool ResolveLineTestNearestUsingNormalStrict(
        PostBox<CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult>& lPostBox, Vector3& lPosition,
        f32 lfMinDistance)
    {
        Emit(5u, { Rel(&lPostBox), Rel(&lPosition), Bits(lfMinDistance) });
        u8 luIntersection = 0;
        std::memcpy(&luIntersection, &lPostBox.mPackage.mbIntersection, 1);
        if (luIntersection == 0)
            return false;
        lPosition.x = lPostBox.mPackage.mPosition.x;
        lPosition.y = lPostBox.mPackage.mPosition.y;
        lPosition.z = lPostBox.mPackage.mPosition.z;
        return true;
    }

    // 11 -- recorded; a box that GOT its package with an intersection moves the position onto the hit (x, y, z) and
    // answers true.
    bool ResolveLineTestNearestUsingDisplacementAndVector(LineTestNearestPostBox& lPostBox, Vector3 lTestPoint,
                                                          Vector3& lPosition, Vector3 lVector, VecFloat lvMinDistance)
    {
        std::vector<u32> lArgs;
        lArgs.push_back(Rel(&lPostBox));
        EmitVector(lArgs, lTestPoint);
        lArgs.push_back(Rel(&lPosition));
        EmitVector(lArgs, lVector);
        for (int i = 0; i < 4; ++i)
            lArgs.push_back(Bits(lvMinDistance.maLanes[i]));
        EmitList(11u, lArgs);
        u8 luIntersection = 0;
        std::memcpy(&luIntersection, &lPostBox.mPackage.mbIntersection, 1);
        if (lPostBox.meState != LineTestNearestPostBox::E_STATE_GOT_PACKAGE || luIntersection == 0)
            return false;
        lPosition.x = lPostBox.mPackage.mPosition.x;
        lPosition.y = lPostBox.mPackage.mPosition.y;
        lPosition.z = lPostBox.mPackage.mPosition.z;
        return true;
    }

    // 7 -- recorded with the mover's speed and value as Update finds them; then speed = the force, value += the
    // timestep (one f32 add).
    void SmoothMover::Update(f32 lfTimestep, f32 lfForceAppliedMinusOneToOne, const Parameters& lParameters)
    {
        u8 luCentering = 0;
        u8 luLimits = 0;
        std::memcpy(&luCentering, &lParameters.mbUseCentering, 1);
        std::memcpy(&luLimits, &lParameters.mbUseLimits, 1);
        Emit(7u, { Rel(this), Bits(lfTimestep), Bits(lfForceAppliedMinusOneToOne), Bits(lParameters.mfMaxValue),
                   Bits(lParameters.mfMinValue), Bits(lParameters.mfDampeningRange), Bits(lParameters.mfMaxSpeed),
                   Bits(lParameters.mfDeadZoneHalfSize), Bits(lParameters.mfBrakingLag), Bits(lParameters.mfNormalLag),
                   Bits(lParameters.mfCenteringRate), Bits(lParameters.mfCenteringRateBlend), luCentering, luLimits,
                   Bits(mfCurrentSpeed), Bits(mfCurrentValue) });
        mfCurrentSpeed = lfForceAppliedMinusOneToOne;
        mfCurrentValue = mfCurrentValue + lfTimestep;
    }
}

    // 2 -- recorded.
    void GroundConstraint::GenerateSceneQueries(const Camera& lrCamera, f32 lfTimestep,
                                                const SceneQueryInterface* lpRequestInterface)
    {
        Emit(2u, { Rel(this), Rel(&lrCamera), Bits(lfTimestep), lpRequestInterface == gpInterface ? 1u : 0u });
    }

    // 3 -- the camera's height += 0.25 (one f32 add); found.
    bool GroundConstraint::ProcessSceneQueryResults(f32 lfTimestep, Camera& lrCamera)
    {
        Emit(3u, { Rel(this), Bits(lfTimestep), Rel(&lrCamera) });
        lrCamera.mTransform.wAxis.y = lrCamera.mTransform.wAxis.y + 0.25f;
        return true;
    }

    // 6 -- recorded.
    void CollisionPolicy::Fail(Camera& lrCamera, s32 leFailedFlag)
    {
        Emit(6u, { Rel(this), Rel(&lrCamera), static_cast<u32>(leFailedFlag) });
    }

    // Not a recorder: the console INLINES Camera::GetNearClipDistance @0x82205B68 into the resolver's
    // ProcessSceneQueryResults (0x82224324..0x8222436C) and this test does not link Camera.cpp, so this is that inline,
    // flag for flag -- the custom near clip when set, else the camera state's E_FLAG_SMALL_NEAR_CLIP (bit 16 of the
    // +0x140 flags) picks flt_82CDA55C == 0x3DCCCCCD over flt_82CDA560 == 0x3E19999A (both read from the image).
    f32 Camera::GetNearClipDistance() const
    {
        if (mbHasCustomNearClipDistance)
            return mfCustomNearClipDistance;
        return ((mState_uFlags & 0x10000) != 0) ? 0.1f : 0.15f;
    }
}

    // 4 -- recorded; the box starts waiting (WaitForPackage).
    void SceneQueryInterface::LineTestNearest(LineTestNearestPostBox& lrPostBox, u32 lx32EntityTypeFlags,
                                              u8 lxVolumeTypeFlags, const Vector3& lLineStart,
                                              const Vector3& lLineEnd, CgsSceneManager::EntityId lExcludeEntityId,
                                              CgsSceneManager::SceneManagerIO::EExclusionMode leExclusionMode) const
    {
        std::vector<u32> lArgs;
        lArgs.push_back(this == gpInterface ? 1u : 0u);
        lArgs.push_back(Rel(&lrPostBox));
        lArgs.push_back(lx32EntityTypeFlags);
        lArgs.push_back(lxVolumeTypeFlags);
        EmitVector(lArgs, lLineStart);
        EmitVector(lArgs, lLineEnd);
        lArgs.push_back(static_cast<u32>(lExcludeEntityId));
        lArgs.push_back(static_cast<u32>(leExclusionMode));
        EmitList(4u, lArgs);
        lrPostBox.meState = LineTestNearestPostBox::E_STATE_WAITING_FOR_PACKAGE;
    }
}

// The fixture car's RaceCarState constructor calls Clear (its body lives with the vehicle manager, which this test
// does not link); the car is a zero-initialised static whose fields the cases set, so an empty body is enough.
void BrnPhysics::Vehicle::RaceCarState::Clear() {}

#ifndef L1_NO_LAYOUT
static void RowsFromWords(rw::math::vpu::Matrix44Affine& lrOut, const u32* lpu)
{
    Vector3* lapRows[4] = { &lrOut.xAxis, &lrOut.yAxis, &lrOut.zAxis, &lrOut.wAxis };
    for (int r = 0; r < 4; ++r)
    {
        lapRows[r]->x = FromBits(lpu[4 * r + 0]);
        lapRows[r]->y = FromBits(lpu[4 * r + 1]);
        lapRows[r]->z = FromBits(lpu[4 * r + 2]);
        lapRows[r]->w = FromBits(lpu[4 * r + 3]);
    }
}

static bool RowsMatch(const rw::math::vpu::Matrix44Affine& lrPc, const u32* lpuConsole)
{
    const Vector3* lapRows[4] = { &lrPc.xAxis, &lrPc.yAxis, &lrPc.zAxis, &lrPc.wAxis };
    bool lbOk = true;
    for (int r = 0; r < 4; ++r)
    {
        lbOk = lbOk && WordMatches(lpuConsole[4 * r + 0], Bits(lapRows[r]->x))
                    && WordMatches(lpuConsole[4 * r + 1], Bits(lapRows[r]->y))
                    && WordMatches(lpuConsole[4 * r + 2], Bits(lapRows[r]->z))
                    && WordMatches(lpuConsole[4 * r + 3], Bits(lapRows[r]->w));
    }
    return lbOk;
}

static BrnDirector::Camera::VehicleInfo gVehicle;
static BrnDirector::AllVehicleData      gWorld;
static BrnDirector::SceneQueryInterface gInterface;
static BrnDirector::Camera::Camera      gCamera;

static void RunFrames()
{
    int liBadCalls = 0, liBadGenerate = 0, liBadProcess = 0, liBadState = 0, liBadAsserts = 0, liFrames = 0;
    int liShown = 0;

    gWorld.Construct();
    gWorld.mpRaceCars = &gVehicle;
    gWorld.mePlayerRaceCarIndex = static_cast<EActiveRaceCarIndex>(0);
    gWorld.mUsedRaceCars.SetBit(0u);
    gpInterface = &gInterface;
    gpCamera = &gCamera;
    guCameraSize = sizeof(gCamera);

    const u32 luCases = sizeof(kaL1Cases) / sizeof(kaL1Cases[0]);
    for (u32 c = 0; c < luCases; ++c)
    {
        const L1Case& lrCase = kaL1Cases[c];
        CollisionPolicyAttachedToVehicle* lpPolicy = ConstructFromPattern(lrCase.muConstructArgument != 0u);
        gpPolicy = lpPolicy;
        lpPolicy->mbAutoElevate           = static_cast<u8>(lrCase.mauFlags[0]);
        lpPolicy->mbSmoothRadiusChanges   = static_cast<u8>(lrCase.mauFlags[1]);
        lpPolicy->mbFailOnContact         = static_cast<u8>(lrCase.mauFlags[2]);
        lpPolicy->mbUseGroundConstraint   = static_cast<u8>(lrCase.mauFlags[3]);
        lpPolicy->mbTestAgainstWorldOnly  = static_cast<u8>(lrCase.mauFlags[4]);
        lpPolicy->mbUseFrustrumResolver   = 0u;
        lpPolicy->mbResetVehicleCollision = static_cast<u8>(lrCase.mauFlags[5]);
        lpPolicy->mPitchMover.mfCenteringRate = FromBits(lrCase.mauPitchMover[0]);
        lpPolicy->mPitchMover.mfCurrentSpeed  = FromBits(lrCase.mauPitchMover[1]);
        lpPolicy->mPitchMover.mfCurrentValue  = FromBits(lrCase.mauPitchMover[2]);
        lpPolicy->mfDesiredNearClip            = FromBits(lrCase.muNearClip);
        lpPolicy->mfMaxRadius                  = FromBits(lrCase.muMaxRadius);
        lpPolicy->mfTrafficCollisionResolution = FromBits(lrCase.muTrafficResolution);
        lpPolicy->mGroundConstraint.SetDesiredHeight(FromBits(lrCase.muDesiredHeight));

        RowsFromWords(gVehicle.mRaceCarState.mTransform, lrCase.mauVehicle);
        gVehicle.mRaceCarState.mEntityId.muValue = lrCase.muEntity;
        gVehicle.mRaceCarState.mfSpeedMPH = FromBits(lrCase.muSpeed);

        BrnDirector::Camera::CollisionPolicySharedInfo lShared{};
        lShared.mpRequestInterface = &gInterface;
        lShared.mpAllVehicleData = &gWorld;
        const f32 lfWorld = FromBits(lrCase.mauTimestep[0]);
        const f32 lfNoSlomo = FromBits(lrCase.mauTimestep[1]);
        const f32 lfGame = FromBits(lrCase.mauTimestep[2]);
        lShared.mTimestep.Set(BrnDirector::VecFloat(lfGame), BrnDirector::VecFloat(lfWorld),
                              BrnDirector::VecFloat(lfNoSlomo), lfGame, lfWorld, lfNoSlomo);

        BrnDirector::Camera::CollisionPolicy* lpBase = lpPolicy;
        for (u32 f = 0; f < lrCase.muFrameCount; ++f)
        {
            const L1Frame& lrFrame = kaL1Frames[lrCase.muFirstFrame + f];
            ++liFrames;
            RowsFromWords(gCamera.mTransform, lrFrame.mauCamera);
            gCalls.clear();
            gpuPitchAnswers = lrFrame.mauPitch;
            giPitchNext = 0;
            const int liAsserts0 = giAsserts;

            lpBase->GenerateSceneQueries(lShared, gCamera);
            const bool lbGenerateOk = RowsMatch(gCamera.mTransform, lrFrame.mauAfterGenerate);

            if (lrFrame.muDeliver)
            {
                lpPolicy->mCarToCamera.meState = BrnDirector::LineTestNearestPostBox::E_STATE_GOT_PACKAGE;
                lpPolicy->mCarToCamera.mPackage.mPosition.x = FromBits(lrFrame.mauHit[0]);
                lpPolicy->mCarToCamera.mPackage.mPosition.y = FromBits(lrFrame.mauHit[1]);
                lpPolicy->mCarToCamera.mPackage.mPosition.z = FromBits(lrFrame.mauHit[2]);
                lpPolicy->mCarToCamera.mPackage.mPosition.w = FromBits(lrFrame.mauHit[3]);
                lpPolicy->mCarToCamera.mPackage.mNormal.x = FromBits(lrFrame.mauNormal[0]);
                lpPolicy->mCarToCamera.mPackage.mNormal.y = FromBits(lrFrame.mauNormal[1]);
                lpPolicy->mCarToCamera.mPackage.mNormal.z = FromBits(lrFrame.mauNormal[2]);
                lpPolicy->mCarToCamera.mPackage.mNormal.w = FromBits(lrFrame.mauNormal[3]);
                const u8 luIntersection = static_cast<u8>(lrFrame.muIntersection);
                std::memcpy(&lpPolicy->mCarToCamera.mPackage.mbIntersection, &luIntersection, 1);
            }

            lpBase->ProcessSceneQueryResults(lShared, gCamera);

            // P1 the calls
            bool lbCallsOk = gCalls.size() == lrFrame.muCallWords;
            for (u32 w = 0; lbCallsOk && w < lrFrame.muCallWords; ++w)
                lbCallsOk = WordMatches(kau32L1CallStream[lrFrame.muFirstCallWord + w], gCalls[w]);
            // P3 the camera after
            const bool lbProcessOk = RowsMatch(gCamera.mTransform, lrFrame.mauAfterProcess);
            // P4 the state after
            u8 luReset = 0;
            std::memcpy(&luReset, &lpPolicy->mbResetVehicleCollision, 1);
            const bool lbStateOk =
                WordMatches(lrFrame.mauPitchMover[0], Bits(lpPolicy->mPitchMover.mfCenteringRate))
                && WordMatches(lrFrame.mauPitchMover[1], Bits(lpPolicy->mPitchMover.mfCurrentSpeed))
                && WordMatches(lrFrame.mauPitchMover[2], Bits(lpPolicy->mPitchMover.mfCurrentValue))
                && WordMatches(lrFrame.muMaxRadius, Bits(lpPolicy->mfMaxRadius))
                && WordMatches(lrFrame.muTrafficResolution, Bits(lpPolicy->mfTrafficCollisionResolution))
                && lrFrame.muResetVehicleCollision == luReset
                && lrFrame.muBoxState == static_cast<u32>(lpPolicy->mCarToCamera.meState);
            // P5 the asserts
            const bool lbAssertsOk = static_cast<u32>(giAsserts - liAsserts0) == lrFrame.muAsserts;

            liBadCalls += lbCallsOk ? 0 : 1;
            liBadGenerate += lbGenerateOk ? 0 : 1;
            liBadProcess += lbProcessOk ? 0 : 1;
            liBadState += lbStateOk ? 0 : 1;
            liBadAsserts += lbAssertsOk ? 0 : 1;
            if (!(lbCallsOk && lbGenerateOk && lbProcessOk && lbStateOk && lbAssertsOk) && liShown < 6)
            {
                ++liShown;
                std::printf("  case %u frame %u: calls %s (console %u words, pc %u) generate %s process %s state %s "
                            "asserts %s (console %u, pc %d)\n", c, f, lbCallsOk ? "ok" : "BAD", lrFrame.muCallWords,
                            static_cast<u32>(gCalls.size()), lbGenerateOk ? "ok" : "BAD", lbProcessOk ? "ok" : "BAD",
                            lbStateOk ? "ok" : "BAD", lbAssertsOk ? "ok" : "BAD", lrFrame.muAsserts,
                            giAsserts - liAsserts0);
                if (!lbCallsOk)
                {
                    std::printf("    console:");
                    for (u32 w = 0; w < lrFrame.muCallWords && w < 40u; ++w)
                        std::printf(" %08X", kau32L1CallStream[lrFrame.muFirstCallWord + w]);
                    std::printf("\n    pc     :");
                    for (size_t w = 0; w < gCalls.size() && w < 40u; ++w)
                        std::printf(" %08X", gCalls[w]);
                    std::printf("\n");
                }
                if (!lbStateOk)
                    std::printf("    state console mover %08X %08X %08X max %08X traffic %08X reset %u box %u | pc "
                                "%08X %08X %08X max %08X traffic %08X reset %u box %u\n", lrFrame.mauPitchMover[0],
                                lrFrame.mauPitchMover[1], lrFrame.mauPitchMover[2], lrFrame.muMaxRadius,
                                lrFrame.muTrafficResolution, lrFrame.muResetVehicleCollision, lrFrame.muBoxState,
                                Bits(lpPolicy->mPitchMover.mfCenteringRate), Bits(lpPolicy->mPitchMover.mfCurrentSpeed),
                                Bits(lpPolicy->mPitchMover.mfCurrentValue), Bits(lpPolicy->mfMaxRadius),
                                Bits(lpPolicy->mfTrafficCollisionResolution), luReset,
                                static_cast<u32>(lpPolicy->mCarToCamera.meState));
                if (!lbProcessOk)
                    std::printf("    camera w console %08X %08X %08X %08X | pc %08X %08X %08X %08X\n",
                                lrFrame.mauAfterProcess[12], lrFrame.mauAfterProcess[13], lrFrame.mauAfterProcess[14],
                                lrFrame.mauAfterProcess[15], Bits(gCamera.mTransform.wAxis.x),
                                Bits(gCamera.mTransform.wAxis.y), Bits(gCamera.mTransform.wAxis.z),
                                Bits(gCamera.mTransform.wAxis.w));
            }
        }
    }

    char lacName[160];
    std::snprintf(lacName, sizeof(lacName), "P1 every call and argument, in order (%d of %d frames wrong)", liBadCalls,
                  liFrames);
    Check(liBadCalls == 0, lacName);
    std::snprintf(lacName, sizeof(lacName), "P2 the camera after GenerateSceneQueries (%d of %d frames wrong)",
                  liBadGenerate, liFrames);
    Check(liBadGenerate == 0, lacName);
    std::snprintf(lacName, sizeof(lacName), "P3 the camera after ProcessSceneQueryResults (%d of %d frames wrong)",
                  liBadProcess, liFrames);
    Check(liBadProcess == 0, lacName);
    std::snprintf(lacName, sizeof(lacName), "P4 the policy after the frame (%d of %d frames wrong)", liBadState,
                  liFrames);
    Check(liBadState == 0, lacName);
    std::snprintf(lacName, sizeof(lacName), "P5 the asserts of the frame (%d of %d frames wrong)", liBadAsserts,
                  liFrames);
    Check(liBadAsserts == 0, lacName);
}

// ============================================================================
// GROUP F -- the frustum arm (piece 6a)
// ============================================================================
static Vector3 VecFromWords(const u32* lpu)
{
    Vector3 l;
    l.x = FromBits(lpu[0]);
    l.y = FromBits(lpu[1]);
    l.z = FromBits(lpu[2]);
    l.w = FromBits(lpu[3]);
    return l;
}

static bool VecMatches(const u32* lpuConsole, const Vector3& lrPc)
{
    return WordMatches(lpuConsole[0], Bits(lrPc.x)) && WordMatches(lpuConsole[1], Bits(lrPc.y))
        && WordMatches(lpuConsole[2], Bits(lrPc.z)) && WordMatches(lpuConsole[3], Bits(lrPc.w));
}

static BrnDirector::Camera::VehicleInfo                            gaRaceCars[8];
static Array<BrnTraffic::BrnTrafficIO::TrafficDirectorEntity, 32u> gTraffic;

// The world a case runs against: group F's single attached car (group P's), or a group-V case's race cars (0 is the
// attached car) and traffic. Returns the attached car (what VehicleRef::Get resolves to).
static BrnDirector::Camera::VehicleInfo* SetupWorld(const L1VehicleCase* lpWorld)
{
    gWorld.Construct();
    gWorld.mePlayerRaceCarIndex = static_cast<EActiveRaceCarIndex>(0);
    if (lpWorld == 0)
    {
        gWorld.mpRaceCars = &gVehicle;
        gWorld.mUsedRaceCars.SetBit(0u);
        return &gVehicle;
    }
    gWorld.mpRaceCars = gaRaceCars;
    for (u32 k = 0; k < 8u; ++k)
    {
        if (lpWorld->muUsedRaceCars & (1u << k))
            gWorld.mUsedRaceCars.SetBit(k);
        RowsFromWords(gaRaceCars[k].mRaceCarState.mTransform, lpWorld->mauRaceCars[k]);
        gaRaceCars[k].mRaceCarState.mHalfExtent = VecFromWords(lpWorld->mauRaceCars[k] + 16);
    }
    gTraffic.Construct();
    for (u32 k = 0; k < lpWorld->muTrafficCount; ++k)
    {
        BrnTraffic::BrnTrafficIO::TrafficDirectorEntity lEntity{};
        RowsFromWords(lEntity.mLocalTransform, lpWorld->mauTraffic[k]);
        lEntity.mHalfExtents = VecFromWords(lpWorld->mauTraffic[k] + 16);
        gTraffic.Append(lEntity);
    }
    gWorld.mpTrafficVehicleArray = &gTraffic;
    return &gaRaceCars[0];
}

// Groups F and V: lpcTag is the check prefix ("F" / "V"), lpcArm its label.
static void RunFrustumGroup(const char* lpcTag, const char* lpcArm, u32 luCases, const L1FrustumCase* const* lapCases,
                            const L1VehicleCase* const* lapWorlds, const L1FrustumFrame* lpFrames,
                            const u32* lpuStream)
{
    int liBadCalls = 0, liBadGenerate = 0, liBadProcess = 0, liBadState = 0, liBadAsserts = 0, liFrames = 0;
    int liShown = 0;

    for (u32 c = 0; c < luCases; ++c)
    {
        const L1Case& lrCase = lapCases[c]->mBase;
        BrnDirector::Camera::VehicleInfo* lpAttached = SetupWorld(lapWorlds != 0 ? lapWorlds[c] : 0);
        CollisionPolicyAttachedToVehicle* lpPolicy = ConstructFromPattern(lrCase.muConstructArgument != 0u);
        gpPolicy = lpPolicy;
        lpPolicy->mbAutoElevate           = static_cast<u8>(lrCase.mauFlags[0]);
        lpPolicy->mbSmoothRadiusChanges   = static_cast<u8>(lrCase.mauFlags[1]);
        lpPolicy->mbFailOnContact         = static_cast<u8>(lrCase.mauFlags[2]);
        lpPolicy->mbUseGroundConstraint   = static_cast<u8>(lrCase.mauFlags[3]);
        lpPolicy->mbTestAgainstWorldOnly  = static_cast<u8>(lrCase.mauFlags[4]);
        lpPolicy->mbUseFrustrumResolver   = 1u;
        lpPolicy->mbResetVehicleCollision = static_cast<u8>(lrCase.mauFlags[5]);
        lpPolicy->mPitchMover.mfCenteringRate = FromBits(lrCase.mauPitchMover[0]);
        lpPolicy->mPitchMover.mfCurrentSpeed  = FromBits(lrCase.mauPitchMover[1]);
        lpPolicy->mPitchMover.mfCurrentValue  = FromBits(lrCase.mauPitchMover[2]);
        lpPolicy->mfDesiredNearClip            = FromBits(lrCase.muNearClip);
        lpPolicy->mfMaxRadius                  = FromBits(lrCase.muMaxRadius);
        lpPolicy->mfTrafficCollisionResolution = FromBits(lrCase.muTrafficResolution);
        lpPolicy->mGroundConstraint.SetDesiredHeight(FromBits(lrCase.muDesiredHeight));
        lpPolicy->mFrustrumCollisionResolver.mVehicleResolveVector =
            VecFromWords(lapCases[c]->mauVehicleResolveVector);

        RowsFromWords(lpAttached->mRaceCarState.mTransform, lrCase.mauVehicle);
        lpAttached->mRaceCarState.mEntityId.muValue = lrCase.muEntity;
        lpAttached->mRaceCarState.mfSpeedMPH = FromBits(lrCase.muSpeed);

        BrnDirector::Camera::CollisionPolicySharedInfo lShared{};
        lShared.mpRequestInterface = &gInterface;
        lShared.mpAllVehicleData = &gWorld;
        const f32 lfWorld = FromBits(lrCase.mauTimestep[0]);
        const f32 lfNoSlomo = FromBits(lrCase.mauTimestep[1]);
        const f32 lfGame = FromBits(lrCase.mauTimestep[2]);
        lShared.mTimestep.Set(BrnDirector::VecFloat(lfGame), BrnDirector::VecFloat(lfWorld),
                              BrnDirector::VecFloat(lfNoSlomo), lfGame, lfWorld, lfNoSlomo);

        BrnDirector::LineTestNearestPostBox* const lapBoxes[4] = {
            &lpPolicy->mFrustrumCollisionResolver.mTopLeft, &lpPolicy->mFrustrumCollisionResolver.mTopRight,
            &lpPolicy->mFrustrumCollisionResolver.mBottomLeft, &lpPolicy->mFrustrumCollisionResolver.mBottomRight };

        BrnDirector::Camera::CollisionPolicy* lpBase = lpPolicy;
        for (u32 f = 0; f < lrCase.muFrameCount; ++f)
        {
            const L1FrustumFrame& lrFrame = lpFrames[lrCase.muFirstFrame + f];
            ++liFrames;
            RowsFromWords(gCamera.mTransform, lrFrame.mauCamera);
            gCamera.mfFOV                       = FromBits(lrFrame.muFieldOfView);
            gCamera.mfAspectRatio               = FromBits(lrFrame.muAspectRatio);
            gCamera.mState_uFlags               = lrFrame.muSmallNearClip ? 0x10000 : 0;
            gCamera.mfCustomNearClipDistance    = FromBits(lrFrame.muCustomNearClip);
            gCamera.mbHasCustomNearClipDistance = lrFrame.muHasCustomNearClip != 0u;
            gCalls.clear();
            gpuPitchAnswers = lrFrame.mauPitch;
            giPitchNext = 0;
            const int liAsserts0 = giAsserts;

            lpBase->GenerateSceneQueries(lShared, gCamera);
            const rw::math::vpu::Matrix44Affine lSnapshot = gCamera.mTransform;
            const bool lbGenerateOk = RowsMatch(gCamera.mTransform, lrFrame.mauAfterGenerate)
                && (gCamera.mbHasCustomNearClipDistance ? 1u : 0u) == lrFrame.muAfterGenerateHasCustom
                && WordMatches(lrFrame.muAfterGenerateCustom, Bits(gCamera.mfCustomNearClipDistance));

            if (lrFrame.muClearCustomNearClip)
                gCamera.mbHasCustomNearClipDistance = false;
            for (int k = 0; k < 4; ++k)
            {
                if (!lrFrame.mauDeliver[k])
                    continue;
                lapBoxes[k]->meState = BrnDirector::LineTestNearestPostBox::E_STATE_GOT_PACKAGE;
                lapBoxes[k]->mPackage.mPosition = VecFromWords(lrFrame.mauHit + 4 * k);
                lapBoxes[k]->mPackage.mNormal = VecFromWords(lrFrame.mauNormal + 4 * k);
                const u8 luIntersection = static_cast<u8>(lrFrame.mauIntersection[k]);
                std::memcpy(&lapBoxes[k]->mPackage.mbIntersection, &luIntersection, 1);
            }

            lpBase->ProcessSceneQueryResults(lShared, gCamera);

            // F1 the calls
            bool lbCallsOk = gCalls.size() == lrFrame.muCallWords;
            for (u32 w = 0; lbCallsOk && w < lrFrame.muCallWords; ++w)
                lbCallsOk = WordMatches(lpuStream[lrFrame.muFirstCallWord + w], gCalls[w]);
            // F3 the camera after
            const bool lbProcessOk = RowsMatch(gCamera.mTransform, lrFrame.mauAfterProcess);
            // F4 the state after
            u8 luReset = 0;
            std::memcpy(&luReset, &lpPolicy->mbResetVehicleCollision, 1);
            bool lbStateOk =
                WordMatches(lrFrame.mauPitchMover[0], Bits(lpPolicy->mPitchMover.mfCenteringRate))
                && WordMatches(lrFrame.mauPitchMover[1], Bits(lpPolicy->mPitchMover.mfCurrentSpeed))
                && WordMatches(lrFrame.mauPitchMover[2], Bits(lpPolicy->mPitchMover.mfCurrentValue))
                && WordMatches(lrFrame.muMaxRadius, Bits(lpPolicy->mfMaxRadius))
                && WordMatches(lrFrame.muTrafficResolution, Bits(lpPolicy->mfTrafficCollisionResolution))
                && lrFrame.muResetVehicleCollision == luReset
                && lrFrame.muBoxState == static_cast<u32>(lpPolicy->mCarToCamera.meState)
                && VecMatches(lrFrame.mauVehicleResolveVector,
                              lpPolicy->mFrustrumCollisionResolver.mVehicleResolveVector);
            for (int k = 0; k < 4; ++k)
                lbStateOk = lbStateOk && lrFrame.mauFrustumBoxState[k] == static_cast<u32>(lapBoxes[k]->meState);
            // F5 the asserts
            const bool lbAssertsOk = static_cast<u32>(giAsserts - liAsserts0) == lrFrame.muAsserts;

            liBadCalls += lbCallsOk ? 0 : 1;
            liBadGenerate += lbGenerateOk ? 0 : 1;
            liBadProcess += lbProcessOk ? 0 : 1;
            liBadState += lbStateOk ? 0 : 1;
            liBadAsserts += lbAssertsOk ? 0 : 1;
            if (!(lbCallsOk && lbGenerateOk && lbProcessOk && lbStateOk && lbAssertsOk) && liShown < 6)
            {
                ++liShown;
                std::printf("  %s case %u frame %u: calls %s (console %u words, pc %u) generate %s process %s "
                            "state %s asserts %s (console %u, pc %d)\n", lpcTag, c, f, lbCallsOk ? "ok" : "BAD",
                            lrFrame.muCallWords, static_cast<u32>(gCalls.size()), lbGenerateOk ? "ok" : "BAD",
                            lbProcessOk ? "ok" : "BAD", lbStateOk ? "ok" : "BAD", lbAssertsOk ? "ok" : "BAD",
                            lrFrame.muAsserts, giAsserts - liAsserts0);
                if (!lbCallsOk)
                {
                    u32 luFirstBad = 0;
                    while (luFirstBad < lrFrame.muCallWords && luFirstBad < gCalls.size()
                           && WordMatches(lpuStream[lrFrame.muFirstCallWord + luFirstBad], gCalls[luFirstBad]))
                        ++luFirstBad;
                    const u32 luFrom = luFirstBad > 8u ? luFirstBad - 8u : 0u;
                    std::printf("    first difference at word %u\n    console:", luFirstBad);
                    for (u32 w = luFrom; w < lrFrame.muCallWords && w < luFrom + 24u; ++w)
                        std::printf(" %08X", lpuStream[lrFrame.muFirstCallWord + w]);
                    std::printf("\n    pc     :");
                    for (size_t w = luFrom; w < gCalls.size() && w < luFrom + 24u; ++w)
                        std::printf(" %08X", gCalls[w]);
                    std::printf("\n");
                }
                if (!lbGenerateOk)
                    std::printf("    near clip console %u %08X | pc %u %08X\n", lrFrame.muAfterGenerateHasCustom,
                                lrFrame.muAfterGenerateCustom, gCamera.mbHasCustomNearClipDistance ? 1u : 0u,
                                Bits(gCamera.mfCustomNearClipDistance));
                if (!lbProcessOk)
                    std::printf("    camera w console %08X %08X %08X %08X | pc %08X %08X %08X %08X\n",
                                lrFrame.mauAfterProcess[12], lrFrame.mauAfterProcess[13], lrFrame.mauAfterProcess[14],
                                lrFrame.mauAfterProcess[15], Bits(gCamera.mTransform.wAxis.x),
                                Bits(gCamera.mTransform.wAxis.y), Bits(gCamera.mTransform.wAxis.z),
                                Bits(gCamera.mTransform.wAxis.w));
                if (!lbGenerateOk)
                    std::printf("    camera after generate: console x %08X %08X %08X w %08X %08X %08X | pc x %08X %08X "
                                "%08X w %08X %08X %08X\n", lrFrame.mauAfterGenerate[0], lrFrame.mauAfterGenerate[1],
                                lrFrame.mauAfterGenerate[2], lrFrame.mauAfterGenerate[12], lrFrame.mauAfterGenerate[13],
                                lrFrame.mauAfterGenerate[14], Bits(lSnapshot.xAxis.x), Bits(lSnapshot.xAxis.y),
                                Bits(lSnapshot.xAxis.z), Bits(lSnapshot.wAxis.x), Bits(lSnapshot.wAxis.y),
                                Bits(lSnapshot.wAxis.z));
                if (!lbStateOk)
                    std::printf("    state console traffic %08X reset %u boxes %u %u %u %u resolve %08X %08X %08X %08X"
                                " | pc traffic %08X reset %u boxes %u %u %u %u resolve %08X %08X %08X %08X\n",
                                lrFrame.muTrafficResolution, lrFrame.muResetVehicleCollision,
                                lrFrame.mauFrustumBoxState[0], lrFrame.mauFrustumBoxState[1],
                                lrFrame.mauFrustumBoxState[2], lrFrame.mauFrustumBoxState[3],
                                lrFrame.mauVehicleResolveVector[0], lrFrame.mauVehicleResolveVector[1],
                                lrFrame.mauVehicleResolveVector[2], lrFrame.mauVehicleResolveVector[3],
                                Bits(lpPolicy->mfTrafficCollisionResolution), luReset,
                                static_cast<u32>(lapBoxes[0]->meState), static_cast<u32>(lapBoxes[1]->meState),
                                static_cast<u32>(lapBoxes[2]->meState), static_cast<u32>(lapBoxes[3]->meState),
                                Bits(lpPolicy->mFrustrumCollisionResolver.mVehicleResolveVector.x),
                                Bits(lpPolicy->mFrustrumCollisionResolver.mVehicleResolveVector.y),
                                Bits(lpPolicy->mFrustrumCollisionResolver.mVehicleResolveVector.z),
                                Bits(lpPolicy->mFrustrumCollisionResolver.mVehicleResolveVector.w));
            }
        }
    }

    char lacName[200];
    std::snprintf(lacName, sizeof(lacName), "%s1 %s: every call and argument, in order (%d of %d frames wrong)", lpcTag,
                  lpcArm, liBadCalls, liFrames);
    Check(liBadCalls == 0, lacName);
    std::snprintf(lacName, sizeof(lacName), "%s2 %s: the camera and its near clip after GenerateSceneQueries (%d of %d "
                  "frames wrong)", lpcTag, lpcArm, liBadGenerate, liFrames);
    Check(liBadGenerate == 0, lacName);
    std::snprintf(lacName, sizeof(lacName), "%s3 %s: the camera after ProcessSceneQueryResults (%d of %d frames wrong)",
                  lpcTag, lpcArm, liBadProcess, liFrames);
    Check(liBadProcess == 0, lacName);
    std::snprintf(lacName, sizeof(lacName), "%s4 %s: the policy and its resolver after the frame (%d of %d frames "
                  "wrong)", lpcTag, lpcArm, liBadState, liFrames);
    Check(liBadState == 0, lacName);
    std::snprintf(lacName, sizeof(lacName), "%s5 %s: the asserts of the frame (%d of %d frames wrong)", lpcTag, lpcArm,
                  liBadAsserts, liFrames);
    Check(liBadAsserts == 0, lacName);
}

static void RunFrustumFrames()
{
    const u32 luCases = sizeof(kaL1FrustumCases) / sizeof(kaL1FrustumCases[0]);
    std::vector<const L1FrustumCase*> lCases;
    for (u32 c = 0; c < luCases; ++c)
        lCases.push_back(&kaL1FrustumCases[c]);
    RunFrustumGroup("F", "frustum arm", luCases, lCases.data(), 0, kaL1FrustumFrames, kau32L1FrustumCallStream);
}

static void RunVehicleFrames()
{
    const u32 luCases = sizeof(kaL1VehicleCases) / sizeof(kaL1VehicleCases[0]);
    std::vector<const L1FrustumCase*> lCases;
    std::vector<const L1VehicleCase*> lWorlds;
    for (u32 c = 0; c < luCases; ++c)
    {
        lCases.push_back(&kaL1VehicleCases[c].mFrustum);
        lWorlds.push_back(&kaL1VehicleCases[c]);
    }
    RunFrustumGroup("V", "traffic push-out", luCases, lCases.data(), lWorlds.data(), kaL1VehicleFrames,
                    kau32L1VehicleCallStream);
}
#endif

int main()
{
    static_assert(sizeof(CollisionPolicyAttachedToVehicle) == 0x250, "the console stride");

    CheckConstruct(false, kaL1ConstructStores0, sizeof(kaL1ConstructStores0) / sizeof(kaL1ConstructStores0[0]),
                   "C1-C3 Construct(false)");
    CheckConstruct(true, kaL1ConstructStores1, sizeof(kaL1ConstructStores1) / sizeof(kaL1ConstructStores1[0]),
                   "C4-C6 Construct(true)");

#ifndef L1_NO_LAYOUT
    RunFrames();
    RunFrustumFrames();
    RunVehicleFrames();
#else
    const char* lapcNames[15] = { "P1 calls", "P2 camera after Generate", "P3 camera after Process", "P4 policy state",
                                  "P5 asserts", "F1 calls", "F2 camera after Generate", "F3 camera after Process",
                                  "F4 policy state", "F5 asserts", "V1 calls", "V2 camera after Generate",
                                  "V3 camera after Process", "V4 policy state", "V5 asserts" };
    for (int i = 0; i < 15; ++i)
    {
        char lacName[160];
        std::snprintf(lacName, sizeof(lacName), "%s: not buildable (the revision has no DWARF layout)", lapcNames[i]);
        Check(false, lacName);
    }
#endif

    std::printf("asserts fired: %d\n", giAsserts);
    std::printf("L1AttachedPolicy: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
