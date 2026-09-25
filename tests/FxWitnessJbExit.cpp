// FX-WITNESS (crash parity 2026-09-24): the [jb-exit] witness names the exit the REAL
// PhysicalBodyPart::TestJointForBreaking body took, and reports the exact ratios that body computed.
//
// Everything that decides the verdict is extracted production text (tests/run_fxwitness_jb_exit.py):
//   jb_census.inc  BrnPhysicalBodyPart.cpp: the gate-census counters + JbDecade, whose ratio ring is
//                  the witness's breadcrumb
//   jb_read.inc    BrnPhysicalBodyPart.cpp: ReadJointBreakCensusDiag
//   jb_decode.inc  BrnDetachedPartManager.cpp: JbExitDecoded + JbExitDecode
//   jb_body.inc    BrnPhysicalBodyPart.cpp: TestJointForBreaking, re-hosted on the same stand-ins
//                  run_joint_break.py uses (its constants are extracted too)
// The stand-ins are only the neighbours the body calls (the IK part's gate answers, the owner body,
// the output queue). The census, the ring and the decode are the shipped code.
//
// Expected values are the body's own arithmetic, stated independently: penRatio = stress * 1.5 /
// maxStress (kfJointPenetrationMultiplier, 0x82FB95D0 <- flt_820945DC), forceRatio =
// |v . axis| * 0.4 / maxStress (kfJointForceMultiplier, 0x82FB96F0 <- flt_8200473C).
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPart.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKBodyPart.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
using namespace BrnPhysics;
using namespace BrnPhysics::Deformation;
static unsigned assertions = 0, checks = 0, failures = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++assertions; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace BrnPhysics { namespace Deformation {
namespace {
#include "jb_census.inc"
}
#include "jb_read.inc"
namespace {
#include "jb_decode.inc"
}
} }

static bool DetachProbeOn() { return false; }   // the census print; not under test
static float kfJointForceMultiplier = .4f, kfJointPenetrationMultiplier = 1.5f;

struct IKFixture {
    DeformationJointSpec joint{}; int type = 0; bool sensor = true; unsigned calls = 0;
    unsigned GetActiveJointIndex() const { return 0; }
    const DeformationJointSpec* GetActiveJointSpec() const { return &joint; }
    int GetPartType() const { return type; }
    bool CheckSensorForcesForJointDetachment(bool) { ++calls; return sensor; }
};
struct OwnerFixture { ExternalPhysicsBody body; const ExternalPhysicsBody& GetVehicleBody() const { return body; } };
struct OutputFixture { DeformationOutputInterface out; DeformationOutputInterface* GetDeformationOutputInterface() { return &out; } };
struct JointBreakFixture {
    ExternalPhysicsBody mRwBody; IKFixture* mpIKPart; OwnerFixture* mpDeformableObject;
    BurnoutBodyPartID mRigidBodyId{}; EntityId mGlobalVehicleId{};
    rw::math::vpu::Vector3Plus mWorldPenetrationPlusCollisionMagnitude{}, mLocalInitialJointPositionPlusLimitStress{};
    bool mbJoinedToVehicle = true; float proportion = 1; unsigned simCalls = 0;
    VecFloat GetJointRotationProportion() const { return { proportion, proportion, proportion, proportion }; }
    void AddToSim(CgsPhysics::PhysicsSimulationIO::InputBuffer*, const Matrix44Affine&, Vector3, Vector3) { ++simCalls; }
    bool TestJointForBreaking(CgsPhysics::PhysicsSimulationIO::InputBuffer*, OutputFixture*);
};
#include "jb_body.inc"

static void Check(bool pass, const char* name, int scenario)
{
    ++checks;
    if (!pass) { ++failures; std::fprintf(stderr, "FAIL: scenario %d: %s\n", scenario, name); }
}

// One scenario: the gate answers, the joint's max stress, the limit stress, the world penetration
// (which engages arm A's axis gate when it has a component along the axis) and the vehicle velocity.
struct Scenario {
    const char* exitName; float maxStress; float proportion; int type; bool sensor;
    float stress; Vector3 penetration; Vector3 velocity;
    unsigned ratios; bool armARan; bool broke;
};

