// FX-RCEM4 (crash parity 2026-09-24, reviewer B on 87d1ad23 / G62): ActiveRaceCar::OnResourcesLoaded
// @0x822EB168 -- the legs that read the car's streamed deformation spec through BrnPhysics::Def
// (@0x822C7708). run_fxrcem4_on_resources_loaded.py extracts VERBATIM:
//   ActiveRaceCar::OnResourcesLoaded                     (BrnActiveRaceCar.cpp)
//   ResolveDeformationSpec -- this TU's spelling of Def over the handle the +0x1C90 wrapper keeps
//   ActiveRaceCar::RenderParams::SetWheelScale @0x822CD170 (BrnActiveRaceCarRenderParams.cpp)
//   StreamedDeformationSpec::GetWheelSpec @0x822A0328    (BrnStreamedDeformationSpec.cpp)
// and replays them on a fixture car against the REAL StreamedDeformationSpec layout
// (BrnStreamedDeformationSpec.h pins the +1552 matrix and maWheelSpecs at +80, stride 48) and the
// real rw::math::vpu::IsValid.
//   0x822EB20C..0x822EB24C  leg 1: mr r3, r24 (this + 0x1C90) ; bl Def ; addi r10, r3, 0x610 ;
//                           four lvx128 / stvx128 pairs -> this + 0x90 (w lanes included)
//   0x822EB250..0x822EB3FC  per-row x/y/z self-equality cascade ; ONE assert
//                           "RwMath::IsValid( mCentreOfMassTransform )" (0x8201D720, :831)
//   0x822EB404              ResetVerletOffsets -- after the copy
//   0x822EB410..0x822EB470  leg 2: per wheel i < 4 (r30 = 0x60 + 0x30*i < 0x120): bl Def (again,
//                           @0x822EB42C) ; the inlined GetWheelSpec bound ; lvx128 v1 = spec + 0x60 +
//                           0x30*i ; SetWheelScale(this + 0x7E0, i, v1) -- after the queue Construct
//                           (0x822EB40C), before the colour leg (0x822EB474)
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

static std::vector<std::string> gaAsserts;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) {
    gaAsserts.push_back(lpcMessage ? lpcMessage : "");
    std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); return 0;
}
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

// The real checked accessor (asserts liWheel < 4, returns &maWheelSpecs[liWheel]).
namespace BrnPhysics { namespace Deformation {
#include "fxrcem4_orl_getwheelspec.inc"
} }

