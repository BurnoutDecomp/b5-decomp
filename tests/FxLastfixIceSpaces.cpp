// FX-LASTFIX item 1b (crash parity 2026-09-26) -- run_fxlastfix_ice_spaces.py: the take's own reference spaces.
//
// BehaviourIceAnim::Update @0x82247108 copies the MainDirector's shared ICE::CameraSpaceHandler (0x82247384, into the
// stack slot var_2B0) and overrides four of the COPY's spaces before its take evaluator reads it:
//   +0x000 mCarToWorld      <- the primary vehicle's +0x1F0 (VehicleInfo::mRaceCarState.mTransform)  0x82247388..0x822473CC
//   +0x040 mCar2ToWorld     <- the secondary vehicle's +0x1F0                                         0x822473D0..0x82247418
//   +0x1C0 mHeading2ToWorld <- this+0x610 (mHeadingSpaceTransform)                                    0x822473DC..0x8224744C
//   +0x140 mHeadingToWorld  <- world +0x80 (AllVehicleData::mPlayerLooseHeadingSpace), only when this+0xE2A
//                              (mbForceHeadingSpaceToBeLooseHeadingSpace) is set: 0x82247450 `beq` (clear) -> T =
//                              0x8224748C, no write; set -> F = 0x82247454..0x82247488.
// The runner lifts the revision's PRODUCTION text out of BrnBehaviourIceAnim.cpp -- Update's statements from
// `ICE::CameraSpaceHandler lSpaces(` up to the ShotContext -- into fxlastfix_iceanim_spaces_arm.inc and compiles it here
// against the REAL ICE::CameraSpaceHandler (the revision's ICECameraSpaceHandler.hpp) and the REAL AllVehicleData /
// VehicleInfo, with members named as the behaviour's. FxLastfixIceSpacesData.h holds the console's window
// 0x8224725C..0x8224748B run on emu64 (the copy constructor 0x821FAA88 interpreted): the stack handler's 0x204 bytes.
//
// EVERY CONSOLE OFFSET IS MAPPED TO THE MEMBER IT NAMES (the x64 host layout differs; nothing is read by offset):
//   the handler's +0x000..+0x1C0 -> kaHandlerMembers below; +0x200 -> mpGamePlayCam;
//   vehicle +0x1F0 -> VehicleInfo::mRaceCarState.mTransform; world +0x80 -> AllVehicleData::mPlayerLooseHeadingSpace;
//   this+0x610 -> mHeadingSpaceTransform; this+0xE2A -> mbForceHeadingSpaceToBeLooseHeadingSpace;
//   this+0xDF0 / +0xE00 -> mPrimaryVehicleRef / mSecondaryVehicleRef.
// Per row, ten checks: the eight matrices of the take's copy (16 lanes each, bit for bit), its mpGamePlayCam (the
// shared handler's, copied), and the shared handler left byte-for-byte unchanged.
#include "types.hpp"
#include "rw/math/vpu/types.h"
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"

#include <cstdio>
#include <cstring>

#include "FxLastfixIceSpacesData.h"

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { return 0; }
    void* EndAssert() { return nullptr; }
}
}

namespace
{
    unsigned Bits(float lf) { unsigned lu; std::memcpy(&lu, &lf, 4); return lu; }
    float Float(unsigned lu) { float lf; std::memcpy(&lf, &lu, 4); return lf; }
    bool IsNaNBits(unsigned lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }
    bool Same(unsigned luA, unsigned luB) { return luA == luB || (IsNaNBits(luA) && IsNaNBits(luB)); }

