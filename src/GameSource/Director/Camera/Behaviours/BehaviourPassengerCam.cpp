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
    // stb 9 / 0xA / 0xB / 0xC + stw 4 + stw 0x10 + stb 8 -- the seven stores ARE the
    // inlined Behaviour::Construct (see Behaviour.cpp). Named base call now that the base
    // has a home. (stw 4 stores 0 == E_WORLD under the canonical Timestep::EType.)
    Behaviour::Construct();
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

// Parameters::Construct (h:108, cpp:34). ADDED 2026-09-24 (FX-DIRECTOR). No console symbol:
// BehaviourParameterBank::Construct inlines it once, over the bank's passenger block at +0x21E4
// (0x8223DF08..0x8223DF54). The stores are:
//   stw 0 +0x04 (the base's debug name)
//   the impact block's own Construct over +0x08..+0x20 (0.06 / 0.0 / 1.15 / 0.11, then
//     0.05 / 15.0 / 5.0)
//   stw 7 +0x00 (the type tag SetParameters asserts on)
// Nothing else is written, so the class seed IS the block's content.
void BehaviourPassengerCam::Parameters::Construct()
{
    Behaviour::Parameters::Construct();
    mType = eBehaviourPassengerCam;
    mImpactParams.Construct();
}

// Adopt an authored passenger-cam parameter block: assert the block's type tag (a non-gating
// tripwire -- the stores happen either way), then store the block pointer and its debug name.
void BehaviourPassengerCam::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourPassengerCam,
               "lpParameters->GetType() == eBehaviourPassengerCam");

    mpParameters = lpParameters;                                 // +0x14
    SetDebugParametersName(lpParameters->GetDebugName());        // +0x10
}

// vtable slot 1: a bare `return true`, COMDAT-folded in the console image (hence no symbol of
// its own). The parameter block is not adopted here and the shared info is untouched.
bool BehaviourPassengerCam::Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo)
{
    (void)lrInfo;
    return true;
}

// vtable slot 6: a tail call into Utils::Tweaker::Construct on the SUPPLIED tweaker, folded
// with BehaviourIceAnim::SetupTweaker. The passenger cam resets the tweaker it is handed and
// exposes no tweakable of its own.
void BehaviourPassengerCam::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    lrTweaker.Construct();
}

}
}
