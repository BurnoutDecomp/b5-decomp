// L1 (owner's list 2026-09-28, item "camera behind walls / below the map"): the collision tuning two behaviour Constructs
// give their car-attached policy, against the console's own (tests/L1ConstructFlagsData.h, the ARTIST words run on
// emu64 by scratch/OWNERLIST_0927/L1/emu/gen_construct_flags_l1.py, from a pattern-filled behaviour):
//   BehaviourGameplayExternal::Construct @0x82224A18 -- after the policy's Construct (0x82224AB0): stb 0 +0x298
//       (0x82224AF8, mbAutoElevate), stb 1 +0x29C (0x82224B34, mbTestAgainstWorldOnly), stb 1 +0x29D (0x82224B3C,
//       mbUseFrustrumResolver), stb 1 +0x299 (0x82224B44, mbSmoothRadiusChanges) -- the policy at +0x50
//   BehaviourIceAnim::Construct @0x82256100 -- after the policy's Construct (0x82256260): stb 1 +0x4AC (0x82256264,
//       mbTestAgainstWorldOnly), stb 1 +0x4AD (0x82256268, mbUseFrustrumResolver) -- the policy at +0x260
// run_l1_construct_flags.py extracts the revision's two Construct bodies into l1_construct_flags.inc; each runs on a
// pattern-filled behaviour (its members' out-of-line Constructs are empty stubs below -- none touches the policy).
//   K1 / K3  the policy's eight bools, byte for byte (mbResetVehicleCollision: the pattern -- nobody stores it)
//   K2 / K4  its two tail floats (mfDesiredNearClip, mfMaxRadius)
#include <cstdio>
#include <cstring>
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"

#include "L1ConstructFlagsData.h"

static int giAsserts = 0;
namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++giAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

// The out-of-line Constructs the two bodies call; their homes are TUs this test does not link, and none of them
// touches the car-attached policy.
namespace BrnDirector
{
namespace Camera
{
    void Behaviour::Construct() {}
    void Camera::Construct() {}
    void VisibilityCollisionPolicy::Construct() {}
    void Utils::CameraSphericalRotationController::Construct() {}

#include "l1_construct_flags.inc"
}
}

static int giChecks = 0;
static int giFailures = 0;

static void Check(bool lbOk, const char* lpcName)
{
    ++giChecks;
    if (!lbOk)
        ++giFailures;
    std::printf("%s  %s\n", lbOk ? "PASS" : "FAIL", lpcName);
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }

static void CheckPolicy(const BrnDirector::Camera::CollisionPolicyAttachedToVehicle& lrPolicy,
                        const L1ConstructFlags& lrConsole, const char* lpcBools, const char* lpcFloats)
{
    const u8 lauPc[8] = { lrPolicy.mbAutoElevate, lrPolicy.mbSmoothRadiusChanges, lrPolicy.mbFailOnContact,
                          lrPolicy.mbUseGroundConstraint, lrPolicy.mbTestAgainstWorldOnly,
                          lrPolicy.mbUseFrustrumResolver, lrPolicy.mbResetVehicleCollision,
                          lrPolicy.mbDoVehicleCollision };
    static const char* const kapcNames[8] = { "mbAutoElevate", "mbSmoothRadiusChanges", "mbFailOnContact",
                                              "mbUseGroundConstraint", "mbTestAgainstWorldOnly",
                                              "mbUseFrustrumResolver", "mbResetVehicleCollision",
                                              "mbDoVehicleCollision" };
    int liBad = 0;
    for (int i = 0; i < 8; ++i)
    {
        if (lauPc[i] != lrConsole.mauBools[i])
        {
            std::printf("  %s policy %s: console %02X pc %02X\n", lrConsole.mpcBehaviour, kapcNames[i],
                        lrConsole.mauBools[i], lauPc[i]);
            ++liBad;
        }
    }
    char lacName[200];
    std::snprintf(lacName, sizeof(lacName), "%s %s::Construct @0x%08X leaves the policy's eight bools as the console's "
                  "(%d wrong)", lpcBools, lrConsole.mpcBehaviour, lrConsole.muAddress, liBad);
    Check(liBad == 0, lacName);
    const bool lbFloats = Bits(lrPolicy.mfDesiredNearClip) == lrConsole.mauFloats[0]
                       && Bits(lrPolicy.mfMaxRadius) == lrConsole.mauFloats[1];
    std::snprintf(lacName, sizeof(lacName), "%s %s: mfDesiredNearClip / mfMaxRadius as the console's (%08X %08X, pc %08X "
                  "%08X)", lpcFloats, lrConsole.mpcBehaviour, lrConsole.mauFloats[0], lrConsole.mauFloats[1],
                  Bits(lrPolicy.mfDesiredNearClip), Bits(lrPolicy.mfMaxRadius));
    Check(lbFloats, lacName);
}

alignas(16) static u8 gaGameplay[sizeof(BrnDirector::Camera::BehaviourGameplayExternal)];
alignas(16) static u8 gaIceAnim[sizeof(BrnDirector::Camera::BehaviourIceAnim)];

int main()
{
    // Construct is called on the raw pattern: nothing is value-initialised first, so a byte no store reaches keeps the
    // pattern, as on the console.
    std::memset(gaGameplay, KU_L1_FLAGS_PATTERN, sizeof(gaGameplay));
    BrnDirector::Camera::BehaviourGameplayExternal* lpGameplay =
        reinterpret_cast<BrnDirector::Camera::BehaviourGameplayExternal*>(gaGameplay);
    lpGameplay->BehaviourGameplayExternal::Construct();
    CheckPolicy(lpGameplay->mCollisionPolicy, kaL1ConstructFlags[0], "K1", "K2");

    std::memset(gaIceAnim, KU_L1_FLAGS_PATTERN, sizeof(gaIceAnim));
    BrnDirector::Camera::BehaviourIceAnim* lpIceAnim = reinterpret_cast<BrnDirector::Camera::BehaviourIceAnim*>(gaIceAnim);
    lpIceAnim->BehaviourIceAnim::Construct();
    CheckPolicy(lpIceAnim->mAttachedToCarCollisionPolicy, kaL1ConstructFlags[1], "K3", "K4");

    std::printf("L1ConstructFlags: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
