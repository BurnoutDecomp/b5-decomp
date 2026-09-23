// FX-VEHPHYS (crash parity 2026-09-23, G31-D1): BrnPhysics::ExternalPhysicsBody::CheckState
// @0x825A24B0 -- which LANES the console's NaN sweep tests.
//
// run_fxvehphys_check_state_lanes.py EXTRACTS the production anonymous-namespace helpers
// (IsLaneNaN / AnyNaN3 / AnyNaN4 / CheckStateFail) and the production CheckState body from
// ExternalPhysicsBody.cpp and compiles them into CheckStateFixture, a struct that carries the ten
// members the sweep reads with their real types. The assert and DebugPrint sinks are captures.
//
// Console facts checked (ARTIST asm):
//   0x825A29DC li r11,0xD0 ; 0x825A29E4 lvx128 v0,r26,r11 ; 0x825A29E8 vcmpeqfp. v0,v0,v0
//              -> mfMass is the ONE whole-register test (CR6 all-true over four lanes).
//   0x825A2A34 li r11,0xE0 ; lvx128 ; 0x825A2A40/2A58/2A74 vspltw v,v0,0/1/2 each + vcmpeqfp.
//              -> mTotalLinearForce is tested on x, y, z only; lane 3 is never splatted.
//   The same three-lane ladder: +0xF0 mTotalTorque (0x825A2AE8..2B24), +0x100 mTotalLinearImpulse
//   (0x825A2B90..2BCC), +0x110 mTotalAngularImpulse (0x825A2C38..2C74), +0x40 mLinearVelocity
//   (0x825A2CE0..2D1C), +0x50 mAngularVelocity (0x825A2D88..2DC4).
//   Each failure: `ld gxMessageFilterFlags ; clrldi 63` gates `gpDebugPrint << context`, then
//   BeginAssert / FireAssert("<Bad ...>") / EndAssert.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Physics/PhysicsUtilities/ExternalPhysicsBody.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static std::vector<std::string> gAsserts;
static std::string gLog;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int) { gAsserts.emplace_back(lpcMessage); return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Message { u64 gxMessageFilterFlags = 1; }
namespace Log
{
    // Harness-only sink: the game's DebugPrint writes to the log file; this one appends to gLog.
    StrStreamBase& DebugPrint::operator<<(const char* lpcText) { gLog += lpcText; return *this; }
    void WriteToLog(const char*) {}
    static DebugPrint sCapture;
    DebugPrint* gpDebugPrint = &sCapture;
}
}

// The production StrStreamBase (DebugPrint's base) so the sink's vtable links.
#include "GameShared/GameClasses/Development/CgsStrStream.cpp"

namespace BrnPhysics
{
#include "check_state_helpers.inc"

    struct CheckStateFixture
    {
        decltype(ExternalPhysicsBody::mTransform)           mTransform;
        decltype(ExternalPhysicsBody::mLocalInverseInertia) mLocalInverseInertia;
        decltype(ExternalPhysicsBody::mWorldInverseInertia) mWorldInverseInertia;
        decltype(ExternalPhysicsBody::mfMass)               mfMass;
        decltype(ExternalPhysicsBody::mTotalLinearForce)    mTotalLinearForce;
        decltype(ExternalPhysicsBody::mTotalTorque)         mTotalTorque;
        decltype(ExternalPhysicsBody::mTotalLinearImpulse)  mTotalLinearImpulse;
        decltype(ExternalPhysicsBody::mTotalAngularImpulse) mTotalAngularImpulse;
        decltype(ExternalPhysicsBody::mLinearVelocity)      mLinearVelocity;
        decltype(ExternalPhysicsBody::mAngularVelocity)     mAngularVelocity;

        void CheckState(const char* lpcContext) const;
    };

#include "check_state_method.inc"
}

using BrnPhysics::CheckStateFixture;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const std::string& lrName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lrName.c_str()); }
}

static f32 Bits(u32 luBits) { f32 lf; std::memcpy(&lf, &luBits, sizeof lf); return lf; }

