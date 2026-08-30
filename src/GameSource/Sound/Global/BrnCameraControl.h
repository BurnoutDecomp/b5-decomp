#ifndef BRN_SOUND_LOGIC_BRN_CAMERA_CONTROL_H
#define BRN_SOUND_LOGIC_BRN_CAMERA_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"

// =============================================================================
// BrnSound::Logic::CameraControl
//   GameSource/Sound/Global/BrnCameraControl.h (DWARF home) +
//   GameSource/Sound/Global/BrnCameraControl.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. CameraControl is a sound-logic
// camera effect *control*. DWARF (BrnCameraControl.h:35):
//   struct BrnSound::Logic::CameraControl : public BrnSound::Logic::BrnEffectControl
// so it inherits the COMMITTED BrnEffectControl dual base (EffectControl primary
// vptr @ this+0, IResourceRequester sub-object vptr @ this+4) reused BY NAME from
// GameSource/Sound/Module/LogicModule/BrnEffectControl.h.
//
// This TU's recon'd function set is exactly ONE entry:
//   `vector deleting destructor' adjustor{4}  @ 0x826BB438
// which is the multiple-inheritance thunk reached through the IResourceRequester
// second base. Its X360 body is:
//   addi r3, r3, -4 ; b CameraControl::`scalar deleting destructor'
// i.e. recover the primary object (this-4) and forward to the real destructor.
// In reconstructed C++ this thunk is COMPILER-GENERATED from the dual-base vtable
// layout of BrnEffectControl (the virtual destructor is reached through the
// IResourceRequester base), so no hand-written body is emitted — declaring the
// virtual destructor on this leaf is what produces the adjustor thunk.
//
// FLAG (shape vs full surface): this is a MINIMAL home for the boot-trace
// CameraControl TU. The full DWARF surface (mCameraMode DataPoint<ECameraModes>,
// GetCameraMode / GetCameraModeFromLogicModule / GetEventSnapshot, the RTTI
// GetTypeInfo/GetTypeName/CreateObject hooks) is DEFERRED to its own TU(s); only
// the destructor (and thereby the adjustor{4} thunk) is materialised here.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the inherited BrnEffectControl
// teardown offsets (meDetachState @ +0x28, mbResourcesReady @ +0x31,
// meAttachState @ +0x24) are X360 byte offsets assuming 4-byte pointers/vptr;
// members are pinned BY NAME via the committed base and absolute offsets are NOT
// static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// BrnCameraControl.h:35 (DWARF). Reuses the committed BrnEffectControl base by
// name. The virtual destructor (and its IResourceRequester-base adjustor{4} thunk
// @ 0x826BB438) are generated structurally from the dual-base layout.
struct CameraControl : public BrnSound::Logic::BrnEffectControl
{
    CameraControl() {}
    virtual ~CameraControl();

    CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetTypeInfo() const override;
    const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectControl* CreateObject(u32 auType);
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_CAMERA_CONTROL_H
