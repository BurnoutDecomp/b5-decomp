// ============================================================================
// GameSource/Director/Camera/Utils/BrnCameraTweakerConstruct.cpp
//
// [marked deviation -- FILE SPLIT, not a code change] BrnDirector::Camera::Utils::
// Tweaker::Construct @0x821F8588, moved here VERBATIM from its sibling
// BrnCameraTweaker.cpp on 2026-08-01. On the console it lives in that one TU with the
// other four tweaker bodies.
//
// ⭐ WHY THE SPLIT. Construct is the ONLY tweaker symbol the camera-behaviour family needs
// (BehaviourIceAnim::SetupTweaker and the arbitrator states reach it; nothing on that path
// renders or updates a tweaker), and it is completely self-contained -- it touches only its
// own binding arrays and its own mbHideInstructions byte, calls nothing, and reads no
// rodata. The rest of BrnCameraTweaker.cpp is not: MEASURED on 2026-08-01, mounting the
// whole TU closes ONE unresolved external and opens FIVE --
//     BrnDirector::Camera::Utils::KAAC_AXIS_NAMES        (extern rodata, no data TU)
//     BrnDirector::Camera::Utils::KAAC_CONTROL_NAMES     (extern rodata, no data TU)
//     Camera::Utils::DebugController::GetControllerInfo
//     CgsDev::DebugInterface::Get2dRender
//     CgsDev::DebugRender::Draw2DTextJustified
// -- all of them the debug RENDER path. Splitting adds nothing to the link and invents
// nothing; the four remaining bodies keep their real implementations for the day the debug
// render layer lands.
//
// DELETE-WHEN: the debug-render leaves above land -> mount BrnCameraTweaker.cpp and merge
// this body back into it. The split has no reason to outlive the blocker.
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"

namespace BrnDirector
{
    namespace Camera
    {
        namespace Utils
        {
            // @0x821F8588. Reset the tweaker to an empty, "instructions-shown" state.
            // The X360 walks the mbUsed flag of every binding (axis stride 20,
            // control stride 16, the two control arrays 1056 bytes apart) and clears
            // it, then clears mbHideInstructions. Nothing else is touched -- AddMapping
            // fully overwrites a slot when it is (re)used.
            void Tweaker::Construct()
            {
                for (s32 liMap = 0; liMap < E_MAP_COUNT; ++liMap)
                {
                    for (s32 liAxis = 0; liAxis < E_AXIS_COUNT; ++liAxis)
                    {
                        maAxisMapping[liMap][liAxis].mbUsed = false;
                    }

                    for (s32 liControl = 0; liControl < DebugController::E_CONTROL_COUNT; ++liControl)
                    {
                        mJustPressedMapping[liMap][liControl].mbUsed  = false;
                        mJustReleasedMapping[liMap][liControl].mbUsed = false;
                    }
                }

                mbHideInstructions = false;
            }
        }
    }
}