static void RunOne(const Scenario& s, int index)
{
    IKFixture ik; OwnerFixture owner; OutputFixture output; JointBreakFixture part;
    output.out.mDetachedPartNotificationQueue.Construct();
    part.mpIKPart = &ik; part.mpDeformableObject = &owner;
    Matrix44Affine pose; pose.SetIdentity();
    owner.body.SetTransform(pose); part.mRwBody.SetTransform(pose);
    owner.body.SetLinearVelocity(s.velocity); owner.body.SetAngularVelocity({});
    ik.joint.mJointAxis = { 1, 0, 0, 0 };
    ik.joint.mfJointDetachThreshold = s.maxStress;
    ik.type = s.type; ik.sensor = s.sensor; part.proportion = s.proportion;
    part.mLocalInitialJointPositionPlusLimitStress.SetPlus(s.stress);
    part.mWorldPenetrationPlusCollisionMagnitude.SetVector3(s.penetration);

    JointBreakCensusDiag before, after;
    ReadJointBreakCensusDiag(before);
    const bool broke = part.TestJointForBreaking(nullptr, &output);
    ReadJointBreakCensusDiag(after);
    const JbExitDecoded d = JbExitDecode(before, after, broke);

    Check(broke == s.broke, "the body's own verdict is the scenario's", index);
    Check(std::strcmp(d.mpcExit, s.exitName) == 0, "decode names the exit the body took", index);
    Check(d.mbOk, "decode self-check is ok for one call", index);
    Check(d.muRatios == s.ratios, "the ring recorded the expected number of ratios", index);
    Check(d.mbArmARan == s.armARan, "arm A engagement decoded", index);
    const bool g2 = std::strcmp(s.exitName, "g2") == 0;
    const bool pastG3b = !g2 && std::strcmp(s.exitName, "g3a") != 0 && std::strcmp(s.exitName, "g3b") != 0;
    Check(d.mbG2 == g2, "g2 flag", index);
    Check(d.mbPastG3b == pastG3b, "past-g3b flag (the sensor band was evaluated)", index);
    Check(d.mbSensorExit == (std::strcmp(s.exitName, "g3c") == 0), "g3c exit flag", index);
    Check(d.mbArmsRan == (std::strcmp(s.exitName, "short") == 0 || std::strcmp(s.exitName, "BREAK") == 0),
          "arms-ran flag", index);
    if (s.ratios >= 1) {
        const float expectedPen = s.stress * 1.5f / s.maxStress;
        Check(d.mfPenRatio == expectedPen, "penRatio is stress*1.5/maxStress, bit for bit", index);
    }
    if (s.ratios >= 3) {
        const float dot = s.velocity.x;   // identity pose: the world axis is (1,0,0)
        const float expectedForce = std::fabs(dot) * 0.4f / s.maxStress;
        Check(d.mfForceRatio == expectedForce, "armA ratio is |v.axis|*0.4/maxStress, bit for bit", index);
    }
}

int main()
{
    const Vector3 zero{ 0, 0, 0, 0 }, alongAxis{ 1, 0, 0, 0 };
    const Scenario scenarios[] = {
        // exit     maxStr prop  type sensor stress pen        velocity           ratios armA  broke
        { "g2",     -1.0f, 1.0f, 0,  true,  2.0f,  zero,      zero,              0u,   false, false },
        { "g3a",     1.0f, 0.2f, 0,  true,  2.0f,  zero,      zero,              2u,   false, false },
        { "g3b",     1.0f, 1.0f, 3,  true,  2.0f,  zero,      zero,              2u,   false, false },
        { "g3c",     0.75f,1.0f, 26, false, 1.29f, zero,      zero,              2u,   false, false },
        { "short",   1.0f, 1.0f, 0,  true,  0.2f,  zero,      zero,              2u,   false, false },
        { "BREAK",   0.75f,1.0f, 26, true,  1.29f, zero,      zero,              2u,   false, true  },
        { "BREAK",   1.0f, 1.0f, 0,  true,  0.0f,  alongAxis, { 10, 20, 30, 0 }, 3u,   true,  true  },
        { "short",   1.0f, 1.0f, 0,  true,  0.1f,  alongAxis, { 1, 0, 0, 0 },    3u,   true,  false },
        { "g3a",     0.5f, 0.29f,0,  true,  0.4f,  zero,      zero,              2u,   false, false },
    };
    const int n = static_cast<int>(sizeof(scenarios) / sizeof(scenarios[0]));
    // Six passes over every scenario on ONE census, so the 4-slot ring wraps at every phase.
    for (int pass = 0; pass < 6; ++pass)
        for (int i = 0; i < n; ++i) RunOne(scenarios[i], pass * 100 + i);

    // Negative control: a snapshot pair spanning TWO calls must fail the self-check (it bites).
    {
        IKFixture ik; OwnerFixture owner; OutputFixture output; JointBreakFixture part;
        output.out.mDetachedPartNotificationQueue.Construct();
        part.mpIKPart = &ik; part.mpDeformableObject = &owner;
        Matrix44Affine pose; pose.SetIdentity(); owner.body.SetTransform(pose); part.mRwBody.SetTransform(pose);
        ik.joint.mJointAxis = { 1, 0, 0, 0 }; ik.joint.mfJointDetachThreshold = 1.0f; part.proportion = 0.1f;
        JointBreakCensusDiag before, after;
        ReadJointBreakCensusDiag(before);
        part.TestJointForBreaking(nullptr, &output);
        part.TestJointForBreaking(nullptr, &output);
        ReadJointBreakCensusDiag(after);
        Check(!JbExitDecode(before, after, false).mbOk, "a two-call window fails the decode self-check", 999);
    }
    Check(assertions == 0, "valid fixtures fire no assertion", 1000);
    std::printf("FxWitnessJbExit: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
