// FX-DIRECTOR2: MomentPlayerJumping (type 7). Its default gate remains closed, and this fixture also exercises the
// recovered original debug-enabled filming closure.
//
// The runner extracts the production BrnMomentPlayerJumping.cpp into fxd2_player_jumping.inc. Leaves are bounded and
// typed: BehaviorManager::NewBehaviour records its handle/owners/ref limit; the ICE behavior methods record their
// parameter/time inputs; shotgroup and assertion calls record the data requested. The camera tail's small stores use
// the real named Camera/CameraEffects members. No game executable or full-game link is involved.
//
// Against the ARTIST bodies:
//   Construct       @0x8225F1F0  the inlined Moment::Construct; the four collections (owners, latches, index,
//                                count); the interpolate handle and parameters {8, 0, method 1, mapping 3}; the
//                                cooldown seed 0.5 (flt_82001DA0); meType -1; mbPrepared 0; mpParameters 0
//   Prepare         @0x82251048  SEARCHING; once only: rig slots 1 2 7 10 9 5, bystander 0 2, dropped 11 12 13, and
//                                the ice ShotList clamped to five with the 24-byte DefaultDataArea for a null element
//   Update          @0x82275988  SEARCHING -- the retail path: the landing cooldown integrates mfSimTimestep while
//                                |time in air| is not > FLT_EPSILON (a NaN integrates); the three gates (action flag
//                                bit 3, mbCanUseSlomo, mbAllowJumpMoment) -> not met / can't switch / head bit 18;
//                                inhibited -> head bit 23; all three -> ATTACHED_RIG_SHOT, PrepareCameras,
//                                FOUND_PREPARING, head bit 19. FOUND_PREPARING / VALID and the default arm.
//   Release         @0x8223AED0  cooldown 0; each collection releases its allocated handles only when
//                                mbAllocatedShots; the interpolate handle; not met, head bit 18, SEARCHING
//   AddShot         @0x8224B1B8 / 0x8224B0A8 / 0x8224B130  the non-gating "can't add shots" assert, the Append
//   Collection Prepare @0x82267C40 / 0x82267D88 / 0x82267ED0 / 0x82273360: preincrement, divwu remainder,
//                        one NewBehaviour call with {handle, arb-state owner, moment owner, ref limit 1}, optional params,
//                        then both latches. The four readiness/failure pairs assert both latches and return true/false.
//   UpdateCamera @0x8223AB78: all six type branches, strict 0.5/1.0/1.1 boundaries, camera identity, flash stores,
//                        detached-rig timestep, and the invalid-type assert followed by a true return.
#include "GameSource/Director/MomentController/Moments/BrnMomentPlayerJumping.h"
#include "GameSource/Director/MomentController/BrnMomentSharedInfo.h"
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"
#include "GameSource/Director/Camera/BrnBehaviourManager.h"
#include "GameSource/Director/BrnDirectorICEWrapper.h"
#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <vector>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;
static std::string gLastAssert;
static std::vector<std::string> gAssertLog;
static unsigned gLogWrites = 0;
static std::string gLastLog;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int)
    {
        ++gAsserts;
        gLastAssert = lpcText ? lpcText : "";
        gAssertLog.push_back(gLastAssert);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    void WriteToLog(const char* lpcText)
    {
        ++gLogWrites;
        gLastLog = lpcText ? lpcText : "";
    }
}
}

using namespace BrnDirector;

// ---- recording stand-ins ------------------------------------------------------------------------------------
struct UnSetCall { const void* mpManager; u32 muKey; };
static std::vector<UnSetCall> gUnSets;
static int giCameraConstructs = 0;
static const void* gpNumShotListThis = nullptr;
static u32 guShotListLength = 7;
struct PointerCall { const void* mpThis; u64 muKey; u32 muIndex; };
static std::vector<PointerCall> gPointerCalls;
static u32 guNullElement = 2;                   // this element index resolves to null
static u32 guDefaultDataBytes = 0;
static int giDefaultDataCalls = 0;
static Attrib::RefSpec gaShotElements[8];
static Attrib::RefSpec gDefaultShot;

namespace BrnDirector
{
namespace Camera
{
    void Camera::Construct() { ++giCameraConstructs; }
    void Camera::SetRequestedBorderPostFX(f32 lfAmount) { mEffects.mfBlackBarAmount = lfAmount; }   // Camera.cpp's one store
    void CameraState::SetFlag(u32 luIndex, bool lbValue)                                          // BrnCameraState.cpp's body
    {
        if (lbValue)
            mCurrentFlags.SetBit(luIndex);
        else
            mCurrentFlags.UnSetBit(luIndex);
    }
    void ValidityAccount::MaskToFailFlags() {}   // Moment::PreUpdate's account mask (tested by run_fxdirector_moments.py)
    void BehaviourManager::UnSetBehaviourUsedByHandle(u32 luAllocationKey)
    {
        gUnSets.push_back(UnSetCall{ this, luAllocationKey });
    }
}
    void Moment::SetParameters(const Parameters*) {}
    void Moment::Destruct() {}
}

namespace Attrib
{
    Instance::Instance(Collection*, void*) {}
    Instance::~Instance() {}
    int Instance::GetClass() const { return 0; }
    u64 Instance::GetCollection() const { return 0; }
    void AssertOnClassCheck(int, int, u64) {}

    void* DefaultDataArea(u32 luBytes)
    {
        ++giDefaultDataCalls;
        guDefaultDataBytes = luBytes;
        return &gDefaultShot;
    }
    void* Instance::GetAttributePointer(u64 luAttributeKey, u32 luIndex) const
    {
        gPointerCalls.push_back(PointerCall{ this, luAttributeKey, luIndex });
        return luIndex == guNullElement ? nullptr : &gaShotElements[luIndex & 7u];
    }
namespace Gen
{
    u32 shotgroup::Num_ShotList() const
    {
        gpNumShotListThis = this;
        return guShotListLength;
    }
}
}

// ---- bounded recording leaves for the original allocation path ----------------------------------------------
struct NewBehaviourCall
{
    const void* mpManager;
    const void* mpHandle;
    const void* mpArbitratorOwner;
    const void* mpMomentOwner;
    s32 miReferenceLimit;
    u32 muAllocationKey;
};
static std::vector<NewBehaviourCall> gNewBehaviourCalls;
static Camera::BehaviourManager::BehaviourHelper gBehaviourHelpers[128];
static u32 guNextAllocationKey = 1;

struct IceSetParametersCall { const void* mpBehaviour; const void* mpParameters; };
static std::vector<IceSetParametersCall> gIceSetParametersCalls;
static f32 gfIceTimeRemaining = 0.5f;
static u32 guIceTimeRemainingCalls = 0;

struct DetachedCall { const void* mpBehaviour; bool mbDetached; };
static std::vector<DetachedCall> gDetachedCalls;

static Camera::BehaviourRig gRigBehaviour;
static Camera::BehaviourBystanderCam gBystanderBehaviour;
static Camera::BehaviourIceAnim gIceBehaviour;

template <typename TBehaviour>
static TBehaviour* FixtureBehaviour();

template <>
Camera::BehaviourRig* FixtureBehaviour<Camera::BehaviourRig>() { return &gRigBehaviour; }
template <>
Camera::BehaviourBystanderCam* FixtureBehaviour<Camera::BehaviourBystanderCam>() { return &gBystanderBehaviour; }
template <>
Camera::BehaviourIceAnim* FixtureBehaviour<Camera::BehaviourIceAnim>() { return &gIceBehaviour; }

