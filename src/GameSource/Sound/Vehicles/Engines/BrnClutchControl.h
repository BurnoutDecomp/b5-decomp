#ifndef BRN_SOUND_VEHICLES_ENGINES_CLUTCH_CONTROL_H
#define BRN_SOUND_VEHICLES_ENGINES_CLUTCH_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"   // committed BrnEffectControl dual base (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Engines::ClutchControl  (+ leaf AIClutchControl)
//   GameSource/Sound/Vehicles/Engines/BrnClutchControl.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. ClutchControl is the engine clutch/gear
// sound CONTROL. DWARF (BrnClutchControl.h:233): AIClutchControl : public
// ClutchControl. ClutchControl reuses the committed BrnEffectControl dual base BY NAME
// (its deleting destructor tears down the same dual-vptr pair as the sibling
// TrafficControl); it also (DWARF) inherits ShiftControl::IShiftingActivator, so the
// leaf ctors install a third sub-object vptr @ +0x38 -- produced STRUCTURALLY here by
// the base spine + the virtual dtors.
//
// FLAG (MINIMAL home): this slice bodies ClutchControl's deleting-destructor anchor
// (@ 0x826B36E8) and AIClutchControl's ctor/CreateObject/dtor. AIClutchControl adds NO
// data members (DWARF). ClutchControl's full member surface (mpPhysicsControl/
// mpEngineControl/mpShiftControl, meClutchState, interp/throttle/RPM fields, mRandom,
// the IShiftingActivator interface) is UN-HOMED and DEFERRED; only the base spine
// needed to construct/destroy the leaves is modelled, mirroring the committed
// TrafficControl / CollisionControl minimal-home convention.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// BrnClutchControl.h (DWARF). Reuses the committed BrnEffectControl dual base BY NAME.
struct ClutchControl : public BrnSound::Logic::BrnEffectControl
{
    ClutchControl() {}
    virtual ~ClutchControl();   // anchor for the scalar deleting destructor @ 0x826B36E8
};

// BrnClutchControl.h:233 (DWARF): AIClutchControl : public ClutchControl. No own data
// members; the three leaf vptr installs (primary @+0, IShiftingActivator sub-object
// @+4, further interface base @+0x38) are structural.
struct AIClutchControl : public ClutchControl
{
    AIClutchControl();          // @ 0x826E3F38
    virtual ~AIClutchControl(); // anchor for the scalar deleting destructor @ 0x826E3F98

    // @ 0x826F12E8 -- RTTI factory hook (seeded into ClassTypeInfo<EffectControl>).
    static CgsSound::Logic::EffectControl* CreateObject( u32 luType );
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_CLUTCH_CONTROL_H