namespace Fixture {
namespace CgsResource { struct ResourceHandle { void* mpResourceMemory; void* mpSourceEntry; }; }

struct ActiveRaceCar;
static const ActiveRaceCar* gpCarUnderTest = nullptr;
static bool AnyWheelScaleSet(const ActiveRaceCar* lpCar);
static bool gbScaledBeforeQueue = false, gbScaledBeforeColour = false;

// ---- the colour leg's AttribSys stand-ins (the leg itself is run_fxrcem4_recolour.py's) ------------
namespace Attrib {
struct Collection {};
namespace Gen {
struct burnoutcarasset {
    struct RefSpec { const Collection* GetCollection() { return nullptr; } } mRef;
    burnoutcarasset(u64, void*) { gbScaledBeforeColour = AnyWheelScaleSet(gpCarUnderTest); }
    RefSpec* GetGraphicsAssetRefSpec() const { return const_cast<RefSpec*>(&mRef); }
};
struct burnoutcargraphicsasset {
    burnoutcargraphicsasset(Collection*, void*) {}
    const s32& PlayerColourIndex() const { static const s32 kiColour = 13; return kiColour; }
    const s32& PlayerColourPaletteIndex() const { static const s32 kiPalette = 2; return kiPalette; }
};
}
}

#include "fxrcem4_orl_resolve.inc"

struct DetachedPartQueue { int miConstructs = 0; void Construct() { ++miConstructs; gbScaledBeforeQueue = AnyWheelScaleSet(gpCarUnderTest); } };

struct ActiveRaceCar {
    enum EState : u32 { E_STATE_INACTIVE = 0, E_STATE_ATTACHED = 1, E_STATE_WAITING = 2, E_STATE_ACTIVE = 3 };
    struct RenderParams {
        Matrix44Affine    mWheelScaleTransforms[6];                            // +0x9C0
        DetachedPartQueue mQueue;
        DetachedPartQueue& GetDetachedPartQueue() { return mQueue; }
        Matrix44Affine& GetWheelScaleMatrix(u32 luWheel) { return mWheelScaleTransforms[luWheel]; }
        void SetWheelScale(u32 luWheel, const Vector3& lrScale);
    };
    u32                          muState = E_STATE_ATTACHED;
    EActiveRaceCarIndex          meActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_2;
    Matrix44Affine               mCentreOfMassTransform;                  // +0x90
    CgsResource::ResourceHandle  mDeformationModelHandle = { nullptr, nullptr };
    CgsResource::ResourceHandle  mGraphicsModelHandle = { nullptr, nullptr };
    RenderParams                 mRenderParams;
    s32                          miDefaultColourIndex = -1, miDefaultColourPalette = -1;
    int                          miVerletResets = 0;
    Matrix44Affine               mComAtVerletReset;
    ActiveRaceCar() {
        // What Prepare / Attach leave: the zero-wAxis identity (MakeIdentityTransform), and
        // RenderParams::Reset's identity wheel scales -- stamped here with a 7.0 sentinel instead so a
        // written slot is told apart from an untouched one.
        mCentreOfMassTransform.xAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
        mCentreOfMassTransform.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
        mCentreOfMassTransform.zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
        mCentreOfMassTransform.wAxis = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        mComAtVerletReset = mCentreOfMassTransform;
        for (Matrix44Affine& lrM : mRenderParams.mWheelScaleTransforms)
            std::memset(&lrM, 0, sizeof lrM), lrM.xAxis.x = lrM.yAxis.y = lrM.zAxis.z = lrM.wAxis.w = 7.0f;
    }
    bool IsAttached() const { return muState != E_STATE_INACTIVE; }
    bool IsActive() const { return muState == E_STATE_ACTIVE; }
    void ResetVerletOffsets() { ++miVerletResets; mComAtVerletReset = mCentreOfMassTransform; }
    void OnResourcesLoaded(const CgsResource::ResourceHandle& lrDeformationModelHandle,
                           const CgsResource::ResourceHandle& lrGraphicsModelHandle,
                           const Vector3& lrInitialVelocity, u64 luCarAssetAttribKey);
};
static bool AnyWheelScaleSet(const ActiveRaceCar* lpCar) {
    if (lpCar == nullptr) return false;
    for (int i = 0; i < 4; ++i) if (lpCar->mRenderParams.mWheelScaleTransforms[i].wAxis.w != 7.0f) return true;
    return false;
}
#include "fxrcem4_orl_setwheelscale.inc"
#include "fxrcem4_orl.inc"
}   // namespace Fixture

typedef BrnPhysics::Deformation::StreamedDeformationSpec Spec;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static int CountAsserts(const char* lpcPrefix) {
    int liCount = 0;
    for (const std::string& lrMessage : gaAsserts)
        if (lrMessage.compare(0, std::strlen(lpcPrefix), lpcPrefix) == 0) ++liCount;
    return liCount;
}
static bool SameBytes(const Matrix44Affine& lrA, const Matrix44Affine& lrB) {
    return std::memcmp(&lrA, &lrB, sizeof(Matrix44Affine)) == 0;
}

// The streamed spec lives in resource memory; the handle's mpResourceMemory points at the slot that
// holds the spec pointer (the double deref ResolveDeformationSpec performs).
struct ResidentSpec {
    Spec* mpSpec;
    const Spec* mpSlot;
    ResidentSpec() : mpSpec(new Spec()), mpSlot(nullptr) { std::memset(mpSpec, 0, sizeof(Spec)); mpSlot = mpSpec; }
    ~ResidentSpec() { delete mpSpec; }
    Fixture::CgsResource::ResourceHandle Handle() { return { &mpSlot, nullptr }; }
};

