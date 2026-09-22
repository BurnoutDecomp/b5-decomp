// FX-AIMOD (crash parity 2026-09-22), G08-D2 + G07-D3: the player's camera reaches
// ResetOnTrackManager and PlayerIsLookingBackwards reads it.
//   ResetOnTrackManager::Update @0x8279A890: `mr r30, r7` (the by-value Camera), then after
//     `stw r31, 0x384(r25)` (the player index) `addi r3, r25, 0x390 ; bl Camera::operator=`
//     (0x8279A8E8..0x8279A8F0).
//   ResetOnTrackManager::PlayerIsLookingBackwards @0x82778000: `lvx128 v127, this, 0x3B0`
//     (mCamera + 0x20 == mTransform.At()), AICar::GetDirection, vmsum3fp128, `fcmpu ; blt` vs 0.0.
// The runner extracts both PRODUCTION bodies, the Strategies.cpp helper namespace, GetAICar and
// the Director camera's copy constructor + GetDirection (Camera.cpp). The request pump's leaves
// are observed fixtures. A pre-fix Update (three parameters) is compiled with the camera
// parameter appended and ignored, so the RED side measures what the old body did with it: nothing.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned gAssertions = 0, gChecks = 0, gFailures = 0;
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcText, const char*, int) { ++gAssertions; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsResource {
BaseResourcePtr::BaseResourcePtr() {}
BaseResourcePtr::~BaseResourcePtr() {}
}

static int gProcessed = 0;
namespace BrnAI {
Vector3 AICar::GetDirection() const { return mDirection; }
void ResetOnTrackManager::ProcessResetOnTrackRequest(const AIModuleIO::ResetOnTrackRequest*, AIModuleResultInterface*, f32) { ++gProcessed; }
bool ResetOnTrackManager::UpdateResetOnTrackSectionUsingRoute(AICar*) { return true; }
void ResetOnTrackManager::UpdateResetOnTrackSectionUsingCurrentSection(AICar*) {}
}

#include "restored_methods.inc"

using namespace BrnAI;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass) { ++gFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

static AICar saCars[35];
static ResetOnTrackManager sManager;

static BrnDirector::Camera::Camera MakeCamera(f32 lfAtX, f32 lfAtY, f32 lfAtZ, f32 lfPosX)
{
    BrnDirector::Camera::Camera lCamera{};
    lCamera.mTransform.At()  = Vector3{ lfAtX, lfAtY, lfAtZ, 0.0f };
    lCamera.mTransform.Pos() = Vector3{ lfPosX, 2.0f, 3.0f, 1.0f };
    lCamera.mfFOV = 1.25f;
    return lCamera;
}

int main()
{
    sManager.mResetOnTrackRequestQueue.Construct();
    sManager.mRecentResets.Construct();
    sManager.mpaAICars = saCars;
    sManager.mePlayerGlobalRaceCarIndex = E_GLOBAL_RACE_CAR_INDEX_0;
    saCars[0].mDirection = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };   // the player drives +Z
    std::memset(&sManager.mCamera, 0, sizeof(sManager.mCamera));

    // (1) The camera looks back down the road: Update copies it, and the player IS looking back.
    sManager.Update(nullptr, E_GLOBAL_RACE_CAR_INDEX_0, 1.0f, MakeCamera(0.0f, 0.0f, -1.0f, 11.0f));
    Check(sManager.mCamera.mTransform.At().z == -1.0f && sManager.mCamera.mTransform.Pos().x == 11.0f &&
          sManager.mCamera.mfFOV == 1.25f, "Update copies the whole camera into mCamera (0x8279A8F0)");
    Check(sManager.PlayerIsLookingBackwards(), "camera At (0,0,-1) vs heading (0,0,1): looking backwards");

    // (2) The next frame's camera replaces it: looking ahead-ish (dot 0.8).
    sManager.Update(nullptr, E_GLOBAL_RACE_CAR_INDEX_0, 1.1f, MakeCamera(0.6f, 0.0f, 0.8f, 12.0f));
    Check(sManager.mCamera.mTransform.At().x == 0.6f && sManager.mCamera.mTransform.Pos().x == 12.0f,
          "every Update refreshes mCamera");
    Check(!sManager.PlayerIsLookingBackwards(), "dot 0.8: not looking backwards");

    // (3) Side-on (dot exactly 0): `blt` is strict, so NOT looking backwards.
    sManager.Update(nullptr, E_GLOBAL_RACE_CAR_INDEX_0, 1.2f, MakeCamera(1.0f, 0.0f, 0.0f, 13.0f));
    Check(!sManager.PlayerIsLookingBackwards(), "dot 0.0: not looking backwards (fcmpu ; blt)");

    // (4) Over the shoulder (dot -0.6): looking backwards; the Y lane counts too (3-lane dot).
    sManager.Update(nullptr, E_GLOBAL_RACE_CAR_INDEX_0, 1.3f, MakeCamera(0.8f, 0.0f, -0.6f, 14.0f));
    Check(sManager.PlayerIsLookingBackwards(), "dot -0.6: looking backwards");
    saCars[0].mDirection = Vector3{ 0.0f, 0.6f, 0.8f, 0.0f };
    sManager.Update(nullptr, E_GLOBAL_RACE_CAR_INDEX_0, 1.4f, MakeCamera(0.0f, -1.0f, 0.5f, 15.0f));
    Check(sManager.PlayerIsLookingBackwards(), "vmsum3fp128 includes Y: 0.6*-1 + 0.8*0.5 = -0.2 < 0");

    Check(gProcessed == 0, "no request was queued, none processed");
    Check(gAssertions == 0, "valid fixtures raise no assertion");
    std::printf("AIModCamera: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