static CheckStateFixture Clean()
{
    CheckStateFixture lBody;
    std::memset(&lBody, 0, sizeof lBody);
    lBody.mTransform.xAxis.x = 1.0f; lBody.mTransform.yAxis.y = 1.0f; lBody.mTransform.zAxis.z = 1.0f;
    lBody.mTransform.wAxis.x = 1000.0f; lBody.mTransform.wAxis.y = 50.0f; lBody.mTransform.wAxis.z = -2000.0f;
    lBody.mLocalInverseInertia.xAxis.x = lBody.mLocalInverseInertia.yAxis.y = lBody.mLocalInverseInertia.zAxis.z = 0.001f;
    lBody.mWorldInverseInertia = lBody.mLocalInverseInertia;
    lBody.mfMass.x = lBody.mfMass.y = lBody.mfMass.z = lBody.mfMass.w = 1500.0f;
    lBody.mLinearVelocity.x = 12.0f; lBody.mLinearVelocity.z = -30.0f;
    lBody.mAngularVelocity.y = 0.5f;
    return lBody;
}

static void Run(const CheckStateFixture& lrBody, u64 lxFlags = 1)
{
    gAsserts.clear();
    gLog.clear();
    CgsDev::Message::gxMessageFilterFlags = lxFlags;
    lrBody.CheckState("[stage]");
}

int main()
{
    struct Member { Vector3 CheckStateFixture::* mpVector; const char* mpcWhat; };
    const Member laMembers[] = {
        { &CheckStateFixture::mTotalLinearForce,   "Bad total linear force" },
        { &CheckStateFixture::mTotalTorque,        "Bad total torque" },
        { &CheckStateFixture::mTotalLinearImpulse, "Bad total linear impulse" },
        { &CheckStateFixture::mTotalAngularImpulse,"Bad total angular impulse" },
        { &CheckStateFixture::mLinearVelocity,     "Bad linear velocity" },
        { &CheckStateFixture::mAngularVelocity,    "Bad angular velocity" },
    };
    const u32 kauNaNs[] = { 0x7FC00000u, 0xFFFFFFFFu };   // the quiet NaN and the all-ones pattern

    // A clean body asserts nothing and prints nothing.
    Run(Clean());
    Check(gAsserts.empty() && gLog.empty(), "clean body: no assert, no stage print");

    // (1) A NaN ONLY in lane w of a Vector3 member: the console never splats lane 3 -> silence.
    for (const Member& lrMember : laMembers)
        for (u32 luNaN : kauNaNs)
        {
            CheckStateFixture lBody = Clean();
            (lBody.*lrMember.mpVector).w = Bits(luNaN);
            Run(lBody);
            char lacLabel[160];
            std::snprintf(lacLabel, sizeof lacLabel, "w-only NaN 0x%08X in '%s' member: console asserts nothing",
                          luNaN, lrMember.mpcWhat);
            Check(gAsserts.empty() && gLog.empty(), lacLabel);
        }

    // (2) A NaN in lane x, y or z of a Vector3 member: exactly that member's assert, once.
    for (const Member& lrMember : laMembers)
        for (int liLane = 0; liLane < 3; ++liLane)
        {
            CheckStateFixture lBody = Clean();
            Vector3& lrVector = lBody.*lrMember.mpVector;
            (liLane == 0 ? lrVector.x : liLane == 1 ? lrVector.y : lrVector.z) = Bits(0x7FC00000u);
            Run(lBody);
            char lacLabel[160];
            std::snprintf(lacLabel, sizeof lacLabel, "lane %d NaN fires exactly one '%s'", liLane, lrMember.mpcWhat);
            Check(gAsserts.size() == 1 && gAsserts[0] == lrMember.mpcWhat && gLog == "[stage]", lacLabel);
        }

    // (3) mfMass is the whole-register test: a NaN in ANY lane, w included, is "Bad mass".
    {
        CheckStateFixture lBody = Clean();
        lBody.mfMass.w = Bits(0xFFFFFFFFu);
        Run(lBody);
        Check(gAsserts.size() == 1 && gAsserts[0] == "Bad mass", "mfMass w-only NaN is one 'Bad mass' (0x825A29E8 vcmpeqfp. v0,v0,v0)");
        lBody = Clean();
        lBody.mfMass.x = Bits(0x7FC00000u);
        Run(lBody);
        Check(gAsserts.size() == 1 && gAsserts[0] == "Bad mass", "mfMass x NaN is one 'Bad mass'");
    }

    // (4) The stage print is gated on gxMessageFilterFlags bit 0; the assert is not.
    {
        CheckStateFixture lBody = Clean();
        lBody.mAngularVelocity.y = Bits(0x7FC00000u);
        Run(lBody, 0);
        Check(gAsserts.size() == 1 && gLog.empty(), "flags bit 0 clear: the assert fires, the stage string is not printed");
        Run(lBody, 1);
        Check(gAsserts.size() == 1 && gLog == "[stage]", "flags bit 0 set: the stage string is printed once");
    }

    std::printf("FxVehphysCheckStateLanes: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