// PUSMC01's shipped +1552 matrix (identity rotation, translation (0, -0.740575, +0.170226)), with
// distinct w lanes so a lane-wise (x/y/z-only) copy is told apart from the console's whole-vector one.
static Matrix44Affine AuthoredCom() {
    Matrix44Affine lM;
    lM.xAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.5f };
    lM.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.25f };
    lM.zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.125f };
    lM.wAxis = Vector3{ 0.0f, -0.740575f, 0.170226f, 1.0f };
    return lM;
}
// Four distinct authored wheel scales (wheel 1 is PUSMC01's 0.662665 tyre), and positions that must
// NOT be what lands in the scale matrix (spec + 0x50 + 0x30*i is the position, + 0x60 the scale).
static void AuthorWheels(Spec* lpSpec) {
    for (int i = 0; i < 4; ++i) {
        lpSpec->maWheelSpecs[i].mPosition = Vector3{ 10.0f + i, 20.0f + i, 30.0f + i, 40.0f };
        lpSpec->maWheelSpecs[i].mScale    = Vector3{ 0.25f + 0.1f * i, 0.662665f + 0.01f * i, 0.5f - 0.05f * i, 9.0f };
    }
}

int main() {
    using namespace Fixture;
    const Vector3 lZero = { 0.0f, 0.0f, 0.0f, 0.0f };
    const f32 lfNan = std::numeric_limits<f32>::quiet_NaN();

    // ---- leg 1, a resident spec: the +1552 matrix is copied whole, before ResetVerletOffsets ----------
    {
        ResidentSpec lRes; lRes.mpSpec->mCarModelSpaceToHandlingBodySpaceTransform = AuthoredCom();
        AuthorWheels(lRes.mpSpec);
        ActiveRaceCar lCar; gpCarUnderTest = &lCar; gaAsserts.clear();
        lCar.OnResourcesLoaded(lRes.Handle(), { nullptr, nullptr }, lZero, 0x1234ull);
        Check(SameBytes(lCar.mCentreOfMassTransform, AuthoredCom()),
              "0x822EB20C: mCentreOfMassTransform (+0x90) = Def(+0x1C90) + 0x610, all 64 bytes (lvx128/stvx128 x4, w lanes too)");
        Check(lCar.miVerletResets == 1 && SameBytes(lCar.mComAtVerletReset, AuthoredCom()),
              "the copy precedes ResetVerletOffsets (0x822EB24C < 0x822EB404)");
        Check(lCar.muState == ActiveRaceCar::E_STATE_WAITING && lCar.mRenderParams.mQueue.miConstructs == 1
                  && lCar.miDefaultColourIndex == 13 && lCar.miDefaultColourPalette == 2,
              "the rest of the body is unchanged (state WAITING, queue Construct, colour leg)");
        Check(gaAsserts.empty(), "a valid authored spec raises no assert");

        // ---- leg 2 on the same load: the four wheel scale matrices ------------------------------------
        bool lbScales = true;
        for (int i = 0; i < 4; ++i) {
            const Vector3& lrS = lRes.mpSpec->maWheelSpecs[i].mScale;
            const Matrix44Affine& lrM = lCar.mRenderParams.mWheelScaleTransforms[i];
            lbScales = lbScales
                && lrM.xAxis.x == lrS.x && lrM.xAxis.y == 0.0f && lrM.xAxis.z == 0.0f && lrM.xAxis.w == 0.0f
                && lrM.yAxis.x == 0.0f && lrM.yAxis.y == lrS.y && lrM.yAxis.z == 0.0f && lrM.yAxis.w == 0.0f
                && lrM.zAxis.x == 0.0f && lrM.zAxis.y == 0.0f && lrM.zAxis.z == lrS.z && lrM.zAxis.w == 0.0f
                && lrM.wAxis.x == 0.0f && lrM.wAxis.y == 0.0f && lrM.wAxis.z == 0.0f && lrM.wAxis.w == 0.0f;
        }
        Check(lbScales, "0x822EB410: mWheelScaleTransforms[i] = diag(maWheelSpecs[i].mScale) (lvx128 spec + 0x60 + 0x30*i), i = 0..3");
        Check(lCar.mRenderParams.mWheelScaleTransforms[4].wAxis.w == 7.0f && lCar.mRenderParams.mWheelScaleTransforms[5].wAxis.w == 7.0f,
              "only four wheels (the loop ends at r30 == 0x120): slots 4 and 5 untouched");
        Check(!gbScaledBeforeQueue && gbScaledBeforeColour,
              "the wheel loop runs after the queue Construct (0x822EB40C) and before the colour leg (0x822EB474)");
    }
    // ---- leg 1: a NaN in an x/y/z lane -- copied, then ONE IsValid assert (:831) -------------------------
    {
        ResidentSpec lRes; Matrix44Affine lBad = AuthoredCom(); lBad.yAxis.z = lfNan;
        lRes.mpSpec->mCarModelSpaceToHandlingBodySpaceTransform = lBad; AuthorWheels(lRes.mpSpec);
        ActiveRaceCar lCar; gpCarUnderTest = &lCar; gaAsserts.clear();
        lCar.OnResourcesLoaded(lRes.Handle(), { nullptr, nullptr }, lZero, 0x1234ull);
        Check(CountAsserts("RwMath::IsValid( mCentreOfMassTransform )") == 1 && gaAsserts.size() == 1,
              "a NaN in yAxis.z: exactly one \"RwMath::IsValid( mCentreOfMassTransform )\" assert (0x8201D720, line 0x33F)");
        Check(lCar.mCentreOfMassTransform.yAxis.z != lCar.mCentreOfMassTransform.yAxis.z,
              "the copy happens before the check (the NaN is stored, then asserted on)");
    }
    // ---- leg 1: a NaN in a w lane only -- no assert (the cascade tests lanes 0..2 of each row) -----------
    {
        ResidentSpec lRes; Matrix44Affine lW = AuthoredCom(); lW.xAxis.w = lfNan;
        lRes.mpSpec->mCarModelSpaceToHandlingBodySpaceTransform = lW; AuthorWheels(lRes.mpSpec);
        ActiveRaceCar lCar; gpCarUnderTest = &lCar; gaAsserts.clear();
        lCar.OnResourcesLoaded(lRes.Handle(), { nullptr, nullptr }, lZero, 0x1234ull);
        Check(gaAsserts.empty() && lCar.mCentreOfMassTransform.xAxis.w != lCar.mCentreOfMassTransform.xAxis.w
                  && lCar.mCentreOfMassTransform.wAxis.y == -0.740575f,
              "a NaN only in xAxis.w is copied and NOT asserted on (vspltw 0/1/2 only)");
    }
    // ---- no resident spec: Def's own assert on every Def call, nothing written (PC-safety guard) ---------
    {
        ActiveRaceCar lCar; gpCarUnderTest = &lCar; const Matrix44Affine lBefore = lCar.mCentreOfMassTransform; gaAsserts.clear();
        lCar.OnResourcesLoaded({ nullptr, nullptr }, { nullptr, nullptr }, lZero, 0x1234ull);
        Check(CountAsserts("Can not instance resource pointer") == 5,
              "an unresolved spec fires Def's \"Can not instance resource pointer\" assert once per Def call: leg 1 + 4 wheel passes");
        Check(SameBytes(lCar.mCentreOfMassTransform, lBefore) && lCar.muState == ActiveRaceCar::E_STATE_WAITING
                  && lCar.miDefaultColourIndex == 13,
              "an unresolved spec leaves the identity and the rest of the body still runs");
        Check(!AnyWheelScaleSet(&lCar), "an unresolved spec writes no wheel scale");
    }

    std::printf("FxRcem4OnResourcesLoaded: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
