// FX-DIRECTOR2 (crash parity 2026-09-25): THE DIRECTOR'S CAMERA SCENE-QUERY CLOSURE, director side.
//
// Compiles the PRODUCTION bodies of the revision under test -- the runner (run_fxdirector2_scene_query.py)
// writes them into fxdirector2_scene_query.inc; a revision without them does not build, and every
// numeric check then counts as failed:
//   BrnSceneQueryInterface.cpp         Clear @0x8221CD38, LineTestFine @0x82232F68, LineTestNearest @0x82233048,
//                                      VolumeTestDeepest @0x82233128
//   BrnVisibilityTest.cpp              VisibilityTest::GenerateSceneQueries @0x822400B0 / ProcessSceneQueryResults
//                                      @0x8220E290 (+ GetOffscreenTime / IsOnScreen)
//   BrnGeometryCollisionPredictor.cpp  GenerateSceneQueries (inlined 0x82240468..0x822404AC) /
//                                      ProcessSceneQueryResults @0x8220E1B8
//   BrnVisibilityCollisionPolicy.cpp   CollisionPolicy::Fail @0x82206450, VisibilityCollisionPolicy::Construct /
//                                      SetTarget / GenerateSceneQueries @0x822402F8 / ProcessSceneQueryResults
//                                      @0x82224530, GroundConstraint @0x82240200 / @0x8220E3A0
//   BrnDirectorModule.cpp              DirectorModule::ProcessSceneQueryResults @0x82239278, over a fixture shell
//                                      that holds the module's six post offices by their DWARF names
//   BrnDirectorModuleIOSceneQuery.cpp  SceneQueryInputBuffer::Construct @0x8221B310 + the read-locked
//                                      GetResultsQueue @0x82206BA8
//   CgsSceneManagerIO_SceneQueryInterface.cpp   SceneQueryInterface::Append @0x823C4FF8
//   BrnCameraValidityAccount.cpp / BrnCameraState.cpp   ValidityAccount::SetFlag @0x82204028,
//                                      CameraState::ClearFlag @0x822044B0
// plus the header templates BrnPostBox.h / BrnPostOffice.h and the production CgsRandom.cpp
// (RandomInt), CgsIOBuffer.cpp and CgsStrStream.cpp beside it.
//
// The SCENE MANAGER is the fixture: the producer's LineTestNearest / LineTestFine / VolumeTestDeepest
// (their real bodies live in the GameShared TUs) record what they were asked, and a small "world" answers
// every recorded nearest query with a result event in a real OutSceneQueryResultsQueue<4032>, which the
// production router then delivers through the production post offices. IsLookingAtTarget @0x822331F0 and
// Utils::GetZoomFromFOVDegs @0x821F23E8 are fixture-controlled (not under test here).

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <limits>
#include <new>
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/Director/Utils/BrnPostBox.h"
#include "GameSource/Director/Utils/BrnPostOffice.h"
#include "GameSource/Director/Utils/BrnDirectorPostOfficeTypes.h"
#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Director/Camera/BrnCameraState.h"
#include "GameSource/Director/Camera/BrnCameraValidityAccount.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOSceneQuery.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryInterface.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "vendor/renderware/collision/CollisionVolume.hpp"   // rw::collision::Volume -- C22 reads the camera's sphere back

static int  giAsserts = 0;
static int  giGroundHeightAsserts = 0;   // GroundConstraint's own "mfDesiredHeight >= 0.0f" (:719 / :738)
static char gacLastAssert[256] = { 0 };

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int)
    {
        ++giAsserts;
        std::snprintf(gacLastAssert, sizeof(gacLastAssert), "%s", lpcText != 0 ? lpcText : "");
        if (lpcText != 0 && std::strcmp(lpcText, "mfDesiredHeight >= 0.0f") == 0)
            ++giGroundHeightAsserts;
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    DebugPrint* gpDebugPrint = nullptr;   // the [scenequery] witness stays silent here
}
namespace Message
{
    u64 gxMessageFilterFlags = 0;         // VariableEventQueue::OutputQueueContents' log gate (CgsLog.h)
}
}

// ---- the scene manager (fixture) ----------------------------------------------------------------------
namespace
{
    struct NearestAsk
    {
        const void* mpProducer;
        u32         muQueryId;
        u32         mxFlags;
        u32         mxVolumeFlags;
        Vector3     mStart;
        Vector3     mEnd;
        u32         muExclude;
        s32         meMode;
    };
    const int  KI_MAX_ASKS = 64;
    NearestAsk gaNearest[KI_MAX_ASKS];
    int        giNearest = 0;

    NearestAsk gFine;
    int        giFine = 0;

    struct DeepestAsk
    {
        u32         muQueryId;
        u32         mxFlags;
        u32         mxVolumeFlags;
        const void* mpVolume;
        const void* mpTransform;
        u32         muExclude;
        s32         meMode;
        // The producer (@0x822170B0) copies the volume's 0x80-byte block and the 64-byte transform into
        // its event, so the images are kept here the same way: the caller's volume block is a stack local.
        alignas(16) unsigned char macVolume[0x80];
        alignas(16) unsigned char macTransform[0x40];
    };
    DeepestAsk gDeepest;
    int        giDeepest = 0;

    bool gbLookingAt      = true;
    int  giLookingAtCalls = 0;
    f32  gfZoom           = 1.0f;
    f32  gfZoomAskedFov   = -1.0f;
}

bool CgsSceneManager::SceneManagerIO::SceneQueryInterface::LineTestNearest(
    const Vector3& lLineStart, const Vector3& lLineEnd, SceneQueryId lQueryId, u32 lx32EntityTypeFlags,
    u8 lxVolumeTypeFlags, EntityId lExcludeEntityId, ENearestExclusionMode leExclusionMode)
{
    if (giNearest < KI_MAX_ASKS)
    {
        NearestAsk& lrAsk   = gaNearest[giNearest];
        lrAsk.mpProducer    = this;
        lrAsk.muQueryId     = lQueryId.mId;
        lrAsk.mxFlags       = lx32EntityTypeFlags;
        lrAsk.mxVolumeFlags = lxVolumeTypeFlags;
        lrAsk.mStart        = lLineStart;
        lrAsk.mEnd          = lLineEnd;
        lrAsk.muExclude     = static_cast<u32>(lExcludeEntityId);
        lrAsk.meMode        = static_cast<s32>(leExclusionMode);
    }
    ++giNearest;
    return true;
}

bool CgsSceneManager::SceneManagerIO::SceneQueryInterface::LineTestFine(
    const Vector3& lLineStart, const Vector3& lLineEnd, SceneQueryId lQueryId, u32 lx32EntityTypeFlags,
    u8 lxVolumeTypeFlags, EntityId lExcludeEntityId, EExclusionMode leExclusionMode)
{
    gFine.mpProducer    = this;
    gFine.muQueryId     = lQueryId.mId;
    gFine.mxFlags       = lx32EntityTypeFlags;
    gFine.mxVolumeFlags = lxVolumeTypeFlags;
    gFine.mStart        = lLineStart;
    gFine.mEnd          = lLineEnd;
    gFine.muExclude     = static_cast<u32>(lExcludeEntityId);
    gFine.meMode        = static_cast<s32>(leExclusionMode);
    ++giFine;
    return true;
}

int CgsSceneManager::SceneManagerIO::SceneQueryInterface::VolumeTestDeepest(
    u32 lQueryId, u32 lx32EntityTypeFlags, u8 lxVolumeTypeFlags, const void* lpVolumeData,
    const void* lpTransform, u32 lExcludeEntityId, EExclusionMode leExclusionMode)
{
    gDeepest.muQueryId     = lQueryId;
    gDeepest.mxFlags       = lx32EntityTypeFlags;
    gDeepest.mxVolumeFlags = lxVolumeTypeFlags;
    gDeepest.mpVolume      = lpVolumeData;
    gDeepest.mpTransform   = lpTransform;
    gDeepest.muExclude     = lExcludeEntityId;
    gDeepest.meMode        = static_cast<s32>(leExclusionMode);
    std::memcpy(gDeepest.macVolume, lpVolumeData, sizeof(gDeepest.macVolume));
    std::memcpy(gDeepest.macTransform, lpTransform, sizeof(gDeepest.macTransform));
    ++giDeepest;
    return 1;
}