namespace BrnDirector
{
namespace Camera
{
    template <typename TBehaviour>
    static void RecordNewBehaviour(BehaviourManager& lrManager, BehaviourHandle<TBehaviour>& lrHandle,
                                   void* lpArbitratorOwner, const void* lpMomentOwner, s32 liReferenceLimit)
    {
        const u32 luKey = guNextAllocationKey++;
        TBehaviour* lpBehaviour = FixtureBehaviour<TBehaviour>();
        lrHandle.mbAllocated = true;
        lrHandle.muAllocationKey = luKey;
        lrHandle.mpHelperPool = &lrManager.mBehaviourHelperPool;
        lrHandle.mpManager = &lrManager;
        lrHandle.mpBehaviour = lpBehaviour;

        BehaviourManager::BehaviourHelper& lrHelper = gBehaviourHelpers[luKey];
        lrHelper.mBehaviourPoolHandle.mpObject = lpBehaviour;
        lrHelper.mBehaviourPoolHandle.mpFreeObjectInterface = nullptr;
        lrHelper.mBehaviourPoolHandle.miIndex = static_cast<s32>(luKey);
        lrHelper.mBehaviourPoolHandle.miSize = static_cast<s32>(sizeof(TBehaviour));
        lrHelper.mpDebugArbitratorStateOwner = static_cast<const ArbitratorState*>(lpArbitratorOwner);
        lrHelper.mpDebugMomentOwner = static_cast<const Moment*>(lpMomentOwner);

        gNewBehaviourCalls.push_back(NewBehaviourCall{
            &lrManager, &lrHandle, lpArbitratorOwner, lpMomentOwner, liReferenceLimit, luKey});
    }

    template <>
    void BehaviourManager::NewBehaviour<BehaviourRig>(BehaviourHandle<BehaviourRig>& lrHandle,
                                                       void* lpArbitratorOwner, const void* lpMomentOwner,
                                                       s32 liReferenceLimit)
    {
        RecordNewBehaviour(*this, lrHandle, lpArbitratorOwner, lpMomentOwner, liReferenceLimit);
    }
    template <>
    void BehaviourManager::NewBehaviour<BehaviourBystanderCam>(BehaviourHandle<BehaviourBystanderCam>& lrHandle,
                                                                 void* lpArbitratorOwner, const void* lpMomentOwner,
                                                                 s32 liReferenceLimit)
    {
        RecordNewBehaviour(*this, lrHandle, lpArbitratorOwner, lpMomentOwner, liReferenceLimit);
    }
    template <>
    void BehaviourManager::NewBehaviour<BehaviourIceAnim>(BehaviourHandle<BehaviourIceAnim>& lrHandle,
                                                           void* lpArbitratorOwner, const void* lpMomentOwner,
                                                           s32 liReferenceLimit)
    {
        RecordNewBehaviour(*this, lrHandle, lpArbitratorOwner, lpMomentOwner, liReferenceLimit);
    }

    template <>
    BehaviourManager::BehaviourHelper& BehaviourHandle<BehaviourRig>::GetHelper() const
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");
        return gBehaviourHelpers[muAllocationKey];
    }
    template <>
    BehaviourManager::BehaviourHelper& BehaviourHandle<BehaviourBystanderCam>::GetHelper() const
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");
        return gBehaviourHelpers[muAllocationKey];
    }
    template <>
    BehaviourManager::BehaviourHelper& BehaviourHandle<BehaviourIceAnim>::GetHelper() const
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");
        return gBehaviourHelpers[muAllocationKey];
    }

    // The camera behaviors are allocation leaves here; the collection and moment code stays extracted production code.
    void Behaviour::Construct() {}
    bool Behaviour::Prepare(const BehaviourSharedPrepareReleaseInfo&) { return true; }
    bool Behaviour::Update(Camera&, const BehaviourSharedInfo&) { return true; }
    bool Behaviour::PostCollisionUpdate(Camera&, const BehaviourSharedInfo&) { return true; }
    void Behaviour::Release(const BehaviourSharedPrepareReleaseInfo&) {}
    CollisionPolicy* Behaviour::GetCollisionPolicy() { return nullptr; }
    void Behaviour::SetupTweaker(Utils::Tweaker&) {}
    const char* Behaviour::GetName() const { return "BehaviourFixture"; }

    void BehaviourRig::Construct() {}
    bool BehaviourRig::Prepare(const BehaviourSharedPrepareReleaseInfo&) { return true; }
    bool BehaviourRig::Update(Camera&, const BehaviourSharedInfo&) { return true; }
    CollisionPolicy* BehaviourRig::GetCollisionPolicy() { return nullptr; }
    void BehaviourRig::SetupTweaker(Utils::Tweaker&) {}
    const char* BehaviourRig::GetName() const { return "BehaviourRigFixture"; }
    void BehaviourRig::SetDetached(bool lbDetached)
    {
        mbDetached = lbDetached;
        gDetachedCalls.push_back(DetachedCall{ this, lbDetached });
    }

    void VisibilityCollisionPolicy::GenerateSceneQueries(const CollisionPolicySharedInfo&, Camera&) {}
    void VisibilityCollisionPolicy::ProcessSceneQueryResults(const CollisionPolicySharedInfo&, Camera&) {}

    void BehaviourBystanderCam::Construct() {}
    bool BehaviourBystanderCam::Prepare(const BehaviourSharedPrepareReleaseInfo&) { return true; }
    bool BehaviourBystanderCam::Update(Camera&, const BehaviourSharedInfo&) { return true; }
    CollisionPolicy* BehaviourBystanderCam::GetCollisionPolicy() { return nullptr; }
    void BehaviourBystanderCam::SetupTweaker(Utils::Tweaker&) {}
    const char* BehaviourBystanderCam::GetName() const { return "BehaviourBystanderFixture"; }

    void BehaviourIceAnim::SetParameters(ShotReference* lpParameters)
    {
        gIceSetParametersCalls.push_back(IceSetParametersCall{ this, lpParameters });
    }
    f32 BehaviourIceAnim::GetTimeRemaining()
    {
        ++guIceTimeRemainingCalls;
        return gfIceTimeRemaining;
    }
}
}

#include "fxd2_player_jumping.inc"

// ---- helpers -----------------------------------------------------------------------------------------------
static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcName);
    }
}
static u32 Bits(f32 lfValue) { u32 luBits; std::memcpy(&luBits, &lfValue, 4); return luBits; }

alignas(16) static u8 gaMomentSlot[sizeof(MomentPlayerJumping) + 16];
static Camera::BehaviourManager gManager;
static DirectorResourceManager gResourceManager;
static MomentPlayerJumping* gpPlacedMoment = nullptr;
static Camera::VehicleInfo gPlayerCar;
static GameState gGameState;
static MomentSharedInfo gInfo;
static MomentPlayerJumping::Parameters gMomentParameters;

static Camera::BehaviourManager& Manager() { return gManager; }
static DirectorResourceManager& ResourceManager() { return gResourceManager; }
static const Camera::BehaviourParameterBank& Bank() { return Manager().mBehaviourParameterBank; }

static MomentPlayerJumping* Placed()
{
    if (gpPlacedMoment)
        gpPlacedMoment->~MomentPlayerJumping();
    std::memset(gaMomentSlot, 0xCD, sizeof(gaMomentSlot));
    gpPlacedMoment = new (gaMomentSlot) MomentPlayerJumping;
    return gpPlacedMoment;
}
static MomentPlayerJumping* FreshMoment()
{
    MomentPlayerJumping* lpMoment = Placed();
    Moment* lpBase = lpMoment;
    lpBase->Construct();
    lpBase->SetParameters(&gMomentParameters);
    lpBase->Prepare(&Manager());
    return lpMoment;
}
static bool HeadFlag(const MomentPlayerJumping* lpMoment, u32 luBit)
{
    return lpMoment->mCamera.mState.mHeadFlags.IsBitSet(luBit);
}
static void Tick(MomentPlayerJumping* lpMoment, f32 lfTimeStep = 1.0f / 30.0f)
{
    lpMoment->PreUpdate();
    static_cast<Moment*>(lpMoment)->Update(lfTimeStep, &Manager(), &gInfo);
}
template <typename TCollection>
static bool Constructed(const TCollection& lrCollection, const Moment* lpOwner)
{
    return lrCollection.mpArbStateOwner == nullptr && lrCollection.mpMomentOwner == lpOwner
        && !lrCollection.mbAllocatedShots && !lrCollection.mbHasCurrentCamera && lrCollection.miCurrentCamera == 0
        && lrCollection.maShots.miCount == 0;
}
template <typename TCollection, typename TParams>
static bool ShotIs(const TCollection& lrCollection, u32 luShot, const TParams* lpParams)
{
    const auto& lrShot = lrCollection.maShots.maElements[luShot];
    return lrShot.mpParams == lpParams && !lrShot.mHandle.mbAllocated && lrShot.mHandle.muAllocationKey == 0
        && lrShot.mHandle.mpHelperPool == nullptr && lrShot.mHandle.mpManager == nullptr
        && lrShot.mHandle.mpBehaviour == nullptr;
}


static void CheckNamed(bool lbPass, const std::string& lrName)
{
    Check(lbPass, lrName.c_str());
}

using Rig6Collection = BehaviourCollection<Camera::BehaviourRig, Camera::BehaviourRig::Parameters, 6>;
using BystanderCollection = BehaviourCollection<Camera::BehaviourBystanderCam,
                                               Camera::BehaviourBystanderCam::Parameters, 2>;
using Rig4Collection = BehaviourCollection<Camera::BehaviourRig, Camera::BehaviourRig::Parameters, 4>;
using IceCollection = BehaviourCollection<Camera::BehaviourIceAnim, Camera::BehaviourIceAnim::ShotReference, 5>;

static void SetRigTag(Camera::BehaviourRig::Parameters& lrParams, const char* lpcName)
{
    lrParams.mType = Camera::eBehaviourRig;
    lrParams.mpcDebugName = lpcName;
}
static void SetBystanderTag(Camera::BehaviourBystanderCam::Parameters& lrParams, const char* lpcName)
{
    lrParams.mType = Camera::eBehaviourBystanderCam;
    lrParams.mpcDebugName = lpcName;
}
static void InitializeBankParameterTags()
{
    auto& lr = Manager().mBehaviourParameterBank.mNamedParameters;
    SetRigTag(lr.mRigRearQFwd, "jump rig rear");
    SetRigTag(lr.mRigFrontQCuFwd, "jump rig front");
    SetRigTag(lr.mRigRoofFwd, "jump rig roof");
    SetRigTag(lr.mRigUnderbelly, "jump rig underbelly");
    SetRigTag(lr.mRigFrontQCuFwd2, "jump rig front 2");
    SetRigTag(lr.mRigBootViewFwd, "jump rig boot");
    SetRigTag(lr.mRigDropUnderbelly, "drop rig underbelly");
    SetRigTag(lr.mRigDropFrontQCuFwd, "drop rig front");
    SetRigTag(lr.mRigDropBootViewFwd, "drop rig boot");
    SetBystanderTag(lr.mBystanderJumpLeftParameters, "jump bystander left");
    SetBystanderTag(lr.mBystanderJumpFromBehindParameters, "jump bystander behind");
}

static void ResetBehaviourRecorder()
{
    gNewBehaviourCalls.clear();
    gUnSets.clear();
    gIceSetParametersCalls.clear();
    gDetachedCalls.clear();
    gAssertLog.clear();
    guNextAllocationKey = 1;
    gfIceTimeRemaining = 0.5f;
    guIceTimeRemainingCalls = 0;
    gRigBehaviour.mpParameters = nullptr;
    gRigBehaviour.mbDetached = false;
    gRigBehaviour.meTimestepType = static_cast<Timestep::EType>(0);
    gBystanderBehaviour.mpParameters = nullptr;
}

static const void* SeedParameterProbe(Rig6Collection&)
{
    gRigBehaviour.mpParameters = &Bank().mNamedParameters.mRigRearQFwd;
    return gRigBehaviour.mpParameters;
}
static const void* SeedParameterProbe(Rig4Collection&)
{
    gRigBehaviour.mpParameters = &Bank().mNamedParameters.mRigRearQFwd;
    return gRigBehaviour.mpParameters;
}
static const void* SeedParameterProbe(BystanderCollection&)
{
    gBystanderBehaviour.mpParameters = &Bank().mNamedParameters.mBystanderJumpLeftParameters;
    return gBystanderBehaviour.mpParameters;
}
static const void* SeedParameterProbe(IceCollection&)
{
    gIceSetParametersCalls.clear();
    return nullptr;
}

static size_t ParameterProbeCallCount(Rig6Collection&) { return 0; }
static size_t ParameterProbeCallCount(Rig4Collection&) { return 0; }
static size_t ParameterProbeCallCount(BystanderCollection&) { return 0; }
static size_t ParameterProbeCallCount(IceCollection&) { return gIceSetParametersCalls.size(); }

static bool ParameterProbeUnchanged(Rig6Collection&, const void* lpExpected, size_t)
{
    return gRigBehaviour.mpParameters == lpExpected;
}
static bool ParameterProbeUnchanged(Rig4Collection&, const void* lpExpected, size_t)
{
    return gRigBehaviour.mpParameters == lpExpected;
}
static bool ParameterProbeUnchanged(BystanderCollection&, const void* lpExpected, size_t)
{
    return gBystanderBehaviour.mpParameters == lpExpected;
}
static bool ParameterProbeUnchanged(IceCollection&, const void*, size_t luCallCount)
{
    return gIceSetParametersCalls.size() == luCallCount;
}

static bool ParameterProbeForwarded(Rig6Collection&, const void* lpExpected, size_t)
{
    return gRigBehaviour.mpParameters == lpExpected;
}
static bool ParameterProbeForwarded(Rig4Collection&, const void* lpExpected, size_t)
{
    return gRigBehaviour.mpParameters == lpExpected;
}
static bool ParameterProbeForwarded(BystanderCollection&, const void* lpExpected, size_t)
{
    return gBystanderBehaviour.mpParameters == lpExpected;
}
static bool ParameterProbeForwarded(IceCollection&, const void* lpExpected, size_t luCallCount)
{
    return gIceSetParametersCalls.size() == luCallCount + 1
        && gIceSetParametersCalls.back().mpBehaviour == &gIceBehaviour
        && gIceSetParametersCalls.back().mpParameters == lpExpected;
}

static bool LastNewBehaviourMatches(const void* lpHandle, const Moment* lpOwner)
{
    if (gNewBehaviourCalls.empty())
        return false;
    const NewBehaviourCall& lrCall = gNewBehaviourCalls.back();
    return lrCall.mpManager == &Manager() && lrCall.mpHandle == lpHandle
        && lrCall.mpArbitratorOwner == nullptr && lrCall.mpMomentOwner == lpOwner
        && lrCall.miReferenceLimit == 1;
}

template <typename TCollection>
static bool ReleasePreparedCollection(TCollection& lrCollection)
{
    return lrCollection.Release() && !lrCollection.mbAllocatedShots && !lrCollection.mbHasCurrentCamera;
}

