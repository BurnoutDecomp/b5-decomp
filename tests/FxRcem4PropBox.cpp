// FX-RCEM4 (crash parity 2026-09-24, CC-3 = G60-D3): ActiveRaceCar::GetPropCollisionBox (ARTIST
// 0x822D3DB0) and RaceCarEntityModule::UpdatePropBoundingBoxes_PreScene (0x822F5668), both extracted
// VERBATIM by run_fxrcem4_prop_box.py (a missing body is replayed as a stand-in that does nothing).
//   GetPropCollisionBox:
//     centre = (mMin + mMax) * 0.5 ; half = mMax - centre ; pos = centre + mComOffset (every lane)
//     h = (wAxis.y + centre.y) - half.y ; e = vmaxfp(h, 0) + 0.05 (unk_82FAD550) ; half.y += e/2 ; pos.y -= e/2
//     BoxVolume::Initialize({lpVolumeBuffer, 0, 0, 0, 0}, half) ; stvx128 pos -> volume + 0x30 ; return volume
//   UpdatePropBoundingBoxes_PreScene, per slot 0..7:
//     scene = lpOutput->GetSceneInputInterface() ; car = GetActiveRaceCar(slot)
//     if (lbz 0x78A && lbz 0x535) ReplaceDynamicVolume(scene, ld 0xD0 (ALL 64 bits), GetPropCollisionBox(car, stack))
// The scene interface is a recorder with BOTH producer forms: the 32-bit EntityId one must never be hit.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"
#include "vendor/renderware/collision/CollisionVolume.hpp"
#include "rw/rwcore_structs.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

static unsigned guAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAssertions; return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

// ---- the vendor producer, as a recorder (its body is BoxVolume.cpp's; only the contract matters) ----
static const void* gpInitializeBlock = nullptr;
static rw::collision::Vec4 gInitializeHalf = {};
namespace rw { namespace collision {
BoxVolume::BoxVolume(const Vec4& arHalfDimensions)
{
    // BoxVolume::BoxVolume @0x82BAA0F0: identity rows, ZERO translation, the half-extents.
    const Vec4 laRows[4] = { {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 0} };
    for (int i = 0; i < 4; ++i) maTransform[i] = laRows[i];
    mBoxData.mfHx = arHalfDimensions.x; mBoxData.mfHy = arHalfDimensions.y; mBoxData.mfHz = arHalfDimensions.z;
}
BoxVolume* BoxVolume::Initialize(const ::rw::Resource& arResource, const Vec4& arHalfDimensions)
{
    gpInitializeBlock = arResource.m_baseResources[0];
    gInitializeHalf = arHalfDimensions;
    return arResource.m_baseResources[0] ? new (arResource.m_baseResources[0]) BoxVolume(arHalfDimensions) : nullptr;
}
} }

namespace BrnWorld {
namespace { const f32 KF_HANDLING_BODY_Y_PAD = 0.05f; }   // unk_82FAD550 (CRT splat of flt_8201497C)

struct Box { Vector4 mMin, mMax; };
struct PhysicsState { Vector3 mComOffset; bool mbDeformedThisFrame = false; };
struct VolumeInstanceId { u64 muId = 0; };
class RaceCarEntityModule;

class ActiveRaceCar {
public:
    Box mDeformedBBox = {};
    PhysicsState mPhysicsState = {};
    Matrix44Affine mCentreOfMassTransform = {};
    VolumeInstanceId mHandlingBodyVolumeId;
    bool mbAddedToScene = false;
    rw::collision::BoxVolume* GetPropCollisionBox(void* lpVolumeBuffer);
};
#include "fxrcem4_pb_car.inc"

// The scene producer, both forms (CgsSceneManagerIO_SceneUpdate.h:241 and the DWARF :458 form).
struct Replace { u64 muKey; bool mbWide; u8 maImage[128]; };
struct FakeScene {
    std::vector<Replace> mCalls;
    void ReplaceDynamicVolume(u32 luEntityId, const void* lpVolume) { Record(luEntityId, false, lpVolume); }
    void ReplaceDynamicVolume(CgsSceneManager::VolumeId lVolumeId, const void* lpVolume) { Record(lVolumeId.mId, true, lpVolume); }
    void Record(u64 luKey, bool lbWide, const void* lpVolume) {
        Replace r; r.muKey = luKey; r.mbWide = lbWide; std::memcpy(r.maImage, lpVolume, 128); mCalls.push_back(r);
    }
};
namespace RaceCarEntityModuleIO {
struct OutputBuffer_PreScene {
    typedef FakeScene SceneInputInterface;
    FakeScene mScene; int miFetches = 0;
    SceneInputInterface* GetSceneInputInterface() { ++miFetches; return &mScene; }
};
}
class RaceCarEntityModule {
public:
    ActiveRaceCar maCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leIndex) { return &maCars[leIndex]; }
    void UpdatePropBoundingBoxes_PreScene(RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput);
};
#include "fxrcem4_pb_module.inc"
}   // namespace BrnWorld

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static bool Near(f32 a, f32 b) { return a == b || std::fabs(a - b) <= 1e-6f; }

