#ifndef BRN_SOUND_VEHICLES_WHEELS_WHEEL_CONTROL_H
#define BRN_SOUND_VEHICLES_WHEELS_WHEEL_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"   // committed BrnEffectControl dual base (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Wheels::WheelControl  (+ leaf AIWheelControl)
//   GameSource/Sound/Vehicles/Wheels/BrnWheelControl.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// WheelControl is the per-car wheel/skid sound CONTROL. DWARF
// (BrnWheelControl.h:129): AIWheelControl : public WheelControl, and WheelControl
// derives from the committed BrnEffectControl (its X360 ctor installs the same
// dual-vptr pair -- primary EffectControl @ this+0, IResourceRequester sub-object
// @ this+4 -- plus a third IShiftingActivator sub-object vptr @ +0x38).
//
// FLAG (MINIMAL home): this slice bodies WheelControl's own vector deleting
// destructor anchor (@ 0x826D00A0), AIWheelControl's ctor (@ 0x826E55A8), and its
// vector deleting destructor anchor (@ 0x826E5608). AIWheelControl adds NO data
// members over WheelControl (DWARF BrnWheelControl.h:129 -- only the RTTI factory +
// overrides). WheelControl's full member surface (WheelSide[2], the road-noise/skid
// effect sub-objects, the IShiftingActivator interface) is UN-HOMED and DEFERRED;
// only the base spine needed to construct/destroy the leaves is modelled here,
// mirroring the committed ClutchControl / TrafficControl minimal-home convention.
// The tri-base vptr installs are produced STRUCTURALLY by the WheelControl base +
// the virtual dtors, not by hand.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

// BrnWheelControl.h (DWARF). The wheel/skid sound control base. Reuses the committed
// BrnEffectControl dual base BY NAME. Only the polymorphic surface needed to
// construct/destroy the AIWheelControl leaf is materialised (full member set
// DEFERRED -- see FLAG).
struct WheelControl : public BrnSound::Logic::BrnEffectControl
{
    WheelControl() {}
    virtual ~WheelControl();   // anchor for the vector deleting destructor @ 0x826D00A0
};

// BrnWheelControl.h:129 (DWARF): AIWheelControl : public WheelControl. Adds no data
// members of its own; the three leaf vptr installs (primary/EffectControl @+0,
// IResourceRequester sub-object @+4, IShiftingActivator sub-object @+0x38) are
// produced structurally by the WheelControl base spine + the virtual ~AIWheelControl.
struct AIWheelControl : public WheelControl
{
    AIWheelControl();                 // @ 0x826E55A8
    virtual ~AIWheelControl();        // anchor for the vector deleting destructor @ 0x826E5608
};

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_WHEELS_WHEEL_CONTROL_H
