#include "GameSource/Sound/Vehicles/Engines/BrnClutchControl.h"

// =============================================================================
// BrnSound::Vehicles::Engines::ClutchControl / AIClutchControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnClutchControl.h for the base
// rationale + minimal-home FLAG.
//
// Recon'd function set:
//   ClutchControl::`scalar deleting destructor'   @ 0x826B36E8  (-> ~ClutchControl)
//   AIClutchControl::AIClutchControl              @ 0x826E3F38
//   AIClutchControl::CreateObject(u32)            @ 0x826F12E8
//   AIClutchControl::`scalar deleting destructor' @ 0x826E3F98  (-> ~AIClutchControl)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ---------------------------------------------------------------------------
// ~ClutchControl  @ 0x826B36E8  (anchor for the X360 `scalar deleting destructor').
// The inner ~ClutchControl is the inherited BrnEffectControl dual-base settle (both
// vptr stores + the attach/detach/resources-ready clears), the same shape as the
// committed TrafficControl @ 0x826B2580. Compiler-synthesised, so the body is empty.
// FLAG: the (a2 & 1) tail frees via the global sound MemBase allocator (off_82FFB954);
// that allocator is not homed here, so the `delete' half is left to the host toolchain.
// ---------------------------------------------------------------------------
ClutchControl::~ClutchControl()
{
}

// ---------------------------------------------------------------------------
// AIClutchControl::AIClutchControl  @ 0x826E3F38  (default ctor)
//
//   bl   ClutchControl::ClutchControl        ; chain the ClutchControl base ctor
//   stw  &off_820B5A6C, 0x00(this)           ; primary vptr (AIClutchControl vtable)
//   stw  &off_820B5A38, 0x04(this)           ; ShiftControl::IShiftingActivator sub-vptr
//   stw  &off_820B5A2C, 0x38(this)           ; second interface-base sub-vptr @ +0x38
//   return this
//
// The X360 chains the ClutchControl base ctor, then writes the three leaf vpointers.
// AIClutchControl declares NO data members of its own, so every store is a compiler-
// emitted vptr write; the reconstructed body is empty and the three vptr installs are
// produced structurally by the multiple-inheritance + virtual declarations.
// ---------------------------------------------------------------------------
AIClutchControl::AIClutchControl()
    : ClutchControl() // chains the ClutchControl base ctor (+ its IShiftingActivator / @+0x38 sub-objects)
{
}

// ---------------------------------------------------------------------------
// AIClutchControl::CreateObject(u32)  @ 0x826F12E8   (the RTTI factory hook)
//
// The X360 allocates a 448-byte (0x1C0) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "AIClutchControl" and placement-constructs an
// AIClutchControl, handing it back as the EffectControl* base sub-object (the +4
// adjust). `a1` only selects the operator-new flavour (0/1).
//
// FLAG (allocator gate): CgsSound::MemBase does NOT model operator new(size, tag,
// flavour) here, so this uses the host `new`; the observable result matches. Mirrors
// the committed CollisionControl::CreateObject. The 0x1C0 size is documentation only.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* AIClutchControl::CreateObject( u32 /*luType*/ )
{
    return new AIClutchControl();
}

// ---------------------------------------------------------------------------
// ~AIClutchControl  @ 0x826E3F98  (anchor for the X360 `scalar deleting destructor').
// The inner ~ClutchControl is the inherited base settle; compiler-synthesised, so the
// leaf body is empty.
// FLAG: the (a2 & 1) tail frees via the global sound MemBase allocator (off_82FFB954);
// that allocator is not homed here, so the `delete' half is left to the host toolchain.
// ---------------------------------------------------------------------------
AIClutchControl::~AIClutchControl()
{
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