template <typename TCollection>
static void ExerciseCollection(TCollection& lrCollection, MomentPlayerJumping* lpOwner,
                               const char* lpcName, u32 luExpectedCount,
                               const u32* lauSequence, u32 luSequenceLength)
{
    const std::string prefix(lpcName);
    const u32 luCount = lrCollection.maShots.GetLength();
    CheckNamed(luCount == luExpectedCount && luSequenceLength >= 2 && lauSequence[0] == 1,
               prefix + ": expected authored list is present and assembly first selects shot 1");
    if (luCount != luExpectedCount || luSequenceLength < 2 || lauSequence[0] != 1)
        return;

    ResetBehaviourRecorder();
    lrCollection.miCurrentCamera = 0;
    const u32 luNullIndex = lauSequence[0];
    const auto* lpOriginalParams = lrCollection.maShots.maElements[luNullIndex].mpParams;
    CheckNamed(lpOriginalParams != nullptr, prefix + ": non-null parameter is available for the null-forwarding case");
    lrCollection.maShots.maElements[luNullIndex].mpParams = nullptr;
    const void* lpSentinel = SeedParameterProbe(lrCollection);
    const size_t luCallsBefore = ParameterProbeCallCount(lrCollection);
    const size_t luAllocationsBefore = gNewBehaviourCalls.size();

    const bool lbNullPrepared = lrCollection.Prepare(Manager());
    const auto& lrNullHandle = lrCollection.maShots.maElements[luNullIndex].mHandle;
    CheckNamed(lbNullPrepared && lrCollection.miCurrentCamera == static_cast<s32>(luNullIndex)
               && lrCollection.mbAllocatedShots && lrCollection.mbHasCurrentCamera
               && gNewBehaviourCalls.size() == luAllocationsBefore + 1,
               prefix + ": Prepare preincrements to shot 1, allocates once, and raises both latches");
    CheckNamed(LastNewBehaviourMatches(&lrNullHandle, lpOwner)
               && gNewBehaviourCalls.back().muAllocationKey == lrNullHandle.muAllocationKey,
               prefix + ": NewBehaviour receives the selected handle, null state owner, this moment, and ref limit 1");
    CheckNamed(ParameterProbeUnchanged(lrCollection, lpSentinel, luCallsBefore),
               prefix + ": null shot parameters skip SetParameters");

    const size_t luAllocatedCallCount = gNewBehaviourCalls.size();
    const size_t luAllocatedParamCount = ParameterProbeCallCount(lrCollection);
    const bool lbAgain = lrCollection.Prepare(Manager());
    CheckNamed(lbAgain && lrCollection.miCurrentCamera == static_cast<s32>(luNullIndex)
               && gNewBehaviourCalls.size() == luAllocatedCallCount
               && ParameterProbeUnchanged(lrCollection, lpSentinel, luAllocatedParamCount),
               prefix + ": Prepare while allocated reuses the selected behavior without reallocation");

    const u32 luOldKey = lrNullHandle.muAllocationKey;
    const size_t luUnsetsBefore = gUnSets.size();
    const bool lbReleased = ReleasePreparedCollection(lrCollection);
    CheckNamed(lbReleased && lrCollection.miCurrentCamera == static_cast<s32>(luNullIndex)
               && !lrNullHandle.mbAllocated && lrNullHandle.mpHelperPool == nullptr
               && lrNullHandle.mpManager == nullptr && lrNullHandle.mpBehaviour == nullptr
               && gUnSets.size() == luUnsetsBefore + 1 && gUnSets.back().muKey == luOldKey,
               prefix + ": Release drops both latches, releases the selected handle, and preserves its index");
    lrCollection.maShots.maElements[luNullIndex].mpParams = lpOriginalParams;

    for (u32 luStep = 1; luStep < luSequenceLength; ++luStep)
    {
        if (lrCollection.mbAllocatedShots)
            ReleasePreparedCollection(lrCollection);
        const u32 luExpected = lauSequence[luStep];
        const void* lpExpectedParams = lrCollection.maShots.maElements[luExpected].mpParams;
        SeedParameterProbe(lrCollection);
        const size_t luParamCalls = ParameterProbeCallCount(lrCollection);
        const size_t luCallCount = gNewBehaviourCalls.size();
        const bool lbPrepared = lrCollection.Prepare(Manager());
        const auto& lrHandle = lrCollection.maShots.maElements[luExpected].mHandle;
        CheckNamed(lpExpectedParams != nullptr && lbPrepared
                   && lrCollection.miCurrentCamera == static_cast<s32>(luExpected)
                   && gNewBehaviourCalls.size() == luCallCount + 1,
                   prefix + ": release/reprepare follows the pinned preincrement/remainder index sequence");
        CheckNamed(LastNewBehaviourMatches(&lrHandle, lpOwner)
                   && gNewBehaviourCalls.back().muAllocationKey == lrHandle.muAllocationKey,
                   prefix + ": each prepare records the exact selected handle and both owner arguments");
        CheckNamed(ParameterProbeForwarded(lrCollection, lpExpectedParams, luParamCalls),
                   prefix + ": a non-null selected parameter reaches SetParameters unchanged");
    }

    if (lrCollection.mbAllocatedShots)
        ReleasePreparedCollection(lrCollection);

    lrCollection.miCurrentCamera = -1;
    const void* lpZeroParams = lrCollection.maShots.maElements[0].mpParams;
    SeedParameterProbe(lrCollection);
    const size_t luZeroParamCalls = ParameterProbeCallCount(lrCollection);
    const size_t luZeroCallCount = gNewBehaviourCalls.size();
    const bool lbUnsignedWrap = lrCollection.Prepare(Manager());
    const auto& lrZeroHandle = lrCollection.maShots.maElements[0].mHandle;
    CheckNamed(lpZeroParams != nullptr && lbUnsignedWrap && lrCollection.miCurrentCamera == 0
               && gNewBehaviourCalls.size() == luZeroCallCount + 1
               && LastNewBehaviourMatches(&lrZeroHandle, lpOwner)
               && ParameterProbeForwarded(lrCollection, lpZeroParams, luZeroParamCalls),
               prefix + ": the stored word wraps before unsigned remainder, so -1 selects shot zero");
    ReleasePreparedCollection(lrCollection);
}

template <typename TCollection>
static void ExerciseCollectionLatches(TCollection& lrCollection, const char* lpcName)
{
    const std::string prefix(lpcName);
    lrCollection.mbAllocatedShots = false;
    lrCollection.mbHasCurrentCamera = false;
    gAssertLog.clear();
    const bool lbFailed = lrCollection.HasFailed();
    CheckNamed(!lbFailed && gAssertLog.size() == 2
               && gAssertLog[0] == "mbAllocatedShots" && gAssertLog[1] == "mbHasCurrentCamera",
               prefix + ": HasFailed asserts both original latches in order, then returns constant false");

    gAssertLog.clear();
    const bool lbCanSwitch = lrCollection.CanSwitchToMeNow();
    CheckNamed(lbCanSwitch && gAssertLog.size() == 2
               && gAssertLog[0] == "mbAllocatedShots" && gAssertLog[1] == "mbHasCurrentCamera",
               prefix + ": CanSwitchToMeNow asserts both original latches in order, then returns constant true");

    lrCollection.mbAllocatedShots = true;
    lrCollection.mbHasCurrentCamera = true;
    gAssertLog.clear();
    CheckNamed(!lrCollection.HasFailed() && gAssertLog.empty(),
               prefix + ": with both latches up, HasFailed still returns false without asserting");
    gAssertLog.clear();
    CheckNamed(lrCollection.CanSwitchToMeNow() && gAssertLog.empty(),
               prefix + ": with both latches up, CanSwitchToMeNow still returns true without asserting");
    lrCollection.mbAllocatedShots = false;
    lrCollection.mbHasCurrentCamera = false;
}

static void TestFilmingCollections()
{
    MomentPlayerJumping* lpMoment = FreshMoment();
    static const u32 kauRig6[] = { 1, 2, 3, 4, 5, 0, 1 };
    static const u32 kauBystander[] = { 1, 0, 1, 0 };
    static const u32 kauRig4[] = { 1, 2, 0, 1 };
    static const u32 kauIce[] = { 1, 2, 3, 4, 0, 1 };

    ExerciseCollection(lpMoment->mRigCollection, lpMoment, "rig-6", 6, kauRig6, sizeof(kauRig6) / sizeof(kauRig6[0]));
    ExerciseCollectionLatches(lpMoment->mRigCollection, "rig-6");
    ExerciseCollection(lpMoment->mBystanderCollection, lpMoment, "bystander", 2,
                       kauBystander, sizeof(kauBystander) / sizeof(kauBystander[0]));
    ExerciseCollectionLatches(lpMoment->mBystanderCollection, "bystander");
    ExerciseCollection(lpMoment->mDroppedRigCollection, lpMoment, "rig-4", 3, kauRig4,
                       sizeof(kauRig4) / sizeof(kauRig4[0]));
    ExerciseCollectionLatches(lpMoment->mDroppedRigCollection, "rig-4");
    ExerciseCollection(lpMoment->mIceCollection, lpMoment, "ice", 5, kauIce,
                       sizeof(kauIce) / sizeof(kauIce[0]));
    ExerciseCollectionLatches(lpMoment->mIceCollection, "ice");
}

