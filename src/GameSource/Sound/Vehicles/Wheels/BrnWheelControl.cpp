#include "GameSource/Sound/Vehicles/Wheels/BrnWheelControl.h"

// =============================================================================
// BrnSound::Vehicles::Wheels::WheelControl / AIWheelControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnWheelControl.h for the
// WheelControl base rationale and the minimal-home FLAG. This TU's recon'd function
// set is three entries:
//   WheelControl::`vector deleting destructor'    @ 0x826D00A0  (-> ~WheelControl anchor)
//   AIWheelControl::AIWheelControl                @ 0x826E55A8
//   AIWheelControl::`vector deleting destructor'  @ 0x826E5608  (-> ~AIWheelControl anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

// ---------------------------------------------------------------------------
// ~WheelControl  @ 0x826D00A0  (the X360 `vector deleting destructor')
//
//   stw  off_820AF544, 0x38(r31)   ; IShiftingActivator sub-object vptr @ +0x38
//   stw  off_820AEA6C, 0(r31)      ; primary vptr (EffectControl path)
//   stw  off_820AEA38, 4(r31)      ; (transient) base-class IResourceRequester vptr
//   li   r6, 3 ; stw r6, 0x28(r31) ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  off_820AA820, 4(r31)      ; final IResourceRequester sub-object vptr
//   stb  0, 0x31(r31)              ; mbResourcesReady = false
//   stw  0, 0x24(r31)              ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { ... deallocate via off_82FFB954 (the MemBase allocator) }
//   return this
//
// Identical inherited-member teardown to the committed BrnEffectControl vector
// deleting destructor (@ 0x826AEF68) -- meDetachState (+0x28), mbResourcesReady
// (+0x31), meAttachState (+0x24) -- plus the leading vptr stores, which are the
// compiler-emitted devirtualization of the WheelControl / BrnEffectControl /
// IResourceRequester / IShiftingActivator base sub-objects. The body is empty because
// the materialised in-air modifiers are value members with compiler-generated
// teardown; the wider WheelSide/WheelData producer surface remains in its own slice.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain (the `delete` half of the X360 vector deleting destructor),
// mirroring the committed BrnEffectControl / ClutchControl / AIWheelControl siblings.
// ---------------------------------------------------------------------------
WheelControl::~WheelControl()
{
}

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