namespace BrnDirector
{
namespace Camera
{
    bool IsLookingAtTarget(const Camera&, const rw::math::vpu::Matrix44Affine&, const AABBox&)
    {
        ++giLookingAtCalls;
        return gbLookingAt;
    }

    namespace Utils
    {
        f32 GetZoomFromFOVDegs(f32 lfFOVDegs)
        {
            gfZoomAskedFov = lfFOVDegs;
            return gfZoom;
        }
    }
}

    // FIXTURE SHELL (not the production class): DirectorModule::ProcessSceneQueryResults reads exactly
    // these six members, by their DWARF names (BrnDirectorModule.h).
    class DirectorModule
    {
    public:
        void ProcessSceneQueryResults(const DirectorIO::SceneQueryInputBuffer* lpSceneQueryInputBuffer);

        LineTestFinePostOffice            mLineTestFinePostOffice;
        LineTestNearestPostOffice         mLineTestNearestPostOffice;
        LineTestFastDoubleSidedPostOffice mLineTestFastDoubleSidedPostOffice;
        SphereTestFastPostOffice          mSphereTestFastPostOffice;
        VolumeTestFinePostOffice          mVolumeTestFinePostOffice;
        VolumeTestDeepestPostOffice       mVolumeTestDeepestPostOffice;
    };
}

// The production bodies under test.
#include "fxdirector2_scene_query.inc"

// FIXTURE: the runner links the production BoxVolume.cpp for SphereVolume::Initialize @0x82BA84E8 (the camera's
// sphere). Its two CreateGPInstance bodies, which nothing here calls, reference the GP method table whose home,
// GPRegistration.cpp, drags the whole GP closure in; an empty table satisfies the link.
#include "vendor/renderware/collision/GPInstance.hpp"
namespace rw { namespace collision {
    const GPInstance::VolumeMethods g_aGPVolumeMethods[GPInstance::NUMINTERNALTYPES] = {};
} }

using namespace BrnDirector;
typedef CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult         NearestResult;
typedef CgsSceneManager::SceneManagerIO::OutEventLineTestFineResult            FineResult;
typedef CgsSceneManager::SceneManagerIO::OutEventLineTestFastDoubleSidedResult FastDSResult;
typedef CgsSceneManager::SceneManagerIO::OutEventSphereTestFastResult          SphereResult;
typedef CgsSceneManager::SceneManagerIO::OutEventVolumeTestDeepestResult       DeepestResult;
typedef CgsSceneManager::SceneManagerIO::OutEventVolumeTestFineResult          VolumeFineResult;

static int giChecks = 0;
static int giFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass)
    {
        ++giFailures;
        std::printf("FAIL  %s\n", lpcName);
    }
}

static Vector3 V3(f32 lfX, f32 lfY, f32 lfZ)
{
    Vector3 lVector;
    lVector.x = lfX; lVector.y = lfY; lVector.z = lfZ; lVector.w = 0.0f;
    return lVector;
}
static bool SameXYZ(const Vector3& a, const Vector3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

// ---- the rig ---------------------------------------------------------------------------------------------
static const f32 KF_REAL_DT       = 1.0f / 60.0f;   // Timestep::E_WORLD_NO_SLOMO -- the visibility / prediction timers
static const f32 KF_WORLD_DT      = 0.000125f;      // Timestep::E_WORLD during a hard stop (the ground constraint's)
static const u32 KU_TARGET_ENTITY = 0x01000400u;    // a race-car entity id (owner 1)

static DirectorModule                                        gModule;
static CgsSceneManager::SceneManagerIO::SceneQueryInterface  gProducer;
static BrnDirector::SceneQueryInterface                      gSqi;
static BrnDirector::Camera::Camera                           gCamera;
static CgsNumeric::Random                                    gRandom;
static BrnDirector::Camera::CollisionPolicySharedInfo        gInfo;
static DirectorIO::SceneQueryInputBuffer                     gInput;
static rw::math::vpu::Matrix44Affine                         gTarget;
static BrnDirector::Camera::AABBox                           gTargetBox;
alignas(16) static unsigned char                             gaPolicyStorage[sizeof(BrnDirector::Camera::VisibilityCollisionPolicy)];

static void ConstructOffices()
{
    gModule.mLineTestFinePostOffice.Construct();
    gModule.mLineTestNearestPostOffice.Construct();
    gModule.mLineTestFastDoubleSidedPostOffice.Construct();
    gModule.mSphereTestFastPostOffice.Construct();
    gModule.mVolumeTestFinePostOffice.Construct();
    gModule.mVolumeTestDeepestPostOffice.Construct();
}

static void SetupRig()
{
    ConstructOffices();
    gSqi.Construct(&gProducer, &gModule.mLineTestFinePostOffice, &gModule.mLineTestNearestPostOffice,
                   &gModule.mLineTestFastDoubleSidedPostOffice, &gModule.mSphereTestFastPostOffice,
                   &gModule.mVolumeTestFinePostOffice, &gModule.mVolumeTestDeepestPostOffice);
    gCamera.mTransform.SetIdentity();
    gCamera.mTransform.wAxis = V3(10.0f, 2.0f, 30.0f);
    gCamera.mfFOV            = 60.0f;
    gTarget.SetIdentity();
    gTarget.wAxis            = V3(10.0f, 2.0f, 50.0f);   // 20 m in front: 400 <= 625 * zoom 1
    gTargetBox.mMin          = V3(-1.0f, 0.0f, -2.0f);
    gTargetBox.mMax          = V3(1.0f, 1.5f, 2.0f);
    gRandom.Construct();
    gInfo.mpRequestInterface = &gSqi;
    gInfo.mpRandom           = &gRandom;
    gInfo.mTimestep.mafTimestep[Timestep::E_WORLD]          = KF_WORLD_DT;
    gInfo.mTimestep.mafTimestep[Timestep::E_WORLD_NO_SLOMO] = KF_REAL_DT;
    gInfo.mTimestep.mafTimestep[Timestep::E_GAME]           = KF_REAL_DT;
    gbLookingAt = true;
    gfZoom      = 1.0f;
}

struct World
{
    bool mbGroundHit;  f32 mfGroundY;
    bool mbPredictHit; f32 mfPredictParam;
    bool mbHitA;       bool mbHitB;
    bool mbInsideGeometry;   // the camera's 0.1 m sphere penetrates something (the deepest query's answer)
};
static const World KW_CLEAR = { true, 0.5f, false, 0.0f, false, false, false };

enum EAsk { E_ASK_GROUND, E_ASK_PREDICT, E_ASK_VIS_A, E_ASK_VIS_B, E_ASK_OTHER };

static EAsk Classify(const NearestAsk& lrAsk)
{
    const Vector3& lrCamera = gCamera.mTransform.wAxis;
    if (lrAsk.mxFlags == 0x1Eu)
        return SameXYZ(lrAsk.mStart, lrCamera) ? E_ASK_VIS_A : E_ASK_VIS_B;
    if (lrAsk.mxFlags == 2u)
        return (lrAsk.mStart.y != lrCamera.y) ? E_ASK_GROUND : E_ASK_PREDICT;
    return E_ASK_OTHER;
}

// The world answers every nearest query asked this frame; the PRODUCTION router delivers.
static void Answer(const World& lrWorld)
{
    gInput.Construct();                                            // 0x8221B310: the queue constructed + prepared
    for (int i = 0; i < giNearest && i < KI_MAX_ASKS; ++i)
    {
        const NearestAsk& lrAsk = gaNearest[i];
        NearestResult lResult;
        std::memset(static_cast<void*>(&lResult), 0, sizeof(lResult));
        lResult.mQueryId.mId = lrAsk.muQueryId;
        switch (Classify(lrAsk))
        {
        case E_ASK_GROUND:
            lResult.mbIntersection = lrWorld.mbGroundHit;
            lResult.mPosition      = V3(lrAsk.mStart.x, lrWorld.mfGroundY, lrAsk.mStart.z);
            break;
        case E_ASK_PREDICT:
            lResult.mbIntersection = lrWorld.mbPredictHit;
            lResult.mfLineParam    = lrWorld.mfPredictParam;
            break;
        case E_ASK_VIS_A: lResult.mbIntersection = lrWorld.mbHitA; break;
        case E_ASK_VIS_B: lResult.mbIntersection = lrWorld.mbHitB; break;
        default: break;
        }
        gInput.mResultsQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResult), 2,
                                      static_cast<s32>(sizeof(lResult)));
    }
    if (giDeepest > 0)                                             // the camera's sphere test (results type 5)
    {
        DeepestResult lResult;
        std::memset(static_cast<void*>(&lResult), 0, sizeof(lResult));
        lResult.mQueryId.mId    = gDeepest.muQueryId;
        lResult.mfDepth         = lrWorld.mbInsideGeometry ? 0.25f : 0.0f;
        lResult.mbIntersection  = lrWorld.mbInsideGeometry;
        gInput.mResultsQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResult), 5,
                                      static_cast<s32>(sizeof(lResult)));
    }
    gInput.LockForRead();
    gModule.ProcessSceneQueryResults(&gInput);
    gInput.UnlockForRead();
}