template <typename TCollection>
static void SeedCollectionCamera(TCollection& lrCollection, f32 lfIdentity)
{
    const u32 luIndex = static_cast<u32>(lrCollection.miCurrentCamera);
    const u32 luKey = lrCollection.maShots.maElements[luIndex].mHandle.muAllocationKey;
    Camera::CameraEffects& lrEffects = gBehaviourHelpers[luKey].mCamera.GetEffects();
    lrEffects.mfBlackBarAmount = lfIdentity;
    lrEffects.mfSimTimeScale = 0.0f;
    lrEffects.mStartHookNameString.Set("seed");
    lrEffects.mfStartHookNameBlendAmount = 0.25f;
    lrEffects.mbHasStartHookNameString = false;
}
static void PrepareForCameraTest(IceCollection& lrCollection, f32 lfIdentity)
{
    Check(lrCollection.Prepare(Manager()), "ICE collection prepares through the extracted production body");
    SeedCollectionCamera(lrCollection, lfIdentity);
}
static void PrepareForCameraTest(BystanderCollection& lrCollection, f32 lfIdentity)
{
    Check(lrCollection.Prepare(Manager()), "bystander collection prepares through the extracted production body");
    SeedCollectionCamera(lrCollection, lfIdentity);
}
static void PrepareForCameraTest(Rig4Collection& lrCollection, f32 lfIdentity)
{
    Check(lrCollection.Prepare(Manager()), "rig-4 collection prepares through the extracted production body");
    SeedCollectionCamera(lrCollection, lfIdentity);
}
static void PrepareForCameraTest(Rig6Collection& lrCollection, f32 lfIdentity)
{
    Check(lrCollection.Prepare(Manager()), "rig-6 collection prepares through the extracted production body");
    SeedCollectionCamera(lrCollection, lfIdentity);
}
static bool CameraMatches(const Camera::Camera& lrCamera, f32 lfIdentity, f32 lfTimeScale)
{
    return Bits(lrCamera.GetEffects().mfBlackBarAmount) == Bits(lfIdentity)
        && Bits(lrCamera.GetEffects().mfSimTimeScale) == Bits(lfTimeScale);
}
static bool HasFlash(const Camera::Camera& lrCamera)
{
    const Camera::CameraEffects& lrEffects = lrCamera.GetEffects();
    return std::strcmp(lrEffects.mStartHookNameString.mHookNameString, "2dFlash") == 0
        && Bits(lrEffects.mfStartHookNameBlendAmount) == 0x3F800000u
        && lrEffects.mbHasStartHookNameString;
}
static bool HasSeedHook(const Camera::Camera& lrCamera)
{
    const Camera::CameraEffects& lrEffects = lrCamera.GetEffects();
    return std::strcmp(lrEffects.mStartHookNameString.mHookNameString, "seed") == 0
        && Bits(lrEffects.mfStartHookNameBlendAmount) == Bits(0.25f)
        && !lrEffects.mbHasStartHookNameString;
}
static void InitializeOutputCamera(Camera::Camera& lrCamera)
{
    lrCamera.GetEffects().mfBlackBarAmount = 99.0f;
    lrCamera.GetEffects().mfSimTimeScale = 77.0f;
    lrCamera.GetEffects().mStartHookNameString.Set("output");
    lrCamera.GetEffects().mfStartHookNameBlendAmount = 0.75f;
    lrCamera.GetEffects().mbHasStartHookNameString = true;
}

static void TestSequenceCameraCase(bool lbDroppedSequence, f32 lfTimer,
                                   bool lbExpectAttachedRig, bool lbExpectFlash,
                                   const char* lpcCase)
{
    ResetBehaviourRecorder();
    MomentPlayerJumping* lpMoment = FreshMoment();
    const f32 lfLeadingIdentity = lbDroppedSequence ? 11.0f : 21.0f;
    const f32 lfRigIdentity = lbDroppedSequence ? 12.0f : 22.0f;

    if (lbDroppedSequence)
    {
        PrepareForCameraTest(lpMoment->mDroppedRigCollection, lfLeadingIdentity);
        PrepareForCameraTest(lpMoment->mRigCollection, lfRigIdentity);
        lpMoment->meType = MomentPlayerJumping::E_TYPE_DROPPED_RIG_THEN_ATTACHED_RIG_SEQUENCE;
    }
    else
    {
        PrepareForCameraTest(lpMoment->mBystanderCollection, lfLeadingIdentity);
        PrepareForCameraTest(lpMoment->mRigCollection, lfRigIdentity);
        lpMoment->meType = MomentPlayerJumping::E_TYPE_BYSTANDER_THEN_ATTACHED_RIG_SEQUENCE;
    }

    lpMoment->mfJumpTimer = lfTimer;
    Camera::Camera lrCamera;
    InitializeOutputCamera(lrCamera);
    gAssertLog.clear();
    const bool lbReady = lpMoment->UpdateCamera(lrCamera);
    const f32 lfExpectedIdentity = lbExpectAttachedRig ? lfRigIdentity : lfLeadingIdentity;
    const f32 lfExpectedScale = lbExpectAttachedRig ? 1.0f : 0.2857143f;
    CheckNamed(lbReady && CameraMatches(lrCamera, lfExpectedIdentity, lfExpectedScale),
               std::string(lpcCase) + ": original sequence branch publishes the selected camera and time scale");
    CheckNamed(lbExpectFlash ? HasFlash(lrCamera) : HasSeedHook(lrCamera),
               std::string(lpcCase) + ": flash stores occur only inside the strict 1.0-to-1.1 window");
    CheckNamed(gAssertLog.empty(),
               std::string(lpcCase) + ": prepared sequence collections satisfy both original latch assertions");

    const bool lbShouldDetach = lbDroppedSequence && !lbExpectAttachedRig;
    CheckNamed((gDetachedCalls.size() == (lbShouldDetach ? 1u : 0u))
               && (!lbShouldDetach || (gDetachedCalls[0].mpBehaviour == &gRigBehaviour
                                       && gDetachedCalls[0].mbDetached && gRigBehaviour.mbDetached
                                       && gRigBehaviour.GetTimestepType() == Timestep::E_WORLD_NO_SLOMO)),
               std::string(lpcCase) + ": only the dropped-rig leading arm detaches into world-no-slo-mo");
}

static void TestSingleCameraBranches()
{
    const f32 kfJumpScale = 0.2857143f;
    const f32 kfJustAboveOne = std::nextafter(1.0f, 2.0f);

    ResetBehaviourRecorder();
    MomentPlayerJumping* lpMoment = FreshMoment();
    PrepareForCameraTest(lpMoment->mIceCollection, 31.0f);
    lpMoment->meType = MomentPlayerJumping::E_TYPE_ICE_RIG;
    Camera::Camera lrIceCamera;
    InitializeOutputCamera(lrIceCamera);
    gfIceTimeRemaining = 0.5f;
    const bool lbIceAtBoundary = lpMoment->UpdateCamera(lrIceCamera);
    Check(lbIceAtBoundary && CameraMatches(lrIceCamera, 31.0f, kfJumpScale)
          && guIceTimeRemainingCalls == 1,
          "UpdateCamera ICE_RIG @0x8223AB78: exact 0.5 remaining is ready (less-than comparison is false)");
    gfIceTimeRemaining = std::nextafter(0.5f, 0.0f);
    Camera::Camera lrIceExpiredCamera;
    InitializeOutputCamera(lrIceExpiredCamera);
    const bool lbIceExpired = lpMoment->UpdateCamera(lrIceExpiredCamera);
    Check(!lbIceExpired && CameraMatches(lrIceExpiredCamera, 31.0f, kfJumpScale)
          && guIceTimeRemainingCalls == 2,
          "UpdateCamera ICE_RIG: strictly below the 0.5 cooldown fails after selecting its camera");

    ResetBehaviourRecorder();
    lpMoment = FreshMoment();
    PrepareForCameraTest(lpMoment->mBystanderCollection, 41.0f);
    lpMoment->meType = MomentPlayerJumping::E_TYPE_BYSTANDER_SHOT;
    lpMoment->mfJumpTimer = 1.0f;
    Camera::Camera lrBystanderAtOne;
    InitializeOutputCamera(lrBystanderAtOne);
    const bool lbBystanderAtOne = lpMoment->UpdateCamera(lrBystanderAtOne);
    Check(lbBystanderAtOne && CameraMatches(lrBystanderAtOne, 41.0f, kfJumpScale),
          "UpdateCamera BYSTANDER_SHOT @0x8223AB78: exactly 1.0 remains valid (greater-than comparison is false)");
    lpMoment->mfJumpTimer = kfJustAboveOne;
    Camera::Camera lrBystanderAfterOne;
    InitializeOutputCamera(lrBystanderAfterOne);
    const bool lbBystanderAfterOne = lpMoment->UpdateCamera(lrBystanderAfterOne);
    Check(!lbBystanderAfterOne && CameraMatches(lrBystanderAfterOne, 41.0f, kfJumpScale),
          "UpdateCamera BYSTANDER_SHOT: the first float above 1.0 fails after camera selection");

    ResetBehaviourRecorder();
    lpMoment = FreshMoment();
    PrepareForCameraTest(lpMoment->mDroppedRigCollection, 51.0f);
    lpMoment->meType = MomentPlayerJumping::E_TYPE_DROPPED_RIG_SHOT;
    lpMoment->mfJumpTimer = 1.0f;
    Camera::Camera lrDroppedAtOne;
    InitializeOutputCamera(lrDroppedAtOne);
    const bool lbDroppedAtOne = lpMoment->UpdateCamera(lrDroppedAtOne);
    Check(lbDroppedAtOne && CameraMatches(lrDroppedAtOne, 51.0f, kfJumpScale)
          && gDetachedCalls.size() == 1 && gRigBehaviour.mbDetached
          && gRigBehaviour.GetTimestepType() == Timestep::E_WORLD_NO_SLOMO,
          "UpdateCamera DROPPED_RIG_SHOT: exact 1.0 remains ready and writes detached/world-no-slo-mo");
    lpMoment->mfJumpTimer = kfJustAboveOne;
    Camera::Camera lrDroppedAfterOne;
    InitializeOutputCamera(lrDroppedAfterOne);
    const bool lbDroppedAfterOne = lpMoment->UpdateCamera(lrDroppedAfterOne);
    Check(!lbDroppedAfterOne && CameraMatches(lrDroppedAfterOne, 51.0f, kfJumpScale),
          "UpdateCamera DROPPED_RIG_SHOT: strict expiration still publishes its selected camera");

    ResetBehaviourRecorder();
    lpMoment = FreshMoment();
    PrepareForCameraTest(lpMoment->mRigCollection, 61.0f);
    lpMoment->meType = MomentPlayerJumping::E_TYPE_ATTACHED_RIG_SHOT;
    Camera::Camera lrAttached;
    InitializeOutputCamera(lrAttached);
    const bool lbAttached = lpMoment->UpdateCamera(lrAttached);
    Check(lbAttached && CameraMatches(lrAttached, 61.0f, kfJumpScale),
          "UpdateCamera ATTACHED_RIG_SHOT: selects the rig camera with the jump time scale");

    ResetBehaviourRecorder();
    lpMoment = FreshMoment();
    lpMoment->meType = MomentPlayerJumping::E_INVALID_TYPE;
    Camera::Camera lrInvalid;
    InitializeOutputCamera(lrInvalid);
    gAssertLog.clear();
    const bool lbInvalid = lpMoment->UpdateCamera(lrInvalid);
    Check(lbInvalid && Bits(lrInvalid.GetEffects().mfBlackBarAmount) == Bits(99.0f)
          && Bits(lrInvalid.GetEffects().mfSimTimeScale) == Bits(77.0f)
          && gAssertLog.size() == 1 && gAssertLog[0] == "invalid type",
          "UpdateCamera default asserts and continues, returning true without replacing the supplied camera");
}

