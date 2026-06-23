#ifndef BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_3D_CONTROL_H
#define BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_3D_CONTROL_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/CgsMemBase.h"   // CgsSound::MemBase

// =============================================================================
// BrnSound::Logic::World::Emitter3dControl
//   GameSource/Sound/Module/LogicModule/BrnEmitter3dControl.{h,cpp}
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The factory CreateObject @ 0x826D1C98
// allocates the object through `CgsSound::MemBase::operator new(176, ...)`, so
// Emitter3dControl is a 176-byte (0xB0) CgsSound::MemBase descendant: the ctor
// installs this class's own vtable at +0 and the deleting destructor re-installs the
// shared MemBase sub-object vtable (off_820AA820) before routing the storage back to
// the sound allocator (off_82FFB954) — exactly the CgsSound::MemBase teardown shape
// (see CgsMemBase.h).
//
// The fields below are the members the constructor (@ 0x826BAD18) initialises and the
// deleting destructor (@ 0x826D1D18) tears down, modelled BY NAME at the X360 store
// sequence. No DWARF/leak home exists for this type, so the field NAMES are
// descriptive (the X360 only proves their offsets/types/initial values, not source
// names); only the initialisation semantics are load-bearing.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 stores are by absolute byte
// offset behind a 4-byte vptr (first data field at +0x04). On a 64-bit host the vptr
// width differs, so members are pinned BY NAME and SEQUENCE only; absolute offsets
// and the 0xB0 size are NOT static_asserted. Uninitialised gaps in the 176-byte
// object are modelled as named reserved spans so the typed members keep their X360
// store order. FLAG: field names are descriptive, not DWARF-attested.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace World
{

// 3D sound-emitter control. Drives an emitter's spatialised playback state.
class Emitter3dControl : public CgsSound::MemBase
{
public:
    // Emitter3dControl @ 0x826BAD18 — installs the vtable and zero/constant-inits
    // every field the ctor touches.
    Emitter3dControl();

    // The X360 `scalar deleting destructor' @ 0x826D1D18 re-installs the MemBase
    // sub-object vtable, sets the teardown state (meReleaseState = 3), and frees via
    // the sound allocator. Modelled as the virtual destructor whose observable body
    // is that state teardown; the vtable re-install and allocator free are
    // host-synthesised (same treatment as CgsMemBase / BrnEffectControl).
    virtual ~Emitter3dControl();

private:
    // --- X360 ctor store map (offsets behind the +0x04 vptr; see LAYOUT NOTE) ---
    u32  muFlagsLow;          // +0x04  (stw 0)
    u32  muVoiceHandle;       // +0x08  (stw 0)
    u16  muSlotIndex;         // +0x0C  (sth 0)
    u16  muSlotGeneration;    // +0x0E  (sth 0)
    u8   maReserved10[8];     // +0x10  (not touched by ctor)
    f32  mfGain;              // +0x18  (stfs 0.0)
    f32  mfPitch;             // +0x1C  (stfs 0.0)
    u32  muStateA;            // +0x20  (stw 0; dtor stores 0)
    s32  meReleaseState;      // +0x24  (stw 0; dtor stores 3)
    u8   maFlags2C[4];        // +0x2C  (ctor stb 0 @+0x2C; dtor stb 0 @+0x2D)
    u32  muStateB;            // +0x30  (stw 0)
    f32  maTransform[8];      // +0x34..+0x50  (8 x stfs 0.0)
    u32  muStateC;            // +0x54  (stw 0)
    u32  muStateD;            // +0x58  (stw 0)
    u8   maReserved5C[0x44];  // +0x5C..+0x9F  (not touched by ctor)
    f32  mfRangeOuter;        // +0xA0  (stfs 2.0)
    f32  mfRangeInner;        // +0xA4  (stfs 0.5)
    u8   mbActive;            // +0xA8  (stb 0)
    u8   maReservedTail[7];   // +0xA9..+0xAF  (pad to the 0xB0 allocation size)
};

} // namespace World
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_3D_CONTROL_H