    rw::math::vpu::Vector3 Vec(const unsigned* lpu)
    {
        rw::math::vpu::Vector3 lv;
        lv.x = Float(lpu[0]); lv.y = Float(lpu[1]); lv.z = Float(lpu[2]); lv.w = Float(lpu[3]);
        return lv;
    }
    rw::math::vpu::Matrix44Affine Mat(const unsigned* lpu)
    {
        rw::math::vpu::Matrix44Affine lm;
        lm.xAxis = Vec(lpu); lm.yAxis = Vec(lpu + 4); lm.zAxis = Vec(lpu + 8); lm.wAxis = Vec(lpu + 12);
        return lm;
    }
    void Words(const rw::math::vpu::Matrix44Affine& lrM, unsigned* lpu)
    {
        const rw::math::vpu::Vector3* lap[4] = { &lrM.xAxis, &lrM.yAxis, &lrM.zAxis, &lrM.wAxis };
        for (int liRow = 0; liRow < 4; ++liRow)
        {
            lpu[4 * liRow + 0] = Bits(lap[liRow]->x); lpu[4 * liRow + 1] = Bits(lap[liRow]->y);
            lpu[4 * liRow + 2] = Bits(lap[liRow]->z); lpu[4 * liRow + 3] = Bits(lap[liRow]->w);
        }
    }

    // The console handler's +0x000..+0x1C0, each mapped to the member it names (DWARF ICECameraSpaceHandler.hpp:109-116).
    struct HandlerMember
    {
        unsigned                                        muConsoleOffset;
        const char*                                     mpcName;
        rw::math::vpu::Matrix44Affine ICE::CameraSpaceHandler::* mpMember;
    };
    const HandlerMember kaHandlerMembers[8] = {
        { 0x000u, "mCarToWorld",          &ICE::CameraSpaceHandler::mCarToWorld },
        { 0x040u, "mCar2ToWorld",         &ICE::CameraSpaceHandler::mCar2ToWorld },
        { 0x080u, "mTrafficLightToWorld", &ICE::CameraSpaceHandler::mTrafficLightToWorld },
        { 0x0C0u, "mSceneToWorld",        &ICE::CameraSpaceHandler::mSceneToWorld },
        { 0x100u, "mImpactToWorld",       &ICE::CameraSpaceHandler::mImpactToWorld },
        { 0x140u, "mHeadingToWorld",      &ICE::CameraSpaceHandler::mHeadingToWorld },
        { 0x180u, "mLooseHeadingToWorld", &ICE::CameraSpaceHandler::mLooseHeadingToWorld },
        { 0x1C0u, "mHeading2ToWorld",     &ICE::CameraSpaceHandler::mHeading2ToWorld },
    };
    const unsigned KU_CONSOLE_GAMEPLAY_CAM_WORD = 0x200u / 4u;   // the handler's +0x200 word: mpGamePlayCam
}

namespace BrnDirector
{
namespace Camera
{
// The shared info's accessor the arm reads (console: *(shared +0x5E4)).
struct FakeSharedInfo
{
    const ICE::CameraSpaceHandler* mpCameraSpaceHandler;
    const ICE::CameraSpaceHandler* GetCameraSpaceHandler() const { return mpCameraSpaceHandler; }
};

// A ref resolves to its fixture vehicle (console: VehicleRef::Get 0x822335A0, hooked the same way on emu64).
struct FakeVehicleRef
{
    const VehicleInfo* mpVehicle;
    const VehicleInfo* Get(const void* lpWorld) const { (void)lpWorld; return mpVehicle; }
};

struct IceAnimSpacesFixture
{
    FakeVehicleRef                mPrimaryVehicleRef;                          // this+0xDF0
    FakeVehicleRef                mSecondaryVehicleRef;                        // this+0xE00
    rw::math::vpu::Matrix44Affine mHeadingSpaceTransform;                      // this+0x610
    bool                          mbForceHeadingSpaceToBeLooseHeadingSpace;    // this+0xE2A

    ICE::CameraSpaceHandler RunArm(const FakeSharedInfo& lrSharedInfo, const AllVehicleData* lpWorld)
    {
#include "fxlastfix_iceanim_spaces_arm.inc"
        return lSpaces;
    }
};
} // namespace Camera
} // namespace BrnDirector

