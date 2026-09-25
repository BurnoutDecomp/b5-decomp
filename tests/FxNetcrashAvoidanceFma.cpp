// crash parity FX-NETCRASH (REVIEW_G item 4, 2026-09-24): the traffic avoidance pipeline's fused
// multiply-adds. The PRODUCTION bodies of
// src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp, extracted by
// run_fxnetcrash_avoidance_fma.py with the constants and helpers they reach:
//   Avoidance_CalculateFeelers (inlined in 0x8272C248), Avoidance_CalculatePassingScore @0x827199B8
//   (+ Avoidance_CalculateDistancePosVelToOrigin @0x82708DD0, Convert3DVectorTo2D, the GetAvoidPass*
//   lane accessors), CalculateAndSetSteeringUsingAvoidance @0x8273D258 -- whose two callees,
//   Avoidance_GetBestVehicleDirection and CalculateAndSetSteering, are SCRIPTED / RECORDING doubles
//   here, so the blend at 0x8273D33C is what the test sees.
// The expected bit patterns come from fma_cases.inc, which the runner computes with exact rational
// arithmetic and one rounding per console instruction; each case is an input on which the fused
// (console) and the twice-rounded (pre-fix) result differ. Every comparison is BITWISE.
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficMathsUtils.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    char* gpcMessageBuffer = nullptr;
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::fprintf(stderr, "ASSERT: %s\n", lpcMessage);
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }             // the witness stream stays off
namespace Message { unsigned long long gxMessageFilterFlags = 0; }
}

typedef BrnPhysics::Vehicle::BrnTrafficDriverControls Controls;

// ---- the doubles' state -----------------------------------------------------------------------------
static Vector3  gScriptAvoid = {};   // what Avoidance_GetBestVehicleDirection hands back
static f32      gScriptRisk  = 0.0f; // and its risk
static Vector3  gSteerDir    = {};   // the direction CalculateAndSetSteering received
static unsigned gSteerCalls  = 0;

// Reached only by CalculateAndSetSteeringUsingAvoidance's [T-avoid] line, whose stream is null here.
VecFloat BrnTraffic::Vehicle::GetSpeed() const { return VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f }; }

namespace BrnTraffic
{
    CgsDev::Log::DebugPrint* TrafficDiagStream() { return nullptr; }

    struct AvoidFixture
    {
        typedef TrafficEntityModule M;

        decltype(M::mCachedCollidableList)                      mCachedCollidableList;
        decltype(M::maFeelerCosSin)                             maFeelerCosSin;
        decltype(M::kfVehicle_AvoidancePassingFactor_Constants) kfVehicle_AvoidancePassingFactor_Constants;
        decltype(M::kfVehicle_Avoidance_Constants)              kfVehicle_Avoidance_Constants;
        decltype(M::mfSimTimeStep)                              mfSimTimeStep;
        decltype(M::mbDEBUGEnableAvoidance)                     mbDEBUGEnableAvoidance;

        Vehicle* GetVehicle(u32) { return nullptr; }

        VecFloat GetAvoidPassImpactTimeMax() const;
        VecFloat GetAvoidPassImpactTimeScoreFactor() const;
        VecFloat GetAvoidPassMaxDistance() const;
        VecFloat GetAvoidPassHeightSkip() const;

        VecFloat Avoidance_CalculateDistancePosVelToOrigin(Vector2 lStart, Vector2 lVel);
        VecFloat Avoidance_CalculatePassingScore(Vector3 lPositionA, Vector3 lVelocityA, Vector3 lPositionB,
                                                 Vector3 lVelocityB, VecFloat lfObjectBHalfLength,
                                                 VecFloat lfObjectBHalfWidth);
        void     Avoidance_CalculateFeelers(Vector3 lDirection, Vector3 lRight, Vector3* laFeelers);
        void     CalculateAndSetSteeringUsingAvoidance(u32 luVehicle, Vector3& lNewDirection, VecFloat lfDistFromTarget,
                                                       Controls* lpOutControls, VecFloat& lfOverallRisk);

