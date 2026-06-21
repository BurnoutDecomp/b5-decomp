#include "GameSource/Director/Camera/ICECameraMover.h"

// ============================================================================
// GameSource/Director/Camera/ICECameraMover.cpp
//
// ICE::ICECameraMover default constructor. The mover is embedded by value in
// BrnDirector::ICEWrapper, so it must be default-constructible; the real per-frame
// state is set by ICECameraMover::Construct (its own TU, SDKs/Packages/ICE/
// ICECameraMover.cpp). The ctor leaves the cubic followers / transform / vectors to
// their members' own default-init (each Cubic1D default-constructs to its rest state)
// and clears the pointers + scalar hysteresis so an un-Construct'd mover is inert.
// ============================================================================

namespace ICE
{
ICECameraMover::ICECameraMover()
    : mpCar(0)
    , mpICECamera(0)
    , mpTake(0)
    , miHardCutInterval(-1)
    , mfSimTime(0.0f)
    , muOldTag(0)
    , miOldOverlay(0)
{
}
}