static void Frame(BrnDirector::Camera::VisibilityCollisionPolicy& lrPolicy, const World& lrWorld)
{
    gSqi.Clear();                                                   // PreSceneQueryUpdate's per-frame Clear
    giNearest = 0;
    giDeepest = 0;
    lrPolicy.GenerateSceneQueries(gInfo, gCamera);
    Answer(lrWorld);
    lrPolicy.ProcessSceneQueryResults(gInfo, gCamera);
}

// A behaviour-pool slot: garbage first, then the policy object, then its Construct + SetTarget.
static BrnDirector::Camera::VisibilityCollisionPolicy& FreshPolicy()
{
    std::memset(gaPolicyStorage, 0xCD, sizeof(gaPolicyStorage));
    BrnDirector::Camera::VisibilityCollisionPolicy* lpPolicy =
        new (gaPolicyStorage) BrnDirector::Camera::VisibilityCollisionPolicy;
    lpPolicy->Construct();
    lpPolicy->SetTarget(gTarget, gTargetBox, CgsSceneManager::EntityId(KU_TARGET_ENTITY));
    std::memset(static_cast<void*>(&gCamera.mValidityAccount), 0, sizeof(gCamera.mValidityAccount));
    gCamera.mState.mCurrentFlags.SetBit(1u);                        // the follow request Fail drops
    gCamera.mTransform.wAxis = V3(10.0f, 2.0f, 30.0f);
    return *lpPolicy;
}

static u32 FailBits()
{
    u32 luBits = 0;
    for (u32 luFlag = 0; luFlag < 14u; ++luFlag)
        if (gCamera.mValidityAccount.mFailedFlags.IsBitSet(luFlag))
            luBits |= 1u << luFlag;
    return luBits;
}

// The console's inlined roll (VisibilityCollisionPolicy::GenerateSceneQueries 0x82240348..0x822403C4, and
// again 0x822403C8..0x82240438): the OLD seed's high word, the 64-bit LCG step (the insrdi pair builds
// 0x5851F42D_4C957F2D; `addi 1`), the unsigned % 101 (`mulhwu 0x446F8657` + `mulli 0x65`), `cmplwi 0x14`.
static bool ConsoleRoll(u64& ruSeed)
{
    const u64 luOld = ruSeed;
    ruSeed = luOld * 0x5851F42D4C957F2Dull + 1ull;
    return (static_cast<u32>(luOld >> 32) % 101u) < 20u;
}

