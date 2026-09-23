// Harness for run_predict_car_car.py. The production PredictCarCarIntersection text (its helper
// block and the two member bodies) is pasted in through extracted.inc, over test doubles of the
// body and the manager, and linked against the REAL rw::collision narrow phase (BoxVolume,
// GPInstance, PrimitivePairIntersect) -- so the verdicts below are the shipped code's.
#include "types.hpp"
#include "BrnCommonTypes.h"                                            // Vector3, Matrix44Affine
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "vendor/renderware/collision/CollisionVolume.hpp"
#include "vendor/renderware/collision/GPInstance.hpp"
#include "rw/rwcore_structs.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

static int g_iAsserts = 0;
int TestInitializeVolumeVTable();   // run_predict_car_car.py's shim over Volume::InitializeVTable
#undef CGS_ASSERT
#define CGS_ASSERT(c, m) do { if (!(c)) { ++g_iAsserts; std::printf("ASSERT: %s\n", m); } } while (0)

// Pre-fix text logs through the debug print once; a null sink keeps it silent.
namespace CgsDev { namespace Log {
struct NullPrint { template <class T> NullPrint& operator<<(const T&) { return *this; } };
NullPrint* gpDebugPrint = nullptr;
} }

namespace BrnPhysics { namespace Vehicle {

struct SimpleVehiclePhysics
{
    CgsGeometric::AxisAlignedBox mBox;
    Matrix44Affine mTransform;
    Vector3 mLinearVelocity;
    Vector3 mAngularVelocity;
    const CgsGeometric::AxisAlignedBox& GetDeformableAABB() const { return mBox; }
    Matrix44Affine GetTransform() const { return mTransform; }
    Vector3 GetLinearVelocity() const { return mLinearVelocity; }
    Vector3 GetAngularVelocity() const { return mAngularVelocity; }
};

inline f32 Dot3(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
const f32 KF_UNIT_NORMAL_TOLERANCE = 0.00999999978f;
inline bool IsUnitLength(const Vector3& lrV)
{
    const f32 lfLengthSq = Dot3(lrV, lrV);
    const f32 lfLength   = (lfLengthSq == 0.0f) ? 0.0f : lfLengthSq / sqrtf(lfLengthSq);
    return !(fabsf(lfLength - 1.0f) > KF_UNIT_NORMAL_TOLERANCE);
}

struct VehicleManager
{
    unsigned char mHead[64];                 // no body sits at offset 0, as in the real manager
    SimpleVehiclePhysics maBodies[4];
    u32  muCachedCarASlot = 0;
    u32  muCachedCarBSlot = 0;
    bool mbCachedCarCarPredictionResult = false;
    Vector3 mCachedCarCarPredictionNormal = { 0.0f, 0.0f, 1.0f, 0.0f };

    u32 CachedCarIdentity(const SimpleVehiclePhysics* lpBody) const;
    bool PredictCarCarIntersection(const SimpleVehiclePhysics* lpBodyA,
                                   const SimpleVehiclePhysics* lpBodyB, f32 lfTimestep);
};

#include "extracted.inc"

} }

using namespace BrnPhysics::Vehicle;

// A Cavalry-sized car in its own space: 1.9 m wide (x), 1.4 m tall (y), 4.5 m long (z).
static void Place(SimpleVehiclePhysics& lrBody, f32 x, f32 z, f32 vx, f32 vz)
{
    lrBody.mBox.mMin = { -0.95f, -0.70f, -2.25f, 0.0f };
    lrBody.mBox.mMax = {  0.95f,  0.70f,  2.25f, 0.0f };
    lrBody.mTransform.xAxis = { 1.0f, 0.0f, 0.0f, 0.0f };
    lrBody.mTransform.yAxis = { 0.0f, 1.0f, 0.0f, 0.0f };
    lrBody.mTransform.zAxis = { 0.0f, 0.0f, 1.0f, 0.0f };
    lrBody.mTransform.wAxis = { x, 0.5f, z, 1.0f };
    lrBody.mLinearVelocity  = { vx, 0.0f, vz, 0.0f };
    lrBody.mAngularVelocity = { 0.0f, 0.0f, 0.0f, 0.0f };
}

int main()
{
    int liChecks = 0, liFailures = 0;
    auto Check = [&](bool lbPass, const char* lpcName) {
        ++liChecks;
        if (!lbPass) { ++liFailures; std::printf("FAIL: %s\n", lpcName); }
    };
    TestInitializeVolumeVTable();   // boot fills the volume descriptor table
    static VehicleManager lManager;
    const f32 kDt = 1.0f / 60.0f;
    SimpleVehiclePhysics& A = lManager.maBodies[0];
    SimpleVehiclePhysics& B = lManager.maBodies[1];
    auto Fresh = [&]() { lManager.muCachedCarASlot = 0; lManager.muCachedCarBSlot = 0; };

    // Head-on, 30 m/s each, noses 0.4 m apart: the step-ahead boxes interpenetrate.
    Place(A, 0.0f, 0.0f, 0.0f, 30.0f);
    Place(B, 0.0f, 4.9f, 0.0f, -30.0f);
    Fresh();
    Check(lManager.PredictCarCarIntersection(&A, &B, kDt), "head-on closing pair is a hit");
    Check(IsUnitLength(lManager.mCachedCarCarPredictionNormal), "a hit caches a unit contact normal");
    Check(std::fabs(lManager.mCachedCarCarPredictionNormal.z) > 0.9f, "the head-on normal is along the cars' length");

    // Side by side 1.8 m apart, running parallel: the raw boxes overlap by 0.1 m, but only the
    // outer 15% of each car's half-width -- the console's near miss.
    Place(A, 0.0f, 0.0f, 0.0f, 25.0f);
    Place(B, 1.8f, 0.0f, 0.0f, 25.0f);
    Fresh();
    Check(!lManager.PredictCarCarIntersection(&A, &B, kDt), "a graze inside the outer 15% of the width is a near miss");
    Check(!lManager.mbCachedCarCarPredictionResult, "a miss caches false");

    // Side by side 1.5 m apart: well inside the 0.85 width -- a hit.
    Place(B, 1.5f, 0.0f, 0.0f, 25.0f);
    Fresh();
    Check(lManager.PredictCarCarIntersection(&A, &B, kDt), "a 0.4 m side overlap is a hit");

    // Lateral closing: 1.9 m apart now, B sliding in at 30 m/s -> 1.4 m after the step: a hit.
    Place(B, 1.9f, 0.0f, -30.0f, 25.0f);
    Fresh();
    Check(lManager.PredictCarCarIntersection(&A, &B, kDt), "the prediction uses the step-ahead pose (closing)");
    // ... and a pair that is a hit where it stands (1.5 m) but separating at 30 m/s is not.
    Place(B, 1.5f, 0.0f, 30.0f, 25.0f);
    Fresh();
    Check(!lManager.PredictCarCarIntersection(&A, &B, kDt), "the prediction uses the step-ahead pose (separating)");

    // Far apart: no contact.
    Place(B, 10.0f, 0.0f, 0.0f, 25.0f);
    Fresh();
    Check(!lManager.PredictCarCarIntersection(&A, &B, kDt), "a distant pair is not a hit");

    // Memo, both orders: a cached hit is returned for (A,B) and (B,A) even after the bodies move.
    Place(B, 1.5f, 0.0f, 0.0f, 25.0f);
    Fresh();
    const bool lbFirst = lManager.PredictCarCarIntersection(&A, &B, kDt);
    Place(B, 50.0f, 0.0f, 0.0f, 25.0f);
    Check(lbFirst && lManager.PredictCarCarIntersection(&A, &B, kDt), "memo hit, same order");
    Check(lManager.PredictCarCarIntersection(&B, &A, kDt), "memo hit, swapped order");
    Fresh();
    Check(!lManager.PredictCarCarIntersection(&A, &B, kDt), "a cleared memo re-predicts");

    Check(g_iAsserts == 0, "no unit-normal asserts");
    std::printf("PredictCarCarIntersection: %d checks, %d failures\n", liChecks, liFailures);
    return liFailures ? 1 : 0;
}
