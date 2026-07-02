#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourDebugFlyWorld.h
//
// BrnDirector::Camera::BehaviourDebugFlyWorld -- the debug "fly world" camera
// behaviour the arbitrator's dev tooling drives. MINIMAL SLICE: only the warp
// entry point Arbitrator::DebugCameraFlyWorldLookAt @0x82234738 calls (the
// GameTalk "CameraPos" command's landing spot). The behaviour's own rig
// (Construct/Update/the fly control state) lands with its own TU -- GROW in
// place. DECLARATION-ONLY: the per-TU `cl /c` gate does not link.
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

class BehaviourDebugFlyWorld
{
public:
    // Snap the fly camera to lEye looking at lLookAt (both world-space; the X360
    // passes the two Vector3s in VMX registers straight through the arbitrator
    // wrapper). Own ledger function -- declared for the wrapper only.
    void WarpToLookAt(Vector3 lEye, Vector3 lLookAt);
};

}
}
