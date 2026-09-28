// OWNERLIST 2026-09-27, lane L5 MENUS: the crash-nav fly-by's BACKWARD lane walk,
// TrafficLaneTruck::MoveAlongTrafficLaneBackwards @0x8222B100 (and its split picker @0x821FAF90), extracted from
// src/GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.cpp by run_menus_roadrunner.py and run over a
// synthetic two-section lane against stand-ins with the production member names.
//
// The console keeps the parameter 0.0001 (flt_82002540 == 0x38D1B717) short of the rung it leaves:
//   0x8222B470..0x8222B478  remaining = ((param - Floor(param)) - 0.0001) * segment length
//   0x8222B488..0x8222B4B4  --rung ; param = Floor(param) - 0.0001            (then "out of sync(2)")
//   0x8222B91C..0x8222B944  new section: rung = muNumRungs - 2 ; param = (f32)(muNumRungs - 1) - 0.0001
// The PC body had none of the three: the parameter landed ON the rung, every later step consumed zero distance, and
// both "Current rung got out of sync" asserts fired each step (2.05 million in one crash-nav pause).
// Expected values: a float32 model of the console body (numpy, each operation rounded once) -- see the runner.
#include "types.hpp"
#include "rw/math/vpu/types.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;

#define CGS_ASSERT(lbCondition, lpcMessage) do { if (!(lbCondition)) { ++gAsserts; } } while (0)

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static u32 Bits(f32 lfValue)
{
    u32 luBits;
    std::memcpy(&luBits, &lfValue, sizeof(luBits));
    return luBits;
}

struct VecFloat { f32 x, y, z, w; };   // the global broadcast-lane type the traffic section takes

namespace BrnTraffic
{
    enum Directions { E_DIR_LEFT = 0, E_DIR_STRAIGHT_ON = 1, E_DIR_RIGHT = 2, E_DIRECTIONS_COUNT = 3 };
    struct Rung { f32 mfUnused; };

    struct Section
    {
        u8  muNumRungs;
        u16 mauBackwardHulls[E_DIRECTIONS_COUNT];
        u8  mauBackwardSections[E_DIRECTIONS_COUNT];
        u32 GetNumSegments() const { return static_cast<u32>(muNumRungs) - 1u; }
        // The sampled point: x = the section-local parameter (enough to see where the walk stopped).
        void CalcTransformAtParameter(const Rung*, const VecFloat& lrParam, u32, rw::math::vpu::Vector3& lrPos,
                                      rw::math::vpu::Vector3& lrAt, rw::math::vpu::Vector3& lrRight) const
        {
            lrPos   = rw::math::vpu::Vector3{ lrParam.x, 0.0f, 0.0f, 0.0f };
            lrAt    = rw::math::vpu::Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
            lrRight = rw::math::vpu::Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
        }
    };

    struct Hull
    {
        u8             muNumSections;
        const Section* mpaSections;
        const f32*     mpaLengths[2];
        const Rung*    mpaRungs;
        const Section* GetSection(u32 luIndex) const { return &mpaSections[luIndex]; }
        const f32*     GetRungLengthsForSection(const Section* lpSection) const
        {
            return mpaLengths[lpSection - mpaSections];
        }
    };
}

namespace BrnDirector
{
    struct WorldMap
    {
        struct LanePosition
        {
            rw::math::vpu::Vector3 mPosition;
            f32                    mfParamAlongSection;
            u16                    muHullIndex;
            u8                     muSection;
            u8                     muRung;
            bool                   mbValid;
        };
        const BrnTraffic::Hull* mpHull;
        const BrnTraffic::Hull* GetTrafficHullData(u32) const { return mpHull; }
        u32                     GetNumTrafficHulls() const { return 1u; }
    };

    namespace Camera
    {
        namespace Utils
        {
            // Stand-in: the look-at frame carries the eye in its translation row.
            inline rw::math::vpu::Matrix44Affine CreateLookAt(rw::math::vpu::Vector3 lEye, rw::math::vpu::Vector3)
            {
                rw::math::vpu::Matrix44Affine lFrame;
                lFrame.SetIdentity();
                lFrame.wAxis = lEye;
                return lFrame;
            }
        }

        struct TrafficLaneTruck
        {
            static void PickSplitToTakeBackwards(const BrnTraffic::Section& lrSection,
                                                 BrnTraffic::Directions lePreferredDirection,
                                                 u8* lpOutSection, u16* lpOutHull, u8* lpOutDirection);
            static void MoveAlongTrafficLaneBackwards(BrnTraffic::Directions lePreferredDirection,
                                                      const BrnDirector::WorldMap& lrWorldMap,
                                                      f32 lfDistToMove,
                                                      BrnDirector::WorldMap::LanePosition* lpPositionInOut,
                                                      rw::math::vpu::Matrix44Affine* lpTransformOut);
        };

#include "menus_roadrunner.inc"
    }
}

