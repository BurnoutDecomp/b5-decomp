// ============================================================================
// GameSource/Director/Camera/Behaviours/BehaviourSpirallingDeathcam.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourSpirallingDeathcam slices this TU set
// owns:
//   - BehaviourSpirallingDeathcam::Prepare @0x821FB3F8  (resets a state field, returns true)
//   - BehaviourSpirallingDeathcam::Start   @0x821F5620  (latches mbStarted)
//
// Prepare is run when the behaviour is installed; Start is run by the crashing/testbed
// arbitrator states (BrnDirector::ArbStateCrashing::Update / ArbStateTestbed::Update) when the
// spiralling deathcam first goes active.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourSpirallingDeathcam.h"

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourSpirallingDeathcam::Prepare @0x821FB3F8
//   Behaviour-faithful: reset the per-activation state field and report prepared OK.
//     *(this + 8) = 0;   // muStateField = 0
//     return 1;
// ----------------------------------------------------------------------------
bool BehaviourSpirallingDeathcam::Prepare()
{
    muStateField = 0;            // stw 0, 8(this)
    return true;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourSpirallingDeathcam::Start @0x821F5620
//   lbz  r11, 0x2F4(this)      ; mbStarted
//   cmplwi r11, 0              ; assert it is clear (!mbStarted)
//   ... on failure: Begin/Fire/End assert ...
//   li   r11, 1
//   stb  r11, 0x2F4(this)      ; mbStarted = 1
// ----------------------------------------------------------------------------
void BehaviourSpirallingDeathcam::Start()
{
    CGS_ASSERT(!mbStarted, "!mbStarted");   // lbz 0x2F4; assert clear
    mbStarted = 1;                          // stb 1, 0x2F4(this)
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourSpirallingDeathcam::SetParameters @0x821F5680
//
// ⭐ ADDED 2026-08-29 (crash-camera wave). ArbStateCrashing::Prepare @0x822655E8 allocates the
// deathcam and immediately hands it the NamedParameters bank's deathcam block, so this was the
// last missing link on that leg.
//   lwz    r11, 0(params)      ; lpParameters->GetType()
//   cmplwi r11, 0x13           ; == eBehaviourSpirallingDeathcam
//   ... assert "lpParameters->GetType() == eBehaviourSpirallingDeathcam" (h:193) ...
//   stw    r31, 0x2D0(this)    ; mpParameters = lpParameters
// The assert is a non-gating tripwire: the console stores the block either way.
// ----------------------------------------------------------------------------
void BehaviourSpirallingDeathcam::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourSpirallingDeathcam,
               "lpParameters->GetType() == eBehaviourSpirallingDeathcam");   // h:193

    mpParameters = lpParameters;   // stw 0x2D0(this)
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourSpirallingDeathcam::Parameters::Construct @0x821FB498
//
// Seed the authored block to its defaults. Store order in the asm is scheduler-shuffled; the
// displacements are what pin each value, and they are written here in ascending offset.
// The Looker sub-block is seeded by its own Construct (asm: the one `bl` in this function,
// on this+0x08); the shake quad is the project-wide 0.06 / 0.0 / 1.15 / 0.11 seed.
// ----------------------------------------------------------------------------
void BehaviourSpirallingDeathcam::Parameters::Construct()
{
    meType        = eBehaviourSpirallingDeathcam;   // stw 0x13, 0(this)
    miBaseField04 = 0;                              // stw 0,    4(this)

    mLookerParams.Construct();                      // bl Looker::Parameters::Construct(this+8)
    mShakeParams.Construct();                       // stfs 0.06 / 0.0 / 1.15 / 0.11 @+0x6C..+0x78

    mfRotationSpeedDegs             = 60.0f;   // stfs flt_82004C6C, 0x7C(this)
    mfHeightIncreaseSpeed           =  0.5f;   // stfs flt_82001DA0, 0x80(this)
    mfRadiusIncreaseSpeed           =  0.0f;   // stfs flt_82001CC0, 0x84(this)
    mfInitialHeight                 =  0.5f;   // stfs flt_82001DA0, 0x88(this)
    mfInitialRadius                 =  1.5f;   // stfs flt_82004D04, 0x8C(this)
    mfTimeBeforeFullLookOffsetSpeed =  4.0f;   // stfs flt_82004EF4, 0x90(this)
    mfLookOffsetSpeed               =  1.0f;   // stfs flt_82001C98, 0x94(this)
    mfHeightIncreaseDuration        = 15.0f;   // stfs flt_820047C4, 0x98(this)
    mfRotationSpeedDecreaseRate     =  5.0f;   // stfs flt_8200426C, 0x9C(this)
    mfMinRotationSpeed              = 20.0f;   // stfs flt_820054CC, 0xA0(this)
    mfMinAttachAmount               =  0.8f;   // stfs flt_820054C8, 0xA4(this)
    mfBlurTime                      =  7.0f;   // stfs flt_820054D0, 0xA8(this)
    mfShakeTime                     =  7.0f;   // stfs flt_820054D0, 0xAC(this)
}

} // namespace Camera
} // namespace BrnDirector