        void Avoidance_GetBestVehicleDirection(u32, Vector3& lNewDirection, VecFloat& lfOverallRisk)
        {
            lNewDirection = gScriptAvoid;
            lfOverallRisk = VecFloat{ gScriptRisk, gScriptRisk, gScriptRisk, gScriptRisk };
        }
        void CalculateAndSetSteering(u32, Vector3 lTargetDirection, Controls*, VecFloat)
        {
            ++gSteerCalls;
            gSteerDir = lTargetDirection;
        }
    };
}

// The production bodies under test, then the generated cases.
#include "fma_bodies.inc"
#include "fma_cases.inc"

using namespace BrnTraffic;

static void Check(bool lbPass, const char* lpcName, int liCase)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s (case %d)\n", lpcName, liCase);
    }
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 FromBits(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static Vector3 V3Bits(const u32* lpu) { return Vector3{ FromBits(lpu[0]), FromBits(lpu[1]), FromBits(lpu[2]), 0.0f }; }
static bool SameBits(const Vector3& lr, const u32* lpu)
{
    const bool lbSame = Bits(lr.x) == lpu[0] && Bits(lr.y) == lpu[1] && Bits(lr.z) == lpu[2];
    if (!lbSame)
    {
        std::fprintf(stderr, "  got %08X %08X %08X want %08X %08X %08X\n", Bits(lr.x), Bits(lr.y), Bits(lr.z),
                     lpu[0], lpu[1], lpu[2]);
    }
    return lbSame;
}

alignas(64) static unsigned char gaFixture[sizeof(AvoidFixture)];
static AvoidFixture& A() { return *reinterpret_cast<AvoidFixture*>(gaFixture); }

int main()
{
    std::memset(gaFixture, 0, sizeof(gaFixture));
    // Construct's seeds (0x82740690 / 0x82740694): {flt_820BA8DC 4, flt_820BA5E4 10, 10, flt_820BA5F4 3}
    // and {flt_820BA5E4 10, flt_82F2FE90 50, 0, 0}.
    A().kfVehicle_AvoidancePassingFactor_Constants = Vector4{ 4.0f, 10.0f, 10.0f, 3.0f };
    A().kfVehicle_Avoidance_Constants              = Vector4{ 10.0f, 50.0f, 0.0f, 0.0f };
    for (int i = 0; i < 2; ++i)
    {
        const f32 lfCos = FromBits(KU_COS_SIN[i][0]);
        const f32 lfSin = FromBits(KU_COS_SIN[i][1]);
        A().maFeelerCosSin[i] = Vector2{ lfCos, lfSin, lfCos, lfCos };   // (cos, sin, cos, cos)
    }
    A().mfSimTimeStep          = FromBits(KU_STEP);
    A().mbDEBUGEnableAvoidance = true;
    A().mCachedCollidableList.Clear();

    // ==== Avoidance_CalculateFeelers: [3] / [4] fused (vmaddfp128 0x8272C424 / 0x8272C40C),
    //      [1] / [2] rounded three times (vmulfp128, vmulfp128, vsubfp) ===========================
    int liCase = 0;
    for (const FeelerCase& lrCase : KA_FEELER_CASES)
    {
        Vector3 la[KI_TRAFFIC_AVOIDANCE_FEELERS];
        const Vector3 lDir = V3Bits(lrCase.dir);
        A().Avoidance_CalculateFeelers(lDir, V3Bits(lrCase.right), la);
        const bool lbPass = SameBits(la[0], lrCase.dir)
                            && SameBits(la[1], lrCase.feeler[0]) && SameBits(la[2], lrCase.feeler[1])
                            && SameBits(la[3], lrCase.feeler[2]) && SameBits(la[4], lrCase.feeler[3]);
        Check(lbPass, "FEELERS [0] Dir, [1]/[2] Dir*sin - Right*cos, [3]/[4] fma(Dir, sin, Right*cos) -- bitwise", liCase++);
    }

    // ==== Avoidance_CalculatePassingScore: fma(ImpactTimeMax - t, ScoreFactor, passing) (0x82719B5C)
    liCase = 0;
    for (const PassingCase& lrCase : KA_PASSING_CASES)
    {
        const f32 lfA = FromBits(lrCase.a);
        const f32 lfV = FromBits(lrCase.v);
        const VecFloat lScore = A().Avoidance_CalculatePassingScore(
            Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, lfV, 0.0f },
            Vector3{ lfA, 0.0f, 0.0f, 0.0f },  Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },
            VecFloat{ 2.0f, 2.0f, 2.0f, 2.0f }, VecFloat{ 1.0f, 1.0f, 1.0f, 1.0f });
        const bool lbPass = Bits(lScore.x) == lrCase.fused && Bits(lScore.y) == lrCase.fused
                            && Bits(lScore.z) == lrCase.fused && Bits(lScore.w) == lrCase.fused;
        if (!lbPass)
        {
            std::fprintf(stderr, "  score %08X, console %08X, twice-rounded %08X\n", Bits(lScore.x), lrCase.fused,
                         lrCase.twice);
        }
        Check(lbPass, "PASSING SCORE is ONE rounding of (4 - t) * 10 + (10 - min(|space|, 10)), splatted -- bitwise", liCase++);
    }

    // ==== CalculateAndSetSteeringUsingAvoidance: risk 0.5 >= 0.2, dist 5 >= 1, dot < 0.94 -> the blend
    //      fma(avoid - target, mfSimTimeStep, target) per lane (0x8273D33C) ===============================
    liCase = 0;
    for (const BlendCase& lrCase : KA_BLEND_CASES)
    {
        gScriptAvoid = V3Bits(lrCase.avoid);
        gScriptRisk  = 0.5f;
        Vector3 lTarget = V3Bits(lrCase.target);
        VecFloat lRisk  = VecFloat{ -7.0f, -7.0f, -7.0f, -7.0f };
        Controls lControls;
        std::memset(&lControls, 0, sizeof(lControls));
        A().CalculateAndSetSteeringUsingAvoidance(7u, lTarget, VecFloat{ 5.0f, 5.0f, 5.0f, 5.0f }, &lControls, lRisk);
        Check(SameBits(lTarget, lrCase.fused) && SameBits(gSteerDir, lrCase.fused),
              "BLEND lNewDirection = fma(avoid - target, mfSimTimeStep, target) per lane, and CalculateAndSetSteering gets it -- bitwise",
              liCase++);
    }
    Check(gSteerCalls == 3u && gAsserts == 0u, "the blend arm ran once per case (3 CalculateAndSetSteering calls), no tripwire", 0);

    // ==== the 0.94 gate (item 3c): `vmsum3fp128 v11, v12, v0` at 0x8273D310, ONE rounding of the exact
    //      dot. Cases 0/1: single >= 0.94f > sequential -> the console SNAPS (lNewDirection = avoid);
    //      cases 2/3: single < 0.94f <= sequential -> it BLENDS (fma per lane, 0x8273D33C) ================
    liCase = 0;
    for (const GateCase& lrCase : KA_GATE_CASES)
    {
        gScriptAvoid = V3Bits(lrCase.avoid);
        gScriptRisk  = 0.5f;
        Vector3 lTarget = V3Bits(lrCase.target);
        VecFloat lRisk  = VecFloat{ -7.0f, -7.0f, -7.0f, -7.0f };
        Controls lControls;
        std::memset(&lControls, 0, sizeof(lControls));
        A().CalculateAndSetSteeringUsingAvoidance(7u, lTarget, VecFloat{ 5.0f, 5.0f, 5.0f, 5.0f }, &lControls, lRisk);
        if (!SameBits(gSteerDir, lrCase.expected))
        {
            std::fprintf(stderr, "  gate dot: console (one rounding) %08X, sequential %08X, 0.94f 3F70A3D7\n",
                         lrCase.single, lrCase.sequential);
        }
        Check(SameBits(lTarget, lrCase.expected) && SameBits(gSteerDir, lrCase.expected),
              liCase < 2 ? "GATE the one-rounding dot reaches 0.94f: the console SNAPS to the avoid direction -- bitwise"
                         : "GATE the one-rounding dot stays below 0.94f: the console BLENDS -- bitwise",
              liCase);
        ++liCase;
    }
    Check(gSteerCalls == 7u && gAsserts == 0u, "the gate cases steered once each (7 calls in all), no tripwire", 0);

    std::printf("FxNetcrashAvoidanceFma: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
