#include "GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// ============================================================================
// BrnDirector::Camera::BehaviourPassengerCam -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (DWARF primary file BehaviourPassengerCam.cpp).
//
// Bodied here (4 ledger functions):
//   Construct @0x821F9E48   Update @0x821F9E70
//   Release   @0x821F9EC0   GetName @0x821F9ED8
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

// @ 0x821F9E48 -- the inlined Behaviour::Construct() base-field clear (the X360
// body is exactly the base's store set: flag bytes +9..+C, timestep word +4,
// debug-name +0x10, prepared byte +8; mpParameters is NOT touched).
void BehaviourPassengerCam::Construct()
{
    mbHasFailed             = false;                        // stb 9
    mbTweakerAttached       = false;                        // stb 0xA
    mbCanSwitchToMeNow      = false;                        // stb 0xB
    mbCanSwitchFromMeNow    = false;                        // stb 0xC
    meTimestepType          = BrnDirector::Timestep::E_TIMESTEP_INVALID;   // stw 4
    mpcDebugParametersName  = 0;                            // stw 0x10
    mbIsPrepared            = false;                        // stb 8
}

// @ 0x821F9E70 -- assert the parameter block was adopted, then report success;
// the camera/info args are untouched by the X360 body (BehaviourPassengerCam.cpp:86).
bool BehaviourPassengerCam::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    (void)lrCamera;
    (void)lrInfo;

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");   // :86 (non-gating)
    return true;
}

// @ 0x821F9EC0 -- drop the prepared flag if it is raised; the info arg is untouched.
void BehaviourPassengerCam::Release(const BehaviourSharedPrepareReleaseInfo& lrInfo)
{
    (void)lrInfo;

    if (IsPrepared())
        SetNotPrepared();
}

// @ 0x821F9ED8
const char* BehaviourPassengerCam::GetName() const
{
    return "BehaviourPassengerCam";
}

}
}
