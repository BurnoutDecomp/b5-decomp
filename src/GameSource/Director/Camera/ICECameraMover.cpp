#include "GameSource/Director/Camera/ICECameraMover.h"

// ============================================================================
// GameSource/Director/Camera/ICECameraMover.cpp
//
// ICE::ICECameraMover default constructor. The mover is embedded by value in
// BrnDirector::ICEWrapper, so it must be default-constructible; the real per-frame
// state is set by ICECameraMover::Construct (its own TU, SDKs/Packages/ICE/
// ICECameraMover.cpp). The ASM (0x827DB878) only default-constructs the two Cubic3D
// followers (mAccelOffset @+0x008, mForward @+0x08C, 132 bytes each -- three Cubic1D
// default-ctors per block); it does NOT touch mpCar/mpICECamera/mpTake or the scalar
// hysteresis fields (miHardCutInterval/mfSimTime/muOldTag/miOldOverlay), which are left
// uninitialized until Construct runs. Do not add initializers for those fields here --
// that would be behavior the binary does not have.
// ============================================================================

namespace ICE
{
ICECameraMover::ICECameraMover()
{
}
}
