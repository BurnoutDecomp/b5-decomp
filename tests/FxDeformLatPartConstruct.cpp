// Harness for run_fxdeformlat_part_construct.py (crash parity G28-D1 + G28-D2, FX-DEFORM-LAT).
//
// The shipped PhysicalBodyPart::Construct (BrnPhysicalBodyPart_Construct.cpp) and its TU constant are
// pasted in through methods.inc and run on a 0xCD-poisoned REAL PhysicalBodyPart (a pool slot that
// was never prepared: PhysicalBodyPartPool::Construct only loops maParts[i].Construct() over carved
// storage). ExternalPhysicsBody's Construct / SetMass / Prepare are recorder fixtures.
//
// ARTIST Construct @0x825B4178:
//   0x825B4194 vspltisw128 v127,0 ; 0x825B419C li r9,0x160 ; 0x825B41A8 stvx128 v127,r31,r9
//     -> mLocalJointPositionPlusRotation (+0x160) = 0                                  (G28-D1)
//   0x825B41A0 ld r10,qword_82F2A3A8 (x360rd FF x8 == K_INVALID_RIGID_BODY_ID) ; 0x825B41AC std
//     r10,0x1D0(r31) -> the whole 8-byte mRigidBodyId = ~0ull                           (G28-D2)
//   stw 0 +0x1DC/+0x1E0, stb 0 +0x1E5/+0x1E6 ; body Construct ; mass 5.0 ; body Prepare ;
//   0x825B4200..0x825B4210 stvx128 v127 -> +0x170/+0x180/+0x190/+0x1B0/+0x1C0.
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"
#include <cstdio>
#include <cstring>

static int giAsserts = 0, giBodyConstructs = 0, giBodyPrepares = 0;
static float gfMass = -1.0f;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpr, const char*, int) { ++giAsserts; std::printf("  assert: %s\n", lpcExpr); return 0; }
void* EndAssert() { return nullptr; }
} }
namespace BrnPhysics {
void ExternalPhysicsBody::Construct() { ++giBodyConstructs; }
bool ExternalPhysicsBody::Prepare() { ++giBodyPrepares; return true; }
void ExternalPhysicsBody::SetMass(f32 lfMass) { gfMass = lfMass; }
}

#include "methods.inc"

using namespace BrnPhysics::Deformation;
alignas(16) static unsigned char gPartStorage[sizeof(PhysicalBodyPart)];

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}
static bool Zero16(const void* p)
{
    static const unsigned char kau8Zero[16] = {};
    return std::memcmp(p, kau8Zero, 16) == 0;
}

int main()
{
    std::memset(gPartStorage, 0xCD, sizeof(gPartStorage));
    PhysicalBodyPart& lrPart = *reinterpret_cast<PhysicalBodyPart*>(gPartStorage);
    lrPart.Construct();

    Check(Zero16(&lrPart.mLocalJointPositionPlusRotation), "mLocalJointPositionPlusRotation = 0 (0x825B41A8 stvx128 +0x160)");
    Check(lrPart.mRigidBodyId.GetBaseRigidBodyID() == 0xFFFFFFFFFFFFFFFFull, "mRigidBodyId = K_INVALID_RIGID_BODY_ID ~0ull (0x825B41AC std)");
    Check(Zero16(&lrPart.mLocalGraphicsPositionPlusJointVelocity) && Zero16(&lrPart.mLocalInitialComPositionPlusMaxJointAngle)
          && Zero16(&lrPart.mLocalInitialJointPositionPlusLimitStress) && Zero16(&lrPart.mWorldPenetrationPlusCollisionMagnitude)
          && Zero16(&lrPart.mAverageCollisionPointPlusNumCollisions), "the five trailing rows = 0 (control)");
    Check(lrPart.mpIKPart == nullptr && lrPart.mpDeformableObject == nullptr && !lrPart.mbAddedToScene && !lrPart.mbFrozen,
          "bindings / flags cleared (control)");
    Check(giBodyConstructs == 1 && giBodyPrepares == 1 && gfMass == 5.0f, "body Construct, mass 5.0, Prepare (control)");
    Check(giAsserts == 0, "no tripwire (control)");
    std::printf("FxDeformLatPartConstruct: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