namespace
{
    // Two sections of five rungs, ten metres apart. Section 0 splits backwards (straight on) into section 1;
    // section 1 is a dead end backwards.
    const f32 kaLengths[5] = { 0.0f, 10.0f, 20.0f, 30.0f, 40.0f };
    const BrnTraffic::Rung kaRungs[1] = { { 0.0f } };
    const BrnTraffic::Section kaSections[2] =
    {
        { 5, { 0xFFFF, 0, 0xFFFF }, { 0xFF, 1, 0xFF } },
        { 5, { 0xFFFF, 0xFFFF, 0xFFFF }, { 0xFF, 0xFF, 0xFF } },
    };
    const BrnTraffic::Hull kHull = { 2, kaSections, { kaLengths, kaLengths }, kaRungs };
    const BrnDirector::WorldMap kWorldMap = { &kHull };

    struct WalkResult
    {
        BrnDirector::WorldMap::LanePosition mPosition;
        unsigned                            muAsserts;
    };

    WalkResult Walk(u8 luSection, u8 luRung, f32 lfParam, f32 lfDistance)
    {
        WalkResult lResult;
        lResult.mPosition.mPosition            = rw::math::vpu::Vector3{ 7.0f, 7.0f, 7.0f, 0.0f };
        lResult.mPosition.mfParamAlongSection  = lfParam;
        lResult.mPosition.muHullIndex          = 0;
        lResult.mPosition.muSection            = luSection;
        lResult.mPosition.muRung               = luRung;
        lResult.mPosition.mbValid              = true;
        rw::math::vpu::Matrix44Affine lTransform;
        const unsigned luBefore = gAsserts;
        BrnDirector::Camera::TrafficLaneTruck::MoveAlongTrafficLaneBackwards(
            BrnTraffic::E_DIR_STRAIGHT_ON, kWorldMap, lfDistance, &lResult.mPosition, &lTransform);
        lResult.muAsserts = gAsserts - luBefore;
        return lResult;
    }
}

int main()
{
    // A: inside one section -- 12 m back from 25 m (rung 2, param 2.5).
    {
        const WalkResult lR = Walk(0, 2, 2.5f, 12.0f);
        Check(lR.muAsserts == 0, "A: no out-of-sync assert");
        Check(lR.mPosition.mbValid, "A: the lane position stays valid");
        Check(lR.mPosition.muSection == 0 && lR.mPosition.muRung == 1, "A: section 0, rung 1");
        Check(Bits(lR.mPosition.mfParamAlongSection) == 0x3FA65FD8u, "A: param == 0x3FA65FD8 (1.2998)");
    }
    // B: across the section start into the split -- 8 m back from 5 m (rung 0, param 0.5).
    {
        const WalkResult lR = Walk(0, 0, 0.5f, 8.0f);
        Check(lR.muAsserts == 0, "B: no out-of-sync assert");
        Check(lR.mPosition.mbValid, "B: the lane position stays valid");
        Check(lR.mPosition.muSection == 1 && lR.mPosition.muRung == 3, "B: section 1, rung 3 (its last segment)");
        Check(Bits(lR.mPosition.mfParamAlongSection) == 0x406CC986u, "B: param == 0x406CC986 (3.6998)");
    }
    // C: four segments and a section change -- 35 m back from 32.5 m (rung 3, param 3.25).
    {
        const WalkResult lR = Walk(0, 3, 3.25f, 35.0f);
        Check(lR.muAsserts == 0, "C: no out-of-sync assert");
        Check(lR.mPosition.mbValid, "C: the lane position stays valid");
        Check(lR.mPosition.muSection == 1 && lR.mPosition.muRung == 3, "C: section 1, rung 3");
        Check(Bits(lR.mPosition.mfParamAlongSection) == 0x406FF2E5u, "C: param == 0x406FF2E5 (3.7492)");
    }
    // D: past the dead end -- 100 m back ends the lane (EndOfLane: invalid, the frame at the seated point).
    {
        const WalkResult lR = Walk(0, 1, 1.5f, 100.0f);
        Check(lR.muAsserts == 0, "D: no out-of-sync assert on the way to the dead end");
        Check(!lR.mPosition.mbValid, "D: the dead end invalidates the lane position");
    }

    std::printf("MenusRoadRunnerBackwards: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