int main()
{
    using namespace BrnDirector;
    using namespace BrnDirector::Camera;
    static VehicleInfo sPrimary;
    static VehicleInfo sSecondary;
    static AllVehicleData sWorld;
    static ICE::CameraSpaceHandler sShared;
    static const int saiGameplayCams[8] = { 0 };
    unsigned luChecks = 0, luFailures = 0, luPrinted = 0;
    int liRowIndex = 0;

    for (const IceSpacesRow& lrRow : kaIceSpacesRows)
    {
        for (const HandlerMember& lrMember : kaHandlerMembers)
            sShared.*(lrMember.mpMember) = Mat(lrRow.mauShared + lrMember.muConsoleOffset / 4u);
        sShared.mpGamePlayCam = reinterpret_cast<decltype(sShared.mpGamePlayCam)>(&saiGameplayCams[liRowIndex++ & 7]);
        sPrimary.mRaceCarState.mTransform   = Mat(lrRow.mauPrimary);      // console vehicle +0x1F0
        sSecondary.mRaceCarState.mTransform = Mat(lrRow.mauSecondary);
        sWorld.mPlayerLooseHeadingSpace     = Mat(lrRow.mauLoose);        // console world +0x80

        IceAnimSpacesFixture lFixture;
        lFixture.mPrimaryVehicleRef.mpVehicle   = &sPrimary;
        lFixture.mSecondaryVehicleRef.mpVehicle = &sSecondary;
        lFixture.mHeadingSpaceTransform = Mat(lrRow.mauHeading);          // console this+0x610 after the SLerp
        lFixture.mbForceHeadingSpaceToBeLooseHeadingSpace = (lrRow.miForceLoose != 0);

        unsigned char lacSharedBefore[sizeof(ICE::CameraSpaceHandler)];
        std::memcpy(lacSharedBefore, &sShared, sizeof(sShared));
        FakeSharedInfo lShared = { &sShared };
        const ICE::CameraSpaceHandler lTake = lFixture.RunArm(lShared, &sWorld);

        for (const HandlerMember& lrMember : kaHandlerMembers)
        {
            unsigned lau[16];
            Words(lTake.*(lrMember.mpMember), lau);
            ++luChecks;
            for (int liLane = 0; liLane < 16; ++liLane)
            {
                if (!Same(lau[liLane], lrRow.mauHandler[lrMember.muConsoleOffset / 4u + liLane]))
                {
                    ++luFailures;
                    if (luPrinted++ < 12)
                        std::printf("FAIL  %s: the take's %s (console +0x%03X) row %d.%c = %08X, the console has %08X%s\n",
                                    lrRow.mpcLabel, lrMember.mpcName, lrMember.muConsoleOffset, liLane / 4,
                                    "xyzw"[liLane % 4], lau[liLane], lrRow.mauHandler[lrMember.muConsoleOffset / 4u + liLane],
                                    Same(lau[liLane], lrRow.mauShared[lrMember.muConsoleOffset / 4u + liLane])
                                        ? " (the SHARED handler's value)" : "");
                    break;
                }
            }
        }
        ++luChecks;   // +0x200: the console copies the shared handler's word; the take's mpGamePlayCam is the shared one
        if (lrRow.mauHandler[KU_CONSOLE_GAMEPLAY_CAM_WORD] != lrRow.mauShared[KU_CONSOLE_GAMEPLAY_CAM_WORD]
            || lTake.mpGamePlayCam != sShared.mpGamePlayCam)
        {
            ++luFailures;
            if (luPrinted++ < 12)
                std::printf("FAIL  %s: mpGamePlayCam (console +0x200) is not the shared handler's\n", lrRow.mpcLabel);
        }
        ++luChecks;   // the MainDirector's shared handler is untouched (the console writes only its copy)
        if (std::memcmp(lacSharedBefore, &sShared, sizeof(sShared)) != 0)
        {
            ++luFailures;
            if (luPrinted++ < 12)
                std::printf("FAIL  %s: the arm wrote the SHARED handler\n", lrRow.mpcLabel);
        }
    }
    std::printf("FxLastfixIceSpaces: %u checks, %u failures\n", luChecks, luFailures);
    return luFailures == 0 ? 0 : 1;
}