static void TestUpdateCameraBranches()
{
    TestSequenceCameraCase(true, 1.0f, false, false, "DROPPED_RIG_THEN_ATTACHED_RIG at 1.0");
    TestSequenceCameraCase(true, 1.05f, true, true, "DROPPED_RIG_THEN_ATTACHED_RIG at 1.05");
    TestSequenceCameraCase(true, 1.1f, true, false, "DROPPED_RIG_THEN_ATTACHED_RIG at 1.1");
    TestSequenceCameraCase(false, 1.0f, false, false, "BYSTANDER_THEN_ATTACHED_RIG at 1.0");
    TestSequenceCameraCase(false, 1.05f, true, true, "BYSTANDER_THEN_ATTACHED_RIG at 1.05");
    TestSequenceCameraCase(false, 1.1f, true, false, "BYSTANDER_THEN_ATTACHED_RIG at 1.1");
    TestSingleCameraBranches();
}

int main()
{
    _putenv_s("BRN_CRASHCAM_DIAG", "");         // the tick witness ([DIAG], NOT X360) stays quiet: the log counts are the traps'
    Manager().mpDirectorResourceManager = &ResourceManager();
    InitializeBankParameterTags();
    gInfo.mpGameState = &gGameState;
    gInfo.mpPlayerCar = &gPlayerCar;
    gInfo.mfTimestep = 0.25f;                   // NOT the addend: the console reads mfSimTimestep (+0x520)
    gInfo.mfSimTimestep = 1.0f / 60.0f;
    gInfo.mbAllowJumpMoment = false;            // the retail value (MainDirector::Construct @0x8225B97C)
    gGameState.miThisFramesActionFlags = 0x8;   // the player is jumping
    gGameState.mbCanUseSlomo = true;
    gPlayerCar.mRaceCarState.mfTimeInAir = 0.0f;

    // ================= Construct @0x8225F1F0 over pool garbage =================
    MomentPlayerJumping* lpMoment = Placed();
    lpMoment->mInterpolater.mbAllocated = true;
    lpMoment->mInterpolater.muAllocationKey = 77;
    lpMoment->mRigCollection.mbAllocatedShots = true;
    Moment* lpBase = lpMoment;
    lpBase->Construct();
    Check(lpMoment->GetState() == Moment::E_STATE_INVALID_INACTIVE && lpMoment->GetType() == Moment::E_MOMENT_PLAYER_JUMPING
              && !lpMoment->IsInhibited() && giCameraConstructs == 1,
          "K1 Construct: the inlined Moment::Construct -- INACTIVE, type 7 (vtable slot 7), not inhibited, "
          "Camera::Construct");
    Check(Constructed(lpMoment->mRigCollection, lpMoment) && Constructed(lpMoment->mBystanderCollection, lpMoment)
              && Constructed(lpMoment->mDroppedRigCollection, lpMoment) && Constructed(lpMoment->mIceCollection, lpMoment),
          "K2 Construct: the four collections -- no arbitrator-state owner, this moment as owner, both latches 0, "
          "current 0, the shot array's count 0 (0x8225F238..0x8225F2A8)");
    Check(!lpMoment->mInterpolater.mbAllocated && lpMoment->mInterpolater.muAllocationKey == 0
              && lpMoment->mInterpolater.mpHelperPool == nullptr && lpMoment->mInterpolater.mpManager == nullptr
              && lpMoment->mInterpolater.mpBehaviour == nullptr
              && lpMoment->mInterpolaterParams.mType == 8 && lpMoment->mInterpolaterParams.mpcDebugName == nullptr
              && lpMoment->mInterpolaterParams.meInterpolationMethod == Camera::BehaviourInterpolate::E_METHOD_ROTATE_ABOUT_PLAYER_CAR
              && lpMoment->mInterpolaterParams.meInterpolationMapping == Camera::BehaviourInterpolate::E_MAPPING_EXPONENTIAL_OUT_X_CUBED,
          "K3 Construct: the interpolate handle's five words zeroed; its parameters {8, no name, method 1 (`stw 1, "
          "0x374`), mapping 3 (`stw 3, 0x378`, stored last)}");
    Check(Bits(lpMoment->mfCooldownTimer) == 0x3F000000u && lpMoment->meType == MomentPlayerJumping::E_INVALID_TYPE
              && !lpMoment->mbPrepared && lpMoment->mpParameters == nullptr,
          "K4 Construct: mfCooldownTimer = kfCooldownTime (flt_82001DA0 == 0x3F000000), meType -1, mbPrepared 0, "
          "mpParameters 0");

    // ================= SetParameters / Prepare @0x82251048 =================
    lpBase->SetParameters(&gMomentParameters);
    const bool lbPrepared = lpBase->Prepare(&Manager());
    Check(lpMoment->mpParameters == &gMomentParameters && lbPrepared && lpMoment->mbPrepared
              && lpMoment->GetState() == Moment::E_STATE_INVALID_SEARCHING,
          "K5 SetParameters stores the block (`stw r4, 0x180(r3)`); Prepare -> SEARCHING, mbPrepared, true");
    const auto& lrBank = Bank().GetNamedParameters();
    const auto& lrRig = lpMoment->mRigCollection;
    Check(lrRig.maShots.miCount == 6 && ShotIs(lrRig, 0, &lrBank.mRigRearQFwd) && ShotIs(lrRig, 1, &lrBank.mRigFrontQCuFwd)
              && ShotIs(lrRig, 2, &lrBank.mRigRoofFwd) && ShotIs(lrRig, 3, &lrBank.mRigUnderbelly)
              && ShotIs(lrRig, 4, &lrBank.mRigFrontQCuFwd2) && ShotIs(lrRig, 5, &lrBank.mRigBootViewFwd),
          "K6 Prepare: the attached-rig collection holds bank rig slots 1 2 7 10 9 5, in that order, handles clear");
    const auto& lrBystander = lpMoment->mBystanderCollection;
    Check(lrBystander.maShots.miCount == 2 && ShotIs(lrBystander, 0, &lrBank.mBystanderJumpLeftParameters)
              && ShotIs(lrBystander, 1, &lrBank.mBystanderJumpFromBehindParameters),
          "K7 Prepare: the bystander collection holds bystander slots 0 and 2");
    const auto& lrDropped = lpMoment->mDroppedRigCollection;
    Check(lrDropped.maShots.miCount == 3 && ShotIs(lrDropped, 0, &lrBank.mRigDropUnderbelly)
              && ShotIs(lrDropped, 1, &lrBank.mRigDropFrontQCuFwd) && ShotIs(lrDropped, 2, &lrBank.mRigDropBootViewFwd),
          "K8 Prepare: the dropped-rig collection holds rig slots 11 12 13");
    const auto& lrIce = lpMoment->mIceCollection;
    const void* lpJumpRig = &ResourceManager().mJumpRig;
    bool lbIceCalls = gPointerCalls.size() == 5;
    for (u32 i = 0; lbIceCalls && i < 5; ++i)
        lbIceCalls = gPointerCalls[i].mpThis == lpJumpRig && gPointerCalls[i].muKey == 0x7533C0E215246B49ULL
                  && gPointerCalls[i].muIndex == i;
    bool lbIceShots = lrIce.maShots.miCount == 5;
    for (u32 i = 0; lbIceShots && i < 5; ++i)
        lbIceShots = ShotIs(lrIce, i, i == guNullElement ? &gDefaultShot : &gaShotElements[i]);
    Check(gpNumShotListThis == lpJumpRig && lbIceCalls && lbIceShots && giDefaultDataCalls == 1 && guDefaultDataBytes == 0x18,
          "K9 Prepare: the jump-rig shot group's ShotList (7 long) clamped to FIVE; each element through "
          "GetAttributePointer(0x7533C0E2_15246B49, i) on the resource manager's mJumpRig, the null one replaced by "
          "DefaultDataArea(0x18)");
    const bool lbAgain = lpBase->Prepare(&Manager());
    Check(lbAgain && lrRig.maShots.miCount == 6 && lrBystander.maShots.miCount == 2 && lrDropped.maShots.miCount == 3
              && lrIce.maShots.miCount == 5 && gPointerCalls.size() == 5,
          "K10 a second Prepare adds nothing (the mbPrepared guard) and reports true");
    guShotListLength = 3;
    gPointerCalls.clear();
    MomentPlayerJumping* lpShort = FreshMoment();
    Check(lpShort->mIceCollection.maShots.miCount == 3 && gPointerCalls.size() == 3,
          "K11 a ShotList shorter than five is taken whole (`cmplwi 5 ; bgt`)");
    guShotListLength = 7;

    // ================= SEARCHING -- the retail path =================
    lpMoment = FreshMoment();
    const unsigned luAssertsRetail = gAsserts, luLogsRetail = gLogWrites;
    Tick(lpMoment);
    Check(!lpMoment->ConditionsAreMet() && !lpMoment->CanSwitchToMeNow() && HeadFlag(lpMoment, 18)
              && lpMoment->GetState() == Moment::E_STATE_INVALID_SEARCHING
              && lpMoment->meType == MomentPlayerJumping::E_INVALID_TYPE
              && gAsserts == luAssertsRetail && gLogWrites == luLogsRetail,
          "K12 RETAIL (jumping, slow-mo allowed, mbAllowJumpMoment false): not met, can't switch, head bit 18, still "
          "SEARCHING -- no shot, no trap, no assert");

    // the landing cooldown
    struct AirCase { f32 mfTimeInAir; bool mbIntegrates; const char* mpcLabel; };
    const f32 kfEpsilon = 1.1920928955078125e-7f;
    const AirCase kaAir[] = {
        { 0.0f, true, "grounded" },
        { -0.0f, true, "-0" },
        { kfEpsilon, true, "exactly FLT_EPSILON (not >)" },
        { -kfEpsilon, true, "-FLT_EPSILON (the sign is cleared)" },
        { std::nextafter(kfEpsilon, 1.0f), false, "just above FLT_EPSILON" },
        { 0.5f, false, "airborne" },
        { -0.3f, false, "negative, |x| > FLT_EPSILON" },
        { std::numeric_limits<f32>::quiet_NaN(), true, "NaN (fails the ordered vcmpgtfp)" },
        { std::numeric_limits<f32>::infinity(), false, "+inf" },
    };
    bool lbAir = true;
    for (const AirCase& lrCase : kaAir)
    {
        lpMoment = FreshMoment();
        gPlayerCar.mRaceCarState.mfTimeInAir = lrCase.mfTimeInAir;
        Tick(lpMoment);
        const f32 lfExpected = lrCase.mbIntegrates ? (gInfo.mfSimTimestep + 0.5f) : 0.5f;
        if (Bits(lpMoment->mfCooldownTimer) != Bits(lfExpected))
        {
            lbAir = false;
            std::printf("      cooldown wrong for %s\n", lrCase.mpcLabel);
        }
    }
    gPlayerCar.mRaceCarState.mfTimeInAir = 0.0f;
    Check(lbAir, "K13 the cooldown integrates mfSimTimestep (+0x520, not mfTimestep) exactly when |time in air| is NOT > "
                 "FLT_EPSILON (flt_82001770): 0, -0, +-epsilon and NaN integrate; above epsilon, negative, +inf do not");

    // the three gates
    struct GateCase { s32 miFlags; bool mbSlomo; bool mbAllow; const char* mpcLabel; };
    const GateCase kaGates[] = {
        { 0x17, true, true, "no action-flag bit 3 (0x17: the stunt / smash / billboard bits only)" },
        { 0x8, false, true, "slow-mo not allowed (GameState +0x100)" },
        { 0x8, true, false, "mbAllowJumpMoment false (+0x524) -- the retail case" },
        { 0x0, false, false, "none" },
    };
    bool lbGates = true;
    for (const GateCase& lrCase : kaGates)
    {
        lpMoment = FreshMoment();
        gGameState.miThisFramesActionFlags = lrCase.miFlags;
        gGameState.mbCanUseSlomo = lrCase.mbSlomo;
        gInfo.mbAllowJumpMoment = lrCase.mbAllow;
        Tick(lpMoment);
        if (lpMoment->ConditionsAreMet() || !HeadFlag(lpMoment, 18) || lpMoment->GetState() != Moment::E_STATE_INVALID_SEARCHING)
        {
            lbGates = false;
            std::printf("      gate case wrong: %s\n", lrCase.mpcLabel);
        }
    }
    gGameState.miThisFramesActionFlags = 0x8;
    gGameState.mbCanUseSlomo = true;
    Check(lbGates, "K14 any gate shut -> not met + head bit 18 (0x82275CEC)");

    // ================= behind the gate (never on retail) =================
    gInfo.mbAllowJumpMoment = true;
    lpMoment = FreshMoment();
    lpMoment->SetInhibited(true);
    lpMoment->mCamera.mState.mHeadFlags.UnSetAll();
    const unsigned luAssertsInhibited = gAsserts;
    Tick(lpMoment);
    Check(!lpMoment->CanSwitchToMeNow() && HeadFlag(lpMoment, 23) && !HeadFlag(lpMoment, 18) && lpMoment->ConditionsAreMet()
              && lpMoment->GetState() == Moment::E_STATE_INVALID_SEARCHING
              && lpMoment->meType == MomentPlayerJumping::E_INVALID_TYPE && gAsserts == luAssertsInhibited,
          "K15 all three gates, inhibited (+0x17B): can't switch, head bit 23 (`oris 0x80`), conditions stay met, "
          "nothing prepared");

    // ================= debug-enabled filming path =================
    gInfo.mbAllowJumpMoment = true;
    gPlayerCar.mRaceCarState.mfTimeInAir = 0.25f;
    ResetBehaviourRecorder();
    lpMoment = FreshMoment();
    lpMoment->mfJumpTimer = 9.0f;
    const unsigned luLogsBefore = gLogWrites;
    gAssertLog.clear();
    Tick(lpMoment);
    Check(lpMoment->meType == MomentPlayerJumping::E_TYPE_ATTACHED_RIG_SHOT && Bits(lpMoment->mfJumpTimer) == 0u
              && !lpMoment->CanSwitchToMeNow() && lpMoment->ConditionsAreMet() && HeadFlag(lpMoment, 19)
              && lpMoment->GetState() == Moment::E_STATE_INVALID_FOUND_PREPARING,
          "K16 all three gates: ATTACHED_RIG_SHOT, PrepareCameras, timer reset, FOUND_PREPARING, head bit 19");
    const auto& lrSelectedRigHandle = lpMoment->mRigCollection.maShots.maElements[1].mHandle;
    Check(gNewBehaviourCalls.size() == 1
              && LastNewBehaviourMatches(&lrSelectedRigHandle, lpMoment)
              && gNewBehaviourCalls[0].muAllocationKey == lrSelectedRigHandle.muAllocationKey
              && gLogWrites == luLogsBefore && gAssertLog.empty(),
          "K17 the enabled path selects rig shot 1 and records the exact handle, both owners, ref limit 1, no trap");

    gAssertLog.clear();
    Tick(lpMoment, 0.05f);
    const Camera::CameraEffects& lrEffects = lpMoment->mCamera.mEffects;
    Check(lpMoment->GetState() == Moment::E_STATE_VALID && Bits(lpMoment->mfJumpTimer) == Bits(0.05f)
              && gNewBehaviourCalls.size() == 1 && gLogWrites == luLogsBefore && gAssertLog.empty(),
          "K18 FOUND_PREPARING sees the original ready result, enters VALID, and keeps the allocated rig behavior");
    Check(Bits(lrEffects.mfBlackBarAmount) == 0x3E19999Au
              && lpMoment->mCamera.mState.mCurrentFlags.IsBitSet(7)
              && lpMoment->mCamera.mState.mCurrentFlags.IsBitSet(1)
              && std::strcmp(lrEffects.mStartHookNameString.mHookNameString, "Jump_Effect") == 0
              && lrEffects.mbHasStartHookNameString && Bits(lrEffects.mfStartHookNameBlendAmount) == 0x3F800000u,
          "K19 VALID update publishes the jump camera tail: border post-FX 0.15, camera bits 7/1, and Jump_Effect");

    gAssertLog.clear();
    Tick(lpMoment, 0.05f);
    Check(lpMoment->GetState() == Moment::E_STATE_VALID && Bits(lpMoment->mfJumpTimer) == Bits(0.1f)
              && gNewBehaviourCalls.size() == 1 && gLogWrites == luLogsBefore && gAssertLog.empty(),
          "K20 a subsequent VALID tick reuses the same behavior and camera without assertions or logging");

    gInfo.mbAllowJumpMoment = false;
    gPlayerCar.mRaceCarState.mfTimeInAir = 0.0f;

    // The collection tests call the extracted template bodies directly for every specialization.
    TestFilmingCollections();

    // Direct calls keep each UpdateCamera arm and strict boundary independently observable.
    TestUpdateCameraBranches();

    // ================= Release @0x8223AED0 =================
    lpMoment = FreshMoment();
    lpMoment->mInterpolater.mbAllocated = true;
    lpMoment->mInterpolater.muAllocationKey = 41;
    lpMoment->mInterpolater.mpManager = &Manager();
    lpMoment->mInterpolater.mpHelperPool = nullptr;
    auto& lrRigShots = lpMoment->mRigCollection;
    lrRigShots.mbAllocatedShots = true;
    lrRigShots.mbHasCurrentCamera = true;
    lrRigShots.maShots.maElements[0].mHandle.mbAllocated = true;
    lrRigShots.maShots.maElements[0].mHandle.muAllocationKey = 12;
    lrRigShots.maShots.maElements[0].mHandle.mpManager = &Manager();
    lrRigShots.maShots.maElements[3].mHandle.mbAllocated = true;
    lrRigShots.maShots.maElements[3].mHandle.muAllocationKey = 15;
    lrRigShots.maShots.maElements[3].mHandle.mpManager = &Manager();
    lpMoment->mBystanderCollection.maShots.maElements[0].mHandle.mbAllocated = true;   // latch down: not released
    lpMoment->mBystanderCollection.maShots.maElements[0].mHandle.mpManager = &Manager();
    lpMoment->PreUpdate();
    gUnSets.clear();
    const bool lbReleased = static_cast<Moment*>(lpMoment)->Release();
    Check(lbReleased && gUnSets.size() == 3 && gUnSets[0].muKey == 12 && gUnSets[1].muKey == 15 && gUnSets[2].muKey == 41
              && gUnSets[0].mpManager == &Manager(),
          "K22 Release: the rig collection's two allocated handles (keys 12, 15) then the interpolate handle (41) are "
          "released -- the bystander collection's handle is not, its mbAllocatedShots latch is down");
    Check(!lrRigShots.mbAllocatedShots && !lrRigShots.mbHasCurrentCamera && !lrRigShots.maShots.maElements[0].mHandle.mbAllocated
              && lrRigShots.maShots.maElements[0].mHandle.mpManager == nullptr
              && lrRigShots.maShots.maElements[0].mHandle.muAllocationKey == 12
              && lpMoment->mBystanderCollection.maShots.maElements[0].mHandle.mbAllocated
              && !lpMoment->mInterpolater.mbAllocated && lpMoment->mInterpolater.mpManager == nullptr
              && lpMoment->mInterpolater.mpHelperPool == nullptr && lpMoment->mInterpolater.muAllocationKey == 41,
          "K23 Release: each released handle's +0x00 / +0x08 / +0x0C / +0x10 cleared and its key (+0x04) kept, as "
          "the inlined BehaviourHandle::Release does; the collection's two latches dropped");
    Check(Bits(lpMoment->mfCooldownTimer) == 0u && !lpMoment->ConditionsAreMet() && !lpMoment->CanSwitchToMeNow()
              && HeadFlag(lpMoment, 18) && lpMoment->GetState() == Moment::E_STATE_INVALID_SEARCHING,
          "K24 Release: cooldown 0, not met, can't switch, head bit 18, SEARCHING (`stw 1, 0x174` -- not INACTIVE)");

    // ================= AddShot's assert =================
    // (The dropped-rig collection has one free slot of four; a full collection's Append writes past its array on the
    // console as well -- its out-of-space assert is non-gating -- so that is not exercised here.)
    lpMoment = FreshMoment();
    gAssertLog.clear();
    lpMoment->mDroppedRigCollection.mbAllocatedShots = true;
    lpMoment->mDroppedRigCollection.AddShot(&lrBank.mRigRearQFwd);
    Check(gAssertLog.size() == 1
              && gAssertLog[0] == "can't add shots when the behaviours have been allocated (although this could be changed)"
              && lpMoment->mDroppedRigCollection.maShots.miCount == 4
              && ShotIs(lpMoment->mDroppedRigCollection, 3, &lrBank.mRigRearQFwd),
          "K25 AddShot on an allocated collection trips its non-gating \"can't add shots\" assert (:159) and still "
          "appends the shot");

    // ================= the rest =================
    Check(std::strcmp(static_cast<Moment*>(lpMoment)->GetName(), "MomentPlayerJumping") == 0
              && lpMoment->GetInstanceType() == Moment::E_MOMENT_PLAYER_JUMPING,
          "K26 GetName \"MomentPlayerJumping\" (0x821F7630), GetInstanceType 7 (0x821F7628)");
    lpMoment = FreshMoment();
    lpMoment->SetState(Moment::E_STATE_INVALID_INACTIVE);
    gAssertLog.clear();
    Tick(lpMoment);
    Check(gAssertLog.size() == 1 && gAssertLog[0] == "unhandled case in switch"
              && lpMoment->GetState() == Moment::E_STATE_INVALID_INACTIVE,
          "K27 Update in any other state trips \"unhandled case in switch\" (:400) and returns");
    MomentPlayerJumping* lpUnprepared = Placed();
    static_cast<Moment*>(lpUnprepared)->Construct();
    lpUnprepared->SetState(Moment::E_STATE_INVALID_SEARCHING);
    gAssertLog.clear();
    Tick(lpUnprepared);
    Check(gAssertLog.size() == 1 && gAssertLog[0] == "mbPrepared" && HeadFlag(lpUnprepared, 18),
          "K28 Update before Prepare trips the non-gating \"mbPrepared\" assert (:263) and runs on");

    std::printf("FxDirector2PlayerJumping: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