#include "fxrcem4_pb_struct.inc"   // static const bool gbStructuralOk (computed by the runner)

// The hand evaluation of 0x822D3DB0, lane by lane.
struct Expect { f32 half[4]; f32 pos[4]; };
static Expect Evaluate(const Vector4& mn, const Vector4& mx, const Vector3& com, f32 lfComY) {
    Expect e;
    const f32 c[4] = { (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f, (mn.w + mx.w) * 0.5f };
    const f32 m[4] = { mx.x, mx.y, mx.z, mx.w }; const f32 o[4] = { com.x, com.y, com.z, com.w };
    for (int i = 0; i < 4; ++i) { e.half[i] = m[i] - c[i]; e.pos[i] = c[i] + o[i]; }
    const f32 h = (lfComY + c[1]) - e.half[1];
    const f32 lfMax = (h != h) ? h : (h > 0.0f ? h : 0.0f);   // vmaxfp: NaN stays NaN
    const f32 d = (lfMax + 0.05f) * 0.5f;
    e.half[1] += d; e.pos[1] -= d;
    return e;
}

int main() {
    using namespace BrnWorld;
    // ---- GetPropCollisionBox ------------------------------------------------------------------------
    {
        ActiveRaceCar car;
        car.mDeformedBBox.mMin = Vector4{ -0.9f, -0.3f, -2.1f, 0.25f };
        car.mDeformedBBox.mMax = Vector4{ 0.95f, 0.9f, 2.3f, 0.75f };
        car.mPhysicsState.mComOffset = Vector3{ 0.01f, -0.1f, 0.2f, 0.5f };
        car.mCentreOfMassTransform.wAxis = Vector3{ 0.0f, 0.5f, 0.0f, 1.0f };
        alignas(16) u8 laBlock[128] = {};
        gpInitializeBlock = nullptr;
        rw::collision::BoxVolume* lpVolume = car.GetPropCollisionBox(laBlock);
        const Expect e = Evaluate(car.mDeformedBBox.mMin, car.mDeformedBBox.mMax, car.mPhysicsState.mComOffset, 0.5f);
        Check(lpVolume == reinterpret_cast<rw::collision::BoxVolume*>(laBlock) && gpInitializeBlock == laBlock,
              "built in the caller's block through BoxVolume::Initialize (resource word 0) and returned");
        Check(lpVolume && Near(gInitializeHalf.x, e.half[0]) && Near(gInitializeHalf.z, e.half[2])
              && Near(gInitializeHalf.w, e.half[3]),
              "half-extents x/z/w = mMax - (mMin + mMax) * 0.5, untouched by the drop");
        // h = 0.5 + 0.3 - 0.6 = 0.2 > 0 -> e = 0.25 -> the box grows DOWN by 0.25 (0.125 each side)
        Check(lpVolume && Near(gInitializeHalf.y, 0.6f + 0.125f),
              "half.y grows by (max(h, 0) + 0.05) / 2 with h = wAxis.y + centre.y - half.y");
        Check(lpVolume && Near(lpVolume->maTransform[3].x, e.pos[0]) && Near(lpVolume->maTransform[3].z, e.pos[2])
              && Near(lpVolume->maTransform[3].w, e.pos[3]),
              "translation row x/z/w = centre + mComOffset (all four lanes stored, stvx128 +0x30)");
        Check(lpVolume && Near(lpVolume->maTransform[3].y, 0.3f - 0.1f - 0.125f),
              "translation y = centre.y + com.y - the same half drop");
        Check(lpVolume && lpVolume->maTransform[0].x == 1.0f && lpVolume->maTransform[1].y == 1.0f
              && lpVolume->maTransform[2].z == 1.0f,
              "the rotation rows stay the ctor's identity");

        // A box whose bottom sits below the COM height: h < 0 -> only the 0.05 pad.
        car.mCentreOfMassTransform.wAxis = Vector3{ 0.0f, -0.2f, 0.0f, 1.0f };
        lpVolume = car.GetPropCollisionBox(laBlock);
        Check(lpVolume && Near(gInitializeHalf.y, 0.6f + 0.025f) && Near(lpVolume->maTransform[3].y, 0.3f - 0.1f - 0.025f),
              "h < 0 clamps to 0 (vmaxfp): only the 0.05 pad (0.025 each side)");

        // A NaN height stays NaN through vmaxfp (AltiVec PEM) -- not the `(x > 0) ? x : 0` zero.
        car.mCentreOfMassTransform.wAxis = Vector3{ 0.0f, std::numeric_limits<f32>::quiet_NaN(), 0.0f, 1.0f };
        lpVolume = car.GetPropCollisionBox(laBlock);
        Check(lpVolume && std::isnan(gInitializeHalf.y) && std::isnan(lpVolume->maTransform[3].y)
              && Near(gInitializeHalf.x, e.half[0]),
              "a NaN COM height gives a NaN y lane (vmaxfp propagates), x lane unaffected");
    }

    // ---- UpdatePropBoundingBoxes_PreScene ------------------------------------------------------------
    {
        RaceCarEntityModule module;
        for (int i = 0; i < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++i) {
            ActiveRaceCar& c = module.maCars[i];
            c.mDeformedBBox.mMin = Vector4{ -1.0f, -0.5f, -2.0f, 0.0f };
            c.mDeformedBBox.mMax = Vector4{ 1.0f, 0.5f + 0.1f * i, 2.0f, 0.0f };
            c.mPhysicsState.mComOffset = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
            c.mCentreOfMassTransform.wAxis = Vector3{ 0.0f, 0.3f, 0.0f, 1.0f };
            c.mHandlingBodyVolumeId.muId = (static_cast<u64>(0x01000000u | i) << 32);   // low dword 0, as Attach seeds it
        }
        module.maCars[1].mbAddedToScene = true; module.maCars[1].mPhysicsState.mbDeformedThisFrame = true;
        module.maCars[4].mbAddedToScene = true; module.maCars[4].mPhysicsState.mbDeformedThisFrame = true;
        module.maCars[2].mbAddedToScene = true;                                    // not deformed this frame
        module.maCars[5].mPhysicsState.mbDeformedThisFrame = true;                 // not in the scene
        RaceCarEntityModuleIO::OutputBuffer_PreScene out;
        module.UpdatePropBoundingBoxes_PreScene(&out);
        const std::vector<Replace>& calls = out.mScene.mCalls;
        Check(calls.size() == 2, "exactly the two cars in the scene AND deformed this frame are replaced");
        Check(calls.size() == 2 && calls[0].mbWide && calls[1].mbWide,
              "through the 64-bit VolumeId form (never the 32-bit EntityId one)");
        Check(calls.size() == 2 && calls[0].muKey == (static_cast<u64>(0x01000001u) << 32)
              && calls[1].muKey == (static_cast<u64>(0x01000004u) << 32),
              "keyed by the WHOLE mHandlingBodyVolumeId (ld 0xD0), slot order");
        {
            const rw::collision::BoxVolume* lpImage =
                calls.size() == 2 ? reinterpret_cast<const rw::collision::BoxVolume*>(calls[1].maImage) : nullptr;
            const Expect e = Evaluate(module.maCars[4].mDeformedBBox.mMin, module.maCars[4].mDeformedBBox.mMax,
                                      module.maCars[4].mPhysicsState.mComOffset, 0.3f);
            Check(lpImage && Near(lpImage->mBoxData.mfHy, e.half[1]) && Near(lpImage->maTransform[3].y, e.pos[1])
                  && Near(lpImage->mBoxData.mfHx, e.half[0]),
                  "each post carries that car's GetPropCollisionBox image (slot 4's deformed box)");
        }
        Check(out.miFetches == E_ACTIVE_RACE_CAR_INDEX_COUNT, "the scene interface is fetched once per slot (bl 0x822B4F78 in the loop)");
    }

    Check(gbStructuralOk, "PreSceneUpdate calls it right after WriteUpdatedAIData, before UpdateOutputInterfaces (0x8230E408)");
    Check(guAssertions == 0, "no assertions");

    std::printf("FxRcem4PropBox: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
