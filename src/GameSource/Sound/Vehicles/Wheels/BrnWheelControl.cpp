#include "GameSource/Sound/Vehicles/Wheels/BrnWheelControl.h"

// =============================================================================
// BrnSound::Vehicles::Wheels::AIWheelControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnWheelControl.h for the
// WheelControl base rationale and the minimal-home FLAG. This TU's recon'd function
// set is two entries:
//   AIWheelControl()               @ 0x826E55A8
//   `vector deleting destructor'   @ 0x826E5608  (-> ~AIWheelControl anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

// ---------------------------------------------------------------------------
// AIWheelControl::AIWheelControl  @ 0x826E55A8
//
//   bl   WheelControl::WheelControl(this)      ; base sub-object ctor
//   stw  off_820B5CDC, 0(this)                 ; primary (EffectControl) leaf vptr @ +0
//   stw  off_820B5CA8, 4(this)                 ; IResourceRequester sub-object leaf vptr @ +4
//   stw  off_820B5C9C, 0x38(this)              ; IShiftingActivator sub-object leaf vptr @ +0x38
//   return this
//
// MSVC full-object ctor: `bl`s WheelControl (the base sub-object construction, which
// installs the shared EffectControl/IResourceRequester/IShiftingActivator bases),
// then installs the three AIWheelControl leaf vptrs. In reconstructed C++ the base is
// the WheelControl() mem-init; the three leaf vptr installs are produced structurally
// by the base spine + the virtual ~AIWheelControl declaration. AIWheelControl adds no
// members.
// ---------------------------------------------------------------------------
AIWheelControl::AIWheelControl()
    : WheelControl()   // `bl WheelControl::WheelControl' + the shared bases (BY NAME)
{
    // The three AIWheelControl leaf vptr installs (primary @ +0, IResourceRequester
    // sub-object @ +4, IShiftingActivator sub-object @ +0x38) are produced structurally
    // by the WheelControl tri-base spine + the virtual ~AIWheelControl declaration.
}

// ---------------------------------------------------------------------------
// ~AIWheelControl  @ 0x826E5608  (the X360 `vector deleting destructor')
//
// Same teardown as the committed TrafficControl deleting destructor (BrnEffectControl
// base member clears) PLUS the extra IShiftingActivator (+0x38) vptr settle from
// WheelControl's third base. Every stored member is owned by the inherited
// BrnEffectControl base; this leaf destructor adds nothing of its own.
// FLAG: the (a2 & 1) tail routes the object through the global sound allocator
// (off_82FFB954, vtable slot +0x14 == Free); that allocator is not homed here, so the
// `delete` half of the X360 deleting-destructor thunk is left to the host toolchain's
// operator delete.
// ---------------------------------------------------------------------------
AIWheelControl::~AIWheelControl()
{
}

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound
