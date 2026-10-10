// BrnRaceCarEntityModule_wBT_01.cpp -- the module's six tailgating tunables (BrnRaceCarEntityModule.cpp in the
// recovered type information, all `extern float32_t` in namespace BrnWorld).
//
// They are WRITABLE data on the console because RaceCarEntityModuleDebugComponent::OnActivate
// registers each one in the "Tailgate..." debug group, and RenderTailgateCones reads three of
// them back to draw the cones. Values read out of the image at the addresses those two bodies
// load:
//   KF_Z_OFFSET_FROM_CAR_CENTRE_TO_CONE_APEX       2.0f   (initialised data)
//   KF_TAILGATING_CONE_HALF_ANGLE_RADS             0.346f (initialised data)
//   KF_TAILGATING_CONE_DEPTH                       20.0f  (initialised data)
//   KF_MIN_TAILGATE_DURATION                       1.0f   (initialised data)
//   KF_TAILGATING_MAX_RELATIVE_VELOCITY_MAGNITUDE  zero in the image; a CRT dynamic initialiser
//                                                  stores 0.44704f * 90.0f into it (90 mph in m/s)
//   KF_MIN_TAILGATEE_SPEED                         30.0f  (initialised data)
//
// UpdateTailgateTimer / UpdateCarsInTailgateCone in this tree still carry the same values as
// file-local constants of their own TUs, so editing these from the debug menu moves the cones
// the debug component draws but not the module's own tailgate test.

#include "types.hpp"

namespace BrnWorld
{
    namespace
    {
        // CgsCore's miles-per-hour to metres-per-second factor, the operand the initialiser loads.
        const f32 KF_MPH_TO_MPS = 0.44704f;
    }

    f32 KF_Z_OFFSET_FROM_CAR_CENTRE_TO_CONE_APEX      = 2.0f;
    f32 KF_TAILGATING_CONE_HALF_ANGLE_RADS            = 0.346f;
    f32 KF_TAILGATING_CONE_DEPTH                      = 20.0f;
    f32 KF_MIN_TAILGATE_DURATION                      = 1.0f;
    f32 KF_TAILGATING_MAX_RELATIVE_VELOCITY_MAGNITUDE = KF_MPH_TO_MPS * 90.0f;
    f32 KF_MIN_TAILGATEE_SPEED                        = 30.0f;
}