int main()
{
    using BrnDirector::Camera::VisibilityCollisionPolicy;
    using BrnDirector::Camera::VisibilityTest;
    using BrnDirector::Camera::GeometryCollisionPredictor;

    // ================= O. the post boxes and offices (BrnPostBox.h / BrnPostOffice.h) =================
    {
        LineTestNearestPostOffice lOffice;
        lOffice.Construct();
        LineTestNearestPostBox laBox[3];
        for (int i = 0; i < 3; ++i) laBox[i].Construct();
        const u16 lu0 = lOffice.AddPostBox(laBox[0]);
        const u16 lu1 = lOffice.AddPostBox(laBox[1]);
        const u16 lu2 = lOffice.AddPostBox(laBox[2]);
        Check(lu0 == 0 && lu1 == 1 && lu2 == 2 && lOffice.GetLength() == 3
                  && laBox[0].GetState() == LineTestNearestPostBox::E_STATE_WAITING_FOR_PACKAGE
                  && laBox[2].GetState() == LineTestNearestPostBox::E_STATE_WAITING_FOR_PACKAGE,
              "O1 AddPostBox mints ids 0,1,2 and leaves every box WAITING_FOR_PACKAGE (0x8222D288)");

        NearestResult lPackage;
        std::memset(static_cast<void*>(&lPackage), 0, sizeof(lPackage));
        lPackage.mfLineParam    = 0.25f;
        lPackage.mbIntersection = true;
        lOffice.Deliver(1, lPackage);
        Check(laBox[1].HasPackage() && laBox[1].GetPackage().mfLineParam == 0.25f && laBox[1].GetPackage().mbIntersection
                  && laBox[0].GetState() == LineTestNearestPostBox::E_STATE_WAITING_FOR_PACKAGE
                  && laBox[2].GetState() == LineTestNearestPostBox::E_STATE_WAITING_FOR_PACKAGE,
              "O2 Deliver(1) hands the package to box 1 only (0x8222D3B8 -> TakePackage 0x821FF128)");

        const int liBefore = giAsserts;
        lOffice.Deliver(1, lPackage);
        Check(giAsserts == liBefore + 1 && std::strstr(gacLastAssert, "E_STATE_WAITING_FOR_PACKAGE") != 0,
              "O3 a second delivery to a box that already GOT its package trips BrnPostBox.cpp:65");

        LineTestNearestPostOffice lOtherOffice;
        lOtherOffice.Construct();
        const int liBeforeWaiting = giAsserts;
        lOtherOffice.AddPostBox(laBox[0]);                 // laBox[0] is still WAITING in lOffice
        Check(giAsserts == liBeforeWaiting + 1 && std::strstr(gacLastAssert, "meState == E_STATE_EMPTY") != 0,
              "O4 asking again through a box whose answer is still out trips WaitForPackage's EMPTY tripwire (BrnPostBox.cpp:54)");

        LineTestFinePostOffice lFineOffice;
        lFineOffice.Construct();
        LineTestFinePostBox lGot, lWaiting;
        lGot.Construct(); lWaiting.Construct();
        lFineOffice.AddPostBox(lGot);
        lFineOffice.AddPostBox(lWaiting);
        FineResult lFineRecord;
        std::memset(static_cast<void*>(&lFineRecord), 0, sizeof(lFineRecord));
        lFineOffice.Deliver(0, &lFineRecord);
        lFineOffice.Clear();
        Check(lGot.GetState() == LineTestFinePostBox::E_STATE_EMPTY
                  && lWaiting.GetState() == LineTestFinePostBox::E_STATE_WAITING_FOR_PACKAGE && lFineOffice.GetLength() == 0,
              "O5 the fine office's Clear (sub_8221CC98) empties the box that GOT its pointer, leaves the waiting one");

        lOffice.Clear();
        Check(lOffice.GetLength() == 0 && laBox[1].GetState() == LineTestNearestPostBox::E_STATE_GOT_PACKAGE,
              "O6 any other office's Clear only forgets its boxes (the inlined `stw 0` to the length word)");
    }

    // ================= S. the director's query handle (BrnSceneQueryInterface.cpp) =================
    SetupRig();
    {
        giNearest = 0;
        LineTestNearestPostBox lBoxA, lBoxB;
        lBoxA.Construct(); lBoxB.Construct();
        gSqi.LineTestNearest(lBoxA, 0x1Eu, 0xFFu, V3(1.0f, 2.0f, 3.0f), V3(4.0f, 5.0f, 6.0f),
                             CgsSceneManager::EntityId(KU_TARGET_ENTITY),
                             CgsSceneManager::SceneManagerIO::E_EXCLUDE_ALL_CHILD_PARTS);
        const NearestAsk& lrAsk = gaNearest[0];
        Check(giNearest == 1 && lrAsk.mpProducer == &gProducer && lrAsk.muQueryId == 0x10000u
                  && lrAsk.mxFlags == 0x1Eu && lrAsk.mxVolumeFlags == 0xFFu
                  && SameXYZ(lrAsk.mStart, V3(1.0f, 2.0f, 3.0f)) && SameXYZ(lrAsk.mEnd, V3(4.0f, 5.0f, 6.0f))
                  && lrAsk.muExclude == KU_TARGET_ENTITY && lrAsk.meMode == 1
                  && lBoxA.GetState() == LineTestNearestPostBox::E_STATE_WAITING_FOR_PACKAGE,
              "S1 LineTestNearest (0x82233048): id = (owner 1 << 16) | box index, every argument forwarded, box WAITING");
        gSqi.LineTestNearest(lBoxB, 2u, 0xFFu, V3(0, 0, 0), V3(0, -1, 0), CgsSceneManager::EntityId(0xFFFFFFFFu),
                             CgsSceneManager::SceneManagerIO::E_EXCLUDE_ENTITY_ONLY);
        Check(giNearest == 2 && gaNearest[1].muQueryId == 0x10001u && gaNearest[1].meMode == 0,
              "S2 the next nearest query takes index 1 (0x10001)");

        LineTestFinePostBox lFineBox;
        lFineBox.Construct();
        giFine = 0;
        gSqi.LineTestFine(lFineBox, 6u, 0x0Fu, V3(7, 8, 9), V3(1, 1, 1), CgsSceneManager::EntityId(0x02000000u),
                          CgsSceneManager::SceneManagerIO::E_EXCLUDE_ENTITY_ONLY);
        Check(giFine == 1 && gFine.muQueryId == 0x10000u && gFine.mxFlags == 6u && gFine.mxVolumeFlags == 0x0Fu
                  && gFine.muExclude == 0x02000000u && gModule.mLineTestFinePostOffice.GetLength() == 1
                  && lFineBox.GetState() == LineTestFinePostBox::E_STATE_WAITING_FOR_PACKAGE,
              "S3 LineTestFine (0x82232F68) mints its own office's id and forwards to the fine producer");

        VolumeTestDeepestPostBox lDeepBox;
        lDeepBox.Construct();
        giDeepest = 0;
        const u32 lauVolume[32] = { 1, 2, 3, 4 };
        gSqi.VolumeTestDeepest(lDeepBox, 30u, 0xFFu, lauVolume, gCamera.mTransform,
                               CgsSceneManager::EntityId(0xFFFFFFFFu), CgsSceneManager::SceneManagerIO::E_EXCLUDE_ENTITY_ONLY);
        Check(giDeepest == 1 && gDeepest.muQueryId == 0x10000u && gDeepest.mxFlags == 30u && gDeepest.mpVolume == lauVolume
                  && gDeepest.mpTransform == &gCamera.mTransform && gDeepest.muExclude == 0xFFFFFFFFu
                  && gModule.mVolumeTestDeepestPostOffice.GetLength() == 1,
              "S4 VolumeTestDeepest (0x82233128): the volume and transform go by address, the id from the deepest office");

        gSqi.Clear();
        Check(gModule.mLineTestFinePostOffice.GetLength() == 0 && gModule.mLineTestNearestPostOffice.GetLength() == 0
                  && gModule.mVolumeTestDeepestPostOffice.GetLength() == 0,
              "S5 Clear (0x8221CD38) empties every office's map");
    }

    // ================= R. the result router (DirectorModule::ProcessSceneQueryResults @0x82239278) ==========
    SetupRig();
    {
        LineTestFinePostBox            lFine;     lFine.Construct();
        LineTestNearestPostBox         lNear0;    lNear0.Construct();
        LineTestNearestPostBox         lNear1;    lNear1.Construct();
        LineTestFastDoubleSidedPostBox lFastDS;   lFastDS.Construct();
        SphereTestFastPostBox          lSphere;   lSphere.Construct();
        VolumeTestDeepestPostBox       lDeep;     lDeep.Construct();
        VolumeTestFinePostBox          lVolFine;  lVolFine.Construct();
        gModule.mLineTestFinePostOffice.AddPostBox(lFine);
        gModule.mLineTestNearestPostOffice.AddPostBox(lNear0);
        gModule.mLineTestNearestPostOffice.AddPostBox(lNear1);
        gModule.mLineTestFastDoubleSidedPostOffice.AddPostBox(lFastDS);
        gModule.mSphereTestFastPostOffice.AddPostBox(lSphere);
        gModule.mVolumeTestDeepestPostOffice.AddPostBox(lDeep);
        gModule.mVolumeTestFinePostOffice.AddPostBox(lVolFine);

        gInput.Construct();
        FineResult lFineRecord;          std::memset(static_cast<void*>(&lFineRecord), 0, sizeof(lFineRecord));
        lFineRecord.mQueryId.Set(1, 0);  lFineRecord.miNumIntersections = 0;
        NearestResult lNearRecord1;      std::memset(static_cast<void*>(&lNearRecord1), 0, sizeof(lNearRecord1));
        lNearRecord1.mQueryId.Set(1, 1); lNearRecord1.mfLineParam = 0.75f; lNearRecord1.mbIntersection = true;
        NearestResult lNearRecord0;      std::memset(static_cast<void*>(&lNearRecord0), 0, sizeof(lNearRecord0));
        lNearRecord0.mQueryId.Set(1, 0); lNearRecord0.mfLineParam = 0.125f;
        FastDSResult lFastRecord;        std::memset(static_cast<void*>(&lFastRecord), 0, sizeof(lFastRecord));
        lFastRecord.mQueryId.Set(1, 0);  lFastRecord.mbIntersection = true;
        SphereResult lSphereRecord;      std::memset(static_cast<void*>(&lSphereRecord), 0, sizeof(lSphereRecord));
        lSphereRecord.mQueryId.Set(1, 0); lSphereRecord.mbIntersection = true;
        DeepestResult lDeepRecord;       std::memset(static_cast<void*>(&lDeepRecord), 0, sizeof(lDeepRecord));
        lDeepRecord.mQueryId.Set(1, 0);  lDeepRecord.mfDepth = 0.0625f; lDeepRecord.mbIntersection = true;
        VolumeFineResult lVolRecord;     std::memset(static_cast<void*>(&lVolRecord), 0, sizeof(lVolRecord));
        lVolRecord.mQueryId.Set(1, 0);   lVolRecord.miNumEntities = 3;
        const u32 luUnknown[2] = { 0x10000u, 0u };

        gInput.mResultsQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lFineRecord), 1, sizeof(lFineRecord));
        gInput.mResultsQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNearRecord1), 2, sizeof(lNearRecord1));
        gInput.mResultsQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(luUnknown), 7, sizeof(luUnknown));
        gInput.mResultsQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNearRecord0), 2, sizeof(lNearRecord0));
        gInput.mResultsQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lFastRecord), 3, sizeof(lFastRecord));
        gInput.mResultsQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSphereRecord), 4, sizeof(lSphereRecord));
        gInput.mResultsQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lDeepRecord), 5, sizeof(lDeepRecord));
        gInput.mResultsQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lVolRecord), 6, sizeof(lVolRecord));

        const CgsModule::Event* lpFirst = 0;
        s32 liSize = 0;
        gInput.mResultsQueue.GetFirstEvent(&lpFirst, &liSize);    // the fine record's address in the queue

        const int liBefore = giAsserts;
        gInput.LockForRead();
        gModule.ProcessSceneQueryResults(&gInput);
        gInput.UnlockForRead();

        Check(lFine.HasPackage() && lFine.GetPackage() == reinterpret_cast<const FineResult*>(lpFirst)
                  && lNear0.HasPackage() && lNear0.GetPackage().mfLineParam == 0.125f
                  && lFastDS.HasPackage() && lFastDS.GetPackage().mbIntersection
                  && lSphere.HasPackage() && lSphere.GetPackage().mbIntersection
                  && lDeep.HasPackage() && lDeep.GetPackage().mfDepth == 0.0625f && lDeep.GetPackage().mbIntersection
                  && lVolFine.HasPackage() && lVolFine.GetPackage().miNumEntities == 3,
              "R1 types 1..6 reach their six offices (5 -> deepest, 6 -> volume fine); type 1 delivers a POINTER into the queue");
        Check(lNear1.HasPackage() && lNear1.GetPackage().mfLineParam == 0.75f && lNear1.GetPackage().mbIntersection,
              "R2 routing is by the query id's index, not by arrival order (index 1 arrived first)");
        Check(giAsserts == liBefore + 1 && std::strstr(gacLastAssert, "Unhandled result type") != 0,
              "R3 an unknown result type trips the default arm's assert once (BrnDirectorModule.cpp:537); the rest still land");
    }

    // ================= A. SceneQueryInterface::Append @0x823C4FF8 (GameShared G1) =================
    {
        using namespace CgsSceneManager::SceneManagerIO;
        static CgsModule::EventQueue<InEventLineTestNearest, 8>   lDestNear, lSrcNear;
        static CgsModule::EventQueue<InEventLineTestFine, 8>      lDestFine, lSrcFine;
        static CgsModule::EventQueue<InEventVolumeTestDeepest, 4> lDestDeep, lSrcDeep;
        static CgsModule::EventQueue<InEventSphereTestFast, 4>    lDestSphere;
        static CgsModule::EventQueue<InEventTriangleCollisionLineTestNearest, 4> lDestTri, lSrcTri;
        lDestNear.Construct(); lSrcNear.Construct(); lDestFine.Construct(); lSrcFine.Construct();
        lDestDeep.Construct(); lSrcDeep.Construct(); lDestSphere.Construct(); lDestTri.Construct(); lSrcTri.Construct();

        InEventLineTestNearest lNear;
        std::memset(static_cast<void*>(&lNear), 0, sizeof(lNear));
        lNear.mQueryId.mId = 0x50000u; lDestNear.AddEvent(lNear);
        lNear.mQueryId.mId = 0x10000u; lSrcNear.AddEvent(lNear);
        lNear.mQueryId.mId = 0x10001u; lSrcNear.AddEvent(lNear);
        InEventLineTestFine lFineEvent;
        std::memset(static_cast<void*>(&lFineEvent), 0, sizeof(lFineEvent));
        lFineEvent.mQueryId.mId = 0x10000u; lSrcFine.AddEvent(lFineEvent);
        InEventVolumeTestDeepest lDeepEvent;
        std::memset(static_cast<void*>(&lDeepEvent), 0x5A, sizeof(lDeepEvent));
        lSrcDeep.AddEvent(lDeepEvent);
        InEventTriangleCollisionLineTestNearest lTri;
        std::memset(static_cast<void*>(&lTri), 0, sizeof(lTri));
        lSrcTri.AddEvent(lTri);

        CgsSceneManager::SceneManagerIO::SceneQueryInterface lDest;
        CgsSceneManager::SceneManagerIO::SceneQueryInterface lSource;
        std::memset(static_cast<void*>(&lDest), 0, sizeof(lDest));
        std::memset(static_cast<void*>(&lSource), 0, sizeof(lSource));
        lDest.mpFineLineTestNearestQueue              = &lDestNear;
        lDest.mpFineLineTestQueue                     = &lDestFine;
        lDest.mpFineVolumeTestDeepestQueue            = &lDestDeep;
        lDest.mpFineSphereTestFastQueue               = &lDestSphere;
        lDest.mpTriangleCollisionLineTestNearestQueue = &lDestTri;
        lSource.mpFineLineTestNearestQueue              = &lSrcNear;
        lSource.mpFineLineTestQueue                     = &lSrcFine;
        lSource.mpFineVolumeTestDeepestQueue            = &lSrcDeep;
        lSource.mpTriangleCollisionLineTestNearestQueue = &lSrcTri;

        const int liBefore = giAsserts;
        lDest.Append(lSource);
        Check(lDestNear.GetLength() == 3 && lDestNear.GetEvent(0).mQueryId.mId == 0x50000u
                  && lDestNear.GetEvent(1).mQueryId.mId == 0x10000u && lDestNear.GetEvent(2).mQueryId.mId == 0x10001u
                  && lDestFine.GetLength() == 1 && lDestFine.GetEvent(0).mQueryId.mId == 0x10000u
                  && lDestDeep.GetLength() == 1 && lDestDeep.GetEvent(0).macOpaquePayload[0x80] == 0x5A,
              "A1 Append merges the fine queues onto the tail, in order, payloads intact");
        Check(lDestTri.GetLength() == 0 && lDestSphere.GetLength() == 0 && giAsserts == liBefore,
              "A2 the triangle-collision slots are not merged, a null source slot leaves its queue alone, no tripwire");
    }

    // ================= V. VisibilityTest (0x822400B0 / 0x8220E290) =================
    SetupRig();
    {
        VisibilityTest lTest;
        lTest.Construct();
        lTest.mbIsOnScreen = false; lTest.mfOffscreenTime = 0.3f;
        giNearest = 0;
        lTest.GenerateSceneQueries(gCamera, KF_REAL_DT, gRandom, &gSqi, gTarget, gTargetBox, false,
                                   CgsSceneManager::EntityId(KU_TARGET_ENTITY));
        Check(giNearest == 0 && !lTest.mbIsOnScreen && lTest.mfOffscreenTime == 0.3f,
              "V1 not a test frame: no queries, the on-screen state and its timer untouched");

        gSqi.Clear(); giNearest = 0; gfZoomAskedFov = -1.0f;
        lTest.GenerateSceneQueries(gCamera, KF_REAL_DT, gRandom, &gSqi, gTarget, gTargetBox, true,
                                   CgsSceneManager::EntityId(KU_TARGET_ENTITY));
        Check(lTest.mbIsOnScreen && lTest.mfOffscreenTime == 0.0f && gfZoomAskedFov == 60.0f,
              "V2 looking at a target 20 m away (400 <= 25^2 * zoom): on screen, the off-screen timer reset, zoom from the FOV");
        Check(giNearest == 2
                  && gaNearest[0].mxFlags == 0x1Eu && gaNearest[0].mxVolumeFlags == 0xFFu
                  && SameXYZ(gaNearest[0].mStart, gCamera.mTransform.wAxis) && SameXYZ(gaNearest[0].mEnd, gTarget.wAxis)
                  && SameXYZ(gaNearest[1].mStart, gTarget.wAxis) && SameXYZ(gaNearest[1].mEnd, gCamera.mTransform.wAxis)
                  && gaNearest[0].muExclude == KU_TARGET_ENTITY && gaNearest[1].muExclude == KU_TARGET_ENTITY
                  && gaNearest[0].meMode == 1 && gaNearest[1].meMode == 1,
              "V3 two nearest tests, camera->target then target->camera: flags 30, volumes 0xFF, the target and all its parts excluded");

        gTarget.wAxis = V3(10.0f, 2.0f, 60.0f);                          // 30 m: 900 > 625
        gSqi.Clear(); giNearest = 0;
        lTest.GenerateSceneQueries(gCamera, KF_REAL_DT, gRandom, &gSqi, gTarget, gTargetBox, true,
                                   CgsSceneManager::EntityId(KU_TARGET_ENTITY));
        const bool lbFarOff = !lTest.mbIsOnScreen;
        gfZoom = 2.0f;                                                   // 900 <= 625 * 2
        gSqi.Clear(); giNearest = 0;
        lTest.GenerateSceneQueries(gCamera, KF_REAL_DT, gRandom, &gSqi, gTarget, gTargetBox, true,
                                   CgsSceneManager::EntityId(KU_TARGET_ENTITY));
        Check(lbFarOff && lTest.mbIsOnScreen, "V4 the distance cap is 25 m squared times the zoom (30 m: off at zoom 1, on at zoom 2)");
        gfZoom = 1.0f;
        gTarget.wAxis = V3(10.0f, 2.0f, 50.0f);

        gbLookingAt = false;
        gSqi.Clear(); giNearest = 0;
        lTest.GenerateSceneQueries(gCamera, KF_REAL_DT, gRandom, &gSqi, gTarget, gTargetBox, true,
                                   CgsSceneManager::EntityId(KU_TARGET_ENTITY));
        Check(!lTest.mbIsOnScreen && giNearest == 2, "V5 not looking at the target (frustum test false): off screen, the line tests still go out");
        gbLookingAt = true;

        lTest.mbTestLookingAt = false; lTest.mbIsOnScreen = false;
        gSqi.Clear(); giNearest = 0;
        lTest.GenerateSceneQueries(gCamera, KF_REAL_DT, gRandom, &gSqi, gTarget, gTargetBox, true,
                                   CgsSceneManager::EntityId(KU_TARGET_ENTITY));
        Check(!lTest.mbIsOnScreen && giNearest == 2, "V6 with looking-at testing off the on-screen state is left alone; the line tests still go out");
        lTest.mbTestLookingAt = true; lTest.mbIsOnScreen = true;

        // Process: A hits.
        World lWorld = KW_CLEAR; lWorld.mbHitA = true;
        gSqi.Clear(); giNearest = 0;
        lTest.GenerateSceneQueries(gCamera, KF_REAL_DT, gRandom, &gSqi, gTarget, gTargetBox, true,
                                   CgsSceneManager::EntityId(KU_TARGET_ENTITY));
        Answer(lWorld);
        lTest.ProcessSceneQueryResults(KF_REAL_DT);
        Check(lTest.mbOccluded && lTest.mfOccludedTime == KF_REAL_DT
                  && lTest.mLineTestA.GetState() == LineTestNearestPostBox::E_STATE_EMPTY
                  && lTest.mLineTestB.GetState() == LineTestNearestPostBox::E_STATE_EMPTY,
              "V7 a hit on A: occluded, the occluded timer starts with this frame's dt, both boxes emptied");

        // No packages this frame: the state holds and the timer runs.
        lTest.ProcessSceneQueryResults(KF_REAL_DT);
        Check(lTest.mbOccluded && lTest.mfOccludedTime == KF_REAL_DT + KF_REAL_DT,
              "V8 a frame with no answers keeps the occluded state and keeps counting");

        // Both miss: clear, timer reset.
        gSqi.Clear(); giNearest = 0;
        lTest.GenerateSceneQueries(gCamera, KF_REAL_DT, gRandom, &gSqi, gTarget, gTargetBox, true,
                                   CgsSceneManager::EntityId(KU_TARGET_ENTITY));
        Answer(KW_CLEAR);
        lTest.ProcessSceneQueryResults(KF_REAL_DT);
        Check(!lTest.mbOccluded && lTest.mfOccludedTime == 0.0f, "V9 both lines clear: not occluded, the occluded timer back to 0");

        lTest.mbIsOnScreen = false; lTest.mfOffscreenTime = 0.0f;
        lTest.ProcessSceneQueryResults(KF_REAL_DT);
        Check(lTest.mfOffscreenTime == KF_REAL_DT, "V10 off screen: the off-screen timer counts this frame's dt");
    }

    // ================= P. GeometryCollisionPredictor =================
    SetupRig();
    {
        GeometryCollisionPredictor lPredictor;
        lPredictor.Construct();
        lPredictor.SetVelocity(V3(3.0f, 0.0f, -4.0f));
        gSqi.Clear(); giNearest = 0;
        lPredictor.GenerateSceneQueries(gCamera, KF_REAL_DT, gRandom, &gSqi);
        Check(giNearest == 1 && gaNearest[0].mxFlags == 2u && gaNearest[0].mxVolumeFlags == 0xFFu
                  && SameXYZ(gaNearest[0].mStart, gCamera.mTransform.wAxis)
                  && SameXYZ(gaNearest[0].mEnd, V3(13.0f, 2.0f, 26.0f))
                  && gaNearest[0].muExclude == 0xFFFFFFFFu && gaNearest[0].meMode == 0,
              "P1 one world-only line from the camera to camera + velocity * 1 s (KF_LOOKAHEAD_TIME 0x82001B6C)");
        World lWorld = KW_CLEAR; lWorld.mbPredictHit = true; lWorld.mfPredictParam = 0.4f;
        Answer(lWorld);
        lPredictor.ProcessSceneQueryResults(KF_REAL_DT);
        Check(lPredictor.WillCollide() && lPredictor.GetTimeUntilCollision() == 0.4f
                  && lPredictor.mLineTest.GetState() == LineTestNearestPostBox::E_STATE_EMPTY,
              "P2 a hit: will collide, time = the hit's line parameter, the box emptied (0x8220E1B8)");
        gSqi.Clear(); giNearest = 0;
        lPredictor.GenerateSceneQueries(gCamera, KF_REAL_DT, gRandom, &gSqi);
        Answer(KW_CLEAR);
        lPredictor.ProcessSceneQueryResults(KF_REAL_DT);
        Check(!lPredictor.WillCollide() && lPredictor.mLineTest.GetState() == LineTestNearestPostBox::E_STATE_EMPTY,
              "P3 a miss: no collision predicted, the box emptied");
    }

    // ================= C. VisibilityCollisionPolicy + GroundConstraint + Fail =================
    SetupRig();
    {
        VisibilityCollisionPolicy& lrPolicy = FreshPolicy();
        Check(!lrPolicy.HasFailed() && lrPolicy.mbCanFail && lrPolicy.mbFirstFrame && lrPolicy.mbTargetSet
                  && !lrPolicy.mbUseGroundConstraint
                  && lrPolicy.mVelocity.x == 0.0f && lrPolicy.mVelocity.y == 0.0f && lrPolicy.mVelocity.z == 0.0f
                  && lrPolicy.mfOcclusionTimeout == 1.5f && lrPolicy.mfOffscreenTimeout == 0.5f
                  && lrPolicy.mVisibilityTest.mfMaxTimeBetweenTests == 0.5f && lrPolicy.mVisibilityTest.mbTestLookingAt
                  && !lrPolicy.mVisibilityTest.mbOccluded && lrPolicy.mVisibilityTest.mbIsOnScreen
                  && lrPolicy.mVisibilityTest.mfOccludedTime == 0.0f && lrPolicy.mVisibilityTest.mfOffscreenTime == 0.0f
                  && lrPolicy.mGroundConstraint.GetDesiredHeight() == -1.0f
                  && lrPolicy.mGroundConstraint.mLineTest.GetState() == LineTestNearestPostBox::E_STATE_EMPTY
                  && !lrPolicy.mGeometryCollisionPredictor.WillCollide()
                  && lrPolicy.mVolumeTest.GetState() == VolumeTestDeepestPostBox::E_STATE_EMPTY
                  && !lrPolicy.mVehicleCollisionPredictor.HasPredictedCollision(),
              "C1 Construct over pool garbage writes every seed (timeouts 1.5 / 0.5 flt_82004D04 / flt_82001DA0, "
              "mbCanFail, mbFirstFrame, the three tests emptied, ground height -1)");
        Check(lrPolicy.mTargetTransform.wAxis.z == 50.0f && lrPolicy.mTargetAABB.mMax.y == 1.5f
                  && static_cast<u32>(lrPolicy.mTargetEntityId) == KU_TARGET_ENTITY,
              "C2 SetTarget stores the transform, the bounds and the entity id and raises mbTargetSet");

        lrPolicy.SetVelocity(V3(3.0f, 0.0f, -4.0f));
        u64 luSeed = gRandom.muSeed;
        ConsoleRoll(luSeed); ConsoleRoll(luSeed);
        Frame(lrPolicy, KW_CLEAR);
        Check(giNearest == 3 && Classify(gaNearest[0]) == E_ASK_PREDICT && Classify(gaNearest[1]) == E_ASK_VIS_A
                  && Classify(gaNearest[2]) == E_ASK_VIS_B && gaNearest[0].muQueryId == 0x10000u
                  && gaNearest[2].muQueryId == 0x10002u && gRandom.muSeed == luSeed,
              "C3 first frame: both 20% rolls forced on -- the prediction line, then A and B; exactly two LCG draws");
        Check(!lrPolicy.HasFailed() && !lrPolicy.mbFirstFrame && FailBits() == 0
                  && lrPolicy.mVisibilityTest.mLineTestA.GetState() == LineTestNearestPostBox::E_STATE_EMPTY
                  && lrPolicy.mGeometryCollisionPredictor.mLineTest.GetState() == LineTestNearestPostBox::E_STATE_EMPTY,
              "C4 a clear first frame fails nothing, drops mbFirstFrame and leaves every box empty");
    }
    {
        VisibilityCollisionPolicy& lrPolicy = FreshPolicy();
        World lWorld = KW_CLEAR; lWorld.mbHitA = true;
        Frame(lrPolicy, lWorld);
        Check(lrPolicy.HasFailed() && FailBits() == (1u << 1) && !gCamera.mState.mCurrentFlags.IsBitSet(1u),
              "C5 first frame occluded: VISIBILITY (1) only -- the camera's account, its follow bit dropped (Fail 0x82206450)");
    }
    {
        VisibilityCollisionPolicy& lrPolicy = FreshPolicy();
        World lWorld = KW_CLEAR; lWorld.mbHitB = true;
        Frame(lrPolicy, lWorld);
        Check(FailBits() == (1u << 1), "C6 first frame occluded through B only: VISIBILITY (1)");
    }
    {
        VisibilityCollisionPolicy& lrPolicy = FreshPolicy();
        gbLookingAt = false;
        Frame(lrPolicy, KW_CLEAR);
        gbLookingAt = true;
        Check(FailBits() == ((1u << 1) | (1u << 2)), "C7 first frame off screen: VISIBILITY (1) and STARTED_OFF_SCREEN (2)");
    }
    {
        VisibilityCollisionPolicy& lrPolicy = FreshPolicy();
        World lWorld = KW_CLEAR; lWorld.mbPredictHit = true; lWorld.mfPredictParam = 0.25f;
        Frame(lrPolicy, lWorld);
        const u32 luNear = FailBits();
        VisibilityCollisionPolicy& lrSecond = FreshPolicy();
        lWorld.mfPredictParam = 1.0f;
        Frame(lrSecond, lWorld);
        Check(luNear == (1u << 9) && FailBits() == 0,
              "C8 first frame, the camera hits the world 0.25 s out: STARTED_TOO_NEAR_GEOMETRY (9); 1.0 s out is not < 1");
    }
    {
        VisibilityCollisionPolicy& lrPolicy = FreshPolicy();
        lrPolicy.SetDesiredHeight(2.0f);
        Frame(lrPolicy, KW_CLEAR);
        const NearestAsk lGround = gaNearest[0];
        Check(giNearest == 4 && Classify(lGround) == E_ASK_GROUND
                  && SameXYZ(lGround.mStart, V3(10.0f, 3.0f, 30.0f)) && SameXYZ(lGround.mEnd, V3(10.0f, -2.0f, 30.0f))
                  && lGround.mxFlags == 2u && lGround.mxVolumeFlags == 0xFFu && lGround.muExclude == 0xFFFFFFFFu
                  && lGround.meMode == 0,
              "C9 the ground line goes first: 1 m above the camera down max(5 m, height) (flt_82001C98 / flt_8200426C), world only");
        Check(gCamera.mTransform.wAxis.y == 2.5f && gCamera.mTransform.wAxis.x == 10.0f && gCamera.mTransform.wAxis.z == 30.0f
                  && FailBits() == 0,
              "C10 ground found at y 0.5: the camera's HEIGHT becomes 0.5 + 2.0, x and z untouched (0x8220E3A0)");
        VisibilityCollisionPolicy& lrTall = FreshPolicy();
        lrTall.SetDesiredHeight(8.0f);
        Frame(lrTall, KW_CLEAR);
        Check(SameXYZ(gaNearest[0].mEnd, V3(10.0f, -5.0f, 30.0f)), "C11 a desired height above 5 m lengthens the ground line to that height");
    }
    {
        VisibilityCollisionPolicy& lrPolicy = FreshPolicy();
        lrPolicy.SetDesiredHeight(2.0f);
        World lWorld = KW_CLEAR; lWorld.mbGroundHit = false;
        Frame(lrPolicy, lWorld);
        Check(FailBits() == (1u << 3), "C12 no ground under the camera: COULDNT_FIND_GROUND (3)");

        VisibilityCollisionPolicy& lrNoFail = FreshPolicy();
        lrNoFail.SetCanFail(false);
        lrNoFail.SetDesiredHeight(2.0f);
        lWorld.mbHitA = true;
        gbLookingAt = false;
        Frame(lrNoFail, lWorld);
        gbLookingAt = true;
        Check(FailBits() == (1u << 3), "C13 mbCanFail off: only the ground failure survives (every other arm tests `lbz 8`)");
    }
    {
        VisibilityCollisionPolicy& lrPolicy = FreshPolicy();
        lrPolicy.SetDesiredHeight(0.05f);
        const int liBefore = giAsserts;
        Frame(lrPolicy, KW_CLEAR);
        Check(giAsserts > liBefore && std::strstr(gacLastAssert, "conflict with sphere test") != 0,
              "C14 a ground height at or under the sphere test's 0.1 m (flt_82004014) trips BrnCollisionPolicy.cpp:845");
    }
    // ---- later frames: the timeouts, against the console's roll sequence ----
    {
        VisibilityCollisionPolicy& lrPolicy = FreshPolicy();
        Frame(lrPolicy, KW_CLEAR);                                        // first frame: clear
        World lOccluded = KW_CLEAR; lOccluded.mbHitA = true;
        bool lbOccluded = false;
        f32  lfOccluded = 0.0f;
        int  liExpected = -1, liActual = -1, liAskMismatches = 0;
        bool lbExpect13 = false;
        for (int liFrame = 2; liFrame < 800 && liActual < 0; ++liFrame)
        {
            u64 luSeed = gRandom.muSeed;
            const bool lbVisibility = ConsoleRoll(luSeed);
            const bool lbPrediction = ConsoleRoll(luSeed);
            Frame(lrPolicy, lOccluded);
            if (giNearest != (lbVisibility ? 2 : 0) + (lbPrediction ? 1 : 0))
                ++liAskMismatches;
            if (lbVisibility) lbOccluded = true;
            if (lbOccluded) lfOccluded += KF_REAL_DT;
            if (liExpected < 0 && lbVisibility && lfOccluded >= 1.5f)
            {
                liExpected = liFrame;
                lbExpect13 = (lfOccluded > 1.5f);
            }
            if (lrPolicy.HasFailed())
                liActual = liFrame;
        }
        std::printf("occlusion timeout: expected frame %d, failed on frame %d, bits 0x%X, ask mismatches %d\n",
                    liExpected, liActual, FailBits(), liAskMismatches);
        Check(liAskMismatches == 0, "C15 every later frame asks exactly what the console's two rolls say (A+B iff roll 1, the prediction iff roll 2)");
        Check(liExpected > 0 && liActual == liExpected
                  && FailBits() == ((1u << 1) | (lbExpect13 ? (1u << 13) : 0u)),
              "C16 occluded for 1.5 s of E_WORLD_NO_SLOMO time: SUBJECT_OCCLUDED (13) + VISIBILITY (1) on the first rolled frame after");
    }
    {
        VisibilityCollisionPolicy& lrPolicy = FreshPolicy();
        Frame(lrPolicy, KW_CLEAR);
        gbLookingAt = false;
        bool lbOnScreen = true;
        f32  lfOffscreen = 0.0f;
        int  liExpected = -1, liActual = -1;
        bool lbExpect12 = false;
        for (int liFrame = 2; liFrame < 800 && liActual < 0; ++liFrame)
        {
            u64 luSeed = gRandom.muSeed;
            const bool lbVisibility = ConsoleRoll(luSeed);
            Frame(lrPolicy, KW_CLEAR);
            if (lbVisibility) lbOnScreen = false;
            if (!lbOnScreen) lfOffscreen += KF_REAL_DT;
            if (liExpected < 0 && lbVisibility && lfOffscreen >= 0.5f)
            {
                liExpected = liFrame;
                lbExpect12 = (lfOffscreen > 0.5f);
            }
            if (lrPolicy.HasFailed())
                liActual = liFrame;
        }
        gbLookingAt = true;
        std::printf("off-screen timeout: expected frame %d, failed on frame %d, bits 0x%X\n", liExpected, liActual, FailBits());
        Check(liExpected > 0 && liActual == liExpected
                  && FailBits() == ((1u << 1) | (lbExpect12 ? (1u << 12) : 0u)),
              "C17 off screen for 0.5 s: SUBJECT_LEFT_FRAME (12) + VISIBILITY (1) on the first rolled frame after");
    }
    // ---- the ground constraint's own height tripwires, by polarity (FX-GATE NaN sweep, 2026-09-25) ----
    // GenerateSceneQueries 0x82240224 / ProcessSceneQueryResults 0x8220E3C4: `fcmpu cr6, height, 0.0f
    // (flt_82001CC0) ; bge <past the assert>`. bge ("not less than") is taken for an unordered compare, so only
    // a height BELOW 0 fires "mfDesiredHeight >= 0.0f" -- NaN and -0.0 skip it. (The policy setter's :489 and
    // the sphere-test :845 are `bgt`-skips and do fire on NaN; they are not counted here.)
    {
        const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();
        VisibilityCollisionPolicy& lrPolicy = FreshPolicy();
        lrPolicy.SetDesiredHeight(KF_NAN);
        giGroundHeightAsserts = 0;
        Frame(lrPolicy, KW_CLEAR);
        Check(giGroundHeightAsserts == 0 && giNearest >= 1 && Classify(gaNearest[0]) == E_ASK_GROUND,
              "C18 a NaN ground height asks its ground line and fires neither :719 nor :738 (bge is taken on unordered)");

        VisibilityCollisionPolicy& lrNegative = FreshPolicy();
        lrNegative.SetDesiredHeight(-1.0f);
        giGroundHeightAsserts = 0;
        Frame(lrNegative, KW_CLEAR);
        Check(giGroundHeightAsserts == 2, "C19 a height of -1 fires both :719 (Generate) and :738 (Process)");

        VisibilityCollisionPolicy& lrNegativeZero = FreshPolicy();
        lrNegativeZero.SetDesiredHeight(-0.0f);
        giGroundHeightAsserts = 0;
        Frame(lrNegativeZero, KW_CLEAR);
        Check(giGroundHeightAsserts == 0, "C20 a height of -0.0 compares equal to 0.0: neither tripwire fires");
    }
    // ---- the camera-in-geometry sphere test (0x82240528..0x82240574; the G3 gate retired 2026-09-25) ----
    {
        VisibilityCollisionPolicy& lrPolicy = FreshPolicy();
        Frame(lrPolicy, KW_CLEAR);
        const rw::collision::Volume& lrSphere = *reinterpret_cast<const rw::collision::Volume*>(gDeepest.macVolume);
        const rw::math::vpu::Matrix44Affine& lrAsked =
            *reinterpret_cast<const rw::math::vpu::Matrix44Affine*>(gDeepest.macTransform);
        Check(giDeepest == 1 && gDeepest.muQueryId == 0x10000u && gDeepest.mxFlags == 0x1Eu
                  && gDeepest.mxVolumeFlags == 0xFFu && gDeepest.mpTransform == &gCamera.mTransform
                  && SameXYZ(lrAsked.wAxis, gCamera.mTransform.wAxis)
                  && gDeepest.muExclude == 0xFFFFFFFFu && gDeepest.meMode == 0,
              "C21 every mbCanFail frame asks ONE VolumeTestDeepest: flags 30, volume flags 0xFF, the camera's own "
              "transform, no excluded entity (0xFFFFFFFF @0x82CDA790), E_EXCLUDE_ENTITY_ONLY (0x82240574)");
        Check(lrSphere.muVTableSlot == static_cast<u32>(rw::collision::E_VOLUMETYPE_SPHERE)
                  && lrSphere.mfRadius == 0.1f
                  && lrSphere.maTransform[0].x == 1.0f && lrSphere.maTransform[1].y == 1.0f
                  && lrSphere.maTransform[2].z == 1.0f && lrSphere.maTransform[3].x == 0.0f
                  && lrSphere.maTransform[3].y == 0.0f && lrSphere.maTransform[3].z == 0.0f,
              "C22 the volume is a sphere of radius 0.1 (flt_82004014) at the origin of the camera's frame "
              "(SphereVolume::Initialize @0x82BA84E8)");
        Check(FailBits() == 0 && lrPolicy.mVolumeTest.GetState() == VolumeTestDeepestPostBox::E_STATE_EMPTY,
              "C23 a sphere in open air fails nothing, and the box is emptied at the end (stw 0, 0x1B0)");

        VisibilityCollisionPolicy& lrInside = FreshPolicy();
        World lInside = KW_CLEAR; lInside.mbInsideGeometry = true;
        Frame(lrInside, lInside);
        Check(lrInside.HasFailed() && FailBits() == (1u << 0)
                  && lrInside.mVolumeTest.GetState() == VolumeTestDeepestPostBox::E_STATE_EMPTY,
              "C24 the sphere inside geometry: COLLISION (0) (0x822246FC), the box emptied");

        VisibilityCollisionPolicy& lrNoFail = FreshPolicy();
        lrNoFail.SetCanFail(false);
        Frame(lrNoFail, lInside);
        Check(giDeepest == 0 && FailBits() == 0,
              "C25 mbCanFail off: the sphere is never asked for (`lbz 8 ; beq` @0x822404B8)");
    }

    std::printf("fxdirector2_scene_query: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
