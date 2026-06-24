#ifndef BRN_SOUND_VEHICLES_BRN_3D_CAR_POSITION_H
#define BRN_SOUND_VEHICLES_BRN_3D_CAR_POSITION_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/Brn3DEffectControl.h"

// =============================================================================
// BrnSound::Vehicles::Car3DControl  (+ its Engine/Exhaust/LeftSide/RightSide3dControl
// leaf subclasses)
//   GameSource/Sound/Vehicles/Brn3dCarPosition.h (DWARF home) +
//   GameSource/Sound/Vehicles/Brn3dCarPosition.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Car3DControl is the 3D-positional
// sound-logic control for a player/AI vehicle. DWARF (Brn3dCarPosition.h:34):
//   struct BrnSound::Vehicles::Car3DControl
//       : public BrnSound::Logic::Brn3DEffectControl
// so it inherits the (minimally-homed) Brn3DEffectControl base, which owns the
// Attrib::Instance member (mEngineDataAtrib) the destructor tears down at +176.
// The four leaf controls each derive from Car3DControl and add no data members of
// their own — they specialise only the virtual GetPositionOffset() and the RTTI
// surface (DWARF Brn3dCarPosition.h:84/94/104/114):
//   struct Engine3dControl  : public Car3DControl
//   struct Exhaust3dControl : public Car3DControl
//   struct LeftSide3dControl  : public Car3DControl
//   struct RightSide3dControl : public Car3DControl
//
// This group's recon'd function set is exactly the FIVE (scalar/vector) deleting
// destructors:
//   Car3DControl       `scalar deleting destructor'  @ 0x826CF838
//   Engine3dControl    `vector deleting destructor'  @ 0x826E4D70
//   Exhaust3dControl   `scalar deleting destructor'  @ 0x826E4E18
//   LeftSide3dControl  `scalar deleting destructor'  @ 0x826E4EC0
//   RightSide3dControl `vector deleting destructor'  @ 0x826E4F68
// Every one shares one X360 teardown shape (verified store-for-store against the
// assembly; offsets identical across the five):
//   Attrib::Instance::~Instance(this + 0xB0);   ; destroy mEngineDataAtrib (+176)
//   stw  3,            0x24(this)                ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  &off_820AA820, 0(this)                 ; primary vptr settle
//   stb  0,            0x2D(this)                ; control bookkeeping flag = false
//   stw  0,            0x20(this)                ; mfDeltaTime = 0
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
// The Attrib::Instance member teardown is produced by the inherited
// ~Brn3DEffectControl chain (which destroys mEngineDataAtrib via the committed
// Attrib::Instance::~Instance); the remaining stores settle inherited-base members.
// This mirrors the committed Passby::Passby3DControl home (BrnPassbyEffect.{h,cpp}),
// whose destructor @ 0x826E8ED0 is byte-identical in shape.
//
// FLAG (shape vs full surface): this is a MINIMAL home for the boot-trace deleting
// destructor TUs. The full DWARF surface (RTTI GetTypeInfo/GetTypeName/CreateObject/
// GetStaticTypeInfo hooks; Prepare/Attach/UpdateParams/GetController/AttachController;
// the virtual GetPositionOffset() per leaf; the Car3DControl data members
// mCarPosition (Vector3), mpPhysicsControl (Engines::PhysicsControl*), mbIsStereo)
// is DEFERRED to its own TU(s). Only the inheritance spine needed to settle the
// destructor is materialised here. The (a2 & 1) deallocation tail dispatches the
// global sound allocator (off_82FFB954); that allocator vtable is not homed here,
// so the `delete` half of the X360 deleting destructor is left to the host
// toolchain (same treatment as the committed Passby3DControl / BrnEffectObject
// siblings).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 offsets (mEngineDataAtrib @
// +0xB0, meDetachState @ +0x24, mfDeltaTime @ +0x20, +0x2D bookkeeping flag) assume
// 4-byte pointers/vptr; members are pinned BY NAME via the inherited bases and the
// absolute offsets are NOT static_asserted across pointer members on the 64-bit
// host.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

// Brn3dCarPosition.h:34 (DWARF). Reuses the Brn3DEffectControl base by name; the
// virtual deleting destructor @ 0x826CF838 runs the inherited teardown (incl. the
// Attrib::Instance member via Attrib::Instance::~Instance).
struct Car3DControl : public BrnSound::Logic::Brn3DEffectControl
{
    Car3DControl() {}
    virtual ~Car3DControl();
};

// Brn3dCarPosition.h:84 (DWARF). Engine3dControl : Car3DControl.
struct Engine3dControl : public Car3DControl
{
    Engine3dControl() {}
    virtual ~Engine3dControl();
};

// Brn3dCarPosition.h:94 (DWARF). Exhaust3dControl : Car3DControl.
struct Exhaust3dControl : public Car3DControl
{
    Exhaust3dControl() {}
    virtual ~Exhaust3dControl();
};

// Brn3dCarPosition.h:104 (DWARF). LeftSide3dControl : Car3DControl.
struct LeftSide3dControl : public Car3DControl
{
    LeftSide3dControl() {}
    virtual ~LeftSide3dControl();
};

// Brn3dCarPosition.h:114 (DWARF). RightSide3dControl : Car3DControl.
struct RightSide3dControl : public Car3DControl
{
    RightSide3dControl() {}
    virtual ~RightSide3dControl();
};

} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_BRN_3D_CAR_POSITION_H
