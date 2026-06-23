#include "GameSource/Sound/Module/LogicModule/BrnEmitter3dControl.h"

#include <cstring>   // memset

// =============================================================================
// BrnSound::Logic::World::Emitter3dControl — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnEmitter3dControl.h for the
// CgsSound::MemBase-descendant shape and the X360-32-bit-vs-host-64-bit offset note.
//
// This TU's recon'd function set is exactly two entries:
//   Emitter3dControl()                      (the ctor)                  @ 0x826BAD18
//   ~Emitter3dControl()                     (the `scalar deleting destructor`) @ 0x826D1D18
//
// dep_flags: none un-homed for THIS TU. The constructor only writes own members; the
// destructor teardown touches muStateA / meReleaseState / maFlags2C (all modelled BY
// NAME). The (a2 & 1) deallocation tail dispatches the global sound allocator
// (off_82FFB954); that allocator vtable is not homed here, so the `delete` half of the
// X360 deleting destructor is left to the host toolchain (same treatment as the
// CgsMemBase / BrnEffectControl siblings).
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace World
{

// ---------------------------------------------------------------------------
// Emitter3dControl  @ 0x826BAD18
//
// Zero/constant-inits every field the ctor stores. The two non-zero constants come
// from shared rodata and are X360-attested:
//   flt_82001CC0 = 0.0f  (every stfs/stw zero store)
//   flt_82001D9C = 2.0f  -> mfRangeOuter (+0xA0)
//   flt_82001DA0 = 0.5f  -> mfRangeInner (+0xA4)
// The leading `stw off_820B21F8, 0(this)` is this class's vtable install, produced
// implicitly by the C++ object construction, so it is not written here.
// ---------------------------------------------------------------------------
Emitter3dControl::Emitter3dControl()
    : muFlagsLow(0)         // +0x04
    , muVoiceHandle(0)      // +0x08
    , muSlotIndex(0)        // +0x0C
    , muSlotGeneration(0)   // +0x0E
    , mfGain(0.0f)          // +0x18
    , mfPitch(0.0f)         // +0x1C
    , muStateA(0)           // +0x20
    , meReleaseState(0)     // +0x24
    , muStateB(0)           // +0x30
    , muStateC(0)           // +0x54
    , muStateD(0)           // +0x58
    , mfRangeOuter(2.0f)    // +0xA0  (flt_82001D9C)
    , mfRangeInner(0.5f)    // +0xA4  (flt_82001DA0)
    , mbActive(0)           // +0xA8
{
    // +0x2C: ctor `stb 0` of the leading flag byte (the rest of the 4-byte flag
    // span is not touched by the ctor — only +0x2D is later cleared by the dtor).
    maFlags2C[0] = 0;

    // +0x34..+0x50: eight consecutive `stfs 0.0` clearing the transform/spatial
    // block (modelled as f32[8]).
    std::memset(maTransform, 0, sizeof(maTransform));

    // The reserved spans (+0x10, +0x5C.., tail) are not initialised by the X360 ctor
    // and are left uninitialised here to match.
}

// ---------------------------------------------------------------------------
// ~Emitter3dControl  @ 0x826D1D18  (the X360 `scalar deleting destructor`)
//
//   stw  off_820AA820, 0(this)     ; re-install the MemBase sub-object vtable
//   stb  0, 0x2D(this)             ; clear the +0x2D flag byte (maFlags2C[1])
//   stw  3, 0x24(this)             ; meReleaseState = 3
//   stw  0, 0x20(this)             ; muStateA = 0
//   if (a2 & 1) { ... deallocate via off_82FFB954 (the MemBase allocator) }
//   return this
//
// The vtable re-install is the compiler-emitted devirtualisation of the base
// destructor sub-object; in reconstructed C++ it is produced implicitly by the
// destructor chain, so the BODY here is the observable member teardown. The store
// ORDER mirrors the asm: maFlags2C[1] (+0x2D), then meReleaseState (+0x24), then
// muStateA (+0x20).
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain (the `delete` half of the X360 deleting destructor).
// ---------------------------------------------------------------------------
Emitter3dControl::~Emitter3dControl()
{
    maFlags2C[1]  = 0;   // stb 0, 0x2D
    meReleaseState = 3;  // stw 3, 0x24
    muStateA      = 0;   // stw 0, 0x20
}

} // namespace World
} // namespace Logic
} // namespace BrnSound
