#pragma once

// =====================================================================================
// rw::audio::core::TimerManager -- rwaudio's per-frame profiling-timer scheduler.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store. There is NO matching TU in the Feb-2007 PS3 leak and no
// DecFIGS DWARF for this type, so every offset below is grounded directly in the
// disassembly of the bodied methods:
//   TimerManager (ctor) @0x82B6BED0   ExecuteTimers @0x82B6BF18   RemoveTimer @0x82B6DE70
//   Release             @0x82B6DEE8   AddTimer      @0x82B6EB88
//
// The manager owns two Collections (@+0x00 and @+0x1C -- 28 bytes each). Each Collection
// hands out doubly-linked Nodes whose owner back-pointer is a TimerHandle (see
// TimerHandle.h / Collection.h). A timer lives in exactly one of the two Collections; the
// Collection it lives in is recorded in its TimerHandle::mStage byte. ExecuteTimers walks
// one Collection's used list, calls each handle's callback (passing the shared time value
// at +0x38), and records the CPU cycles the callback took into TimerHandle::mCpuTicks.
//
// A timer may remove itself from inside its own callback: RemoveTimer, seeing the handle
// is the one currently executing (mpCurrentHandle), defers the unlink by stashing the
// node in mpDeferredRemoveNode + its Collection index in miCurrentCollection; ExecuteTimers
// performs the actual Collection::RemoveNode after the callback returns.
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable for matching a
// third-party/legacy API).
//
// Layout grounded in the asm load/store displacements (this == r3). NOTE: the +0xNN
// annotations are the X360 (32-bit-pointer) offsets from the disassembly and are
// documentary only -- the members are declared with x64 widths (the project PC target),
// so the C++ compiler computes the real PC offsets; only the member ORDER is load-bearing.
//   +0x00  maCollections[0]    (Collection; 28 bytes on X360)
//   +0x1C  maCollections[1]    (Collection; 28 bytes on X360)
//   +0x38  mfCallbackTime      (f32,  init -1.0; the time value passed to each callback)
//   +0x3C  mpCurrentHandle     (TimerHandle*; handle whose callback is currently running)
//   +0x40  miCurrentCollection (int;  Collection index for a deferred self-remove)
//   +0x44  mpDeferredRemoveNode(Node*; pending self-remove node, or null -- also the flag)
// (mpCurrentHandle / miCurrentCollection are intentionally left uninitialised by the ctor,
// exactly as the asm leaves them; only maCollections, mfCallbackTime and
// mpDeferredRemoveNode are zeroed/seeded there.)
// =====================================================================================

#include "types.hpp" // f32, s32, u8, u32

#include "rw/audio/core/Collection.h"  // Collection (embedded by value) + Collection::Node
#include "rw/audio/core/TimerHandle.h" // TimerHandle (referenced through the Nodes)

namespace rw
{
namespace audio
{
namespace core
{

class TimerManager
{
public:
    // Per-tick timer callback: (context, time). Matches TimerHandle::mpCallback.
    typedef void (*TimerCallback)(void *pContext, f32 fTime);

    // Default-construct: zero both embedded Collections, seed mfCallbackTime to -1.0, and
    // clear the deferred-remove slot. (IDA renders `this`/return as int because `this` is
    // the only arg; the asm r3 contract is authoritative.)
    static TimerManager *TimerManager_ctor(TimerManager *self);

    // Register a timer in Collection `collectionIndex`: grow that Collection if it has no
    // capacity yet (by 74 slots), allocate a Node wired to `handle`, then fill the handle.
    // Returns 0 on success, non-zero if the Collection grow failed.
    static int AddTimer(TimerManager *self, TimerHandle *handle, TimerCallback callback,
                        void *context, const char *name, int collectionIndex, u8 visibility);

    // Walk Collection `collectionIndex`'s used list, invoking each handle's callback and
    // timing it into TimerHandle::mCpuTicks; honour any deferred self-remove.
    static void ExecuteTimers(TimerManager *self, int collectionIndex);

    // Clear + release both owned Collections (frees their NodeBlocks).
    static void Release(TimerManager *self);

    // Unregister `handle`. If it is the one currently executing, defer the unlink; else
    // unlink it now (unless already stage 3 = free). Resets the handle to stage 3.
    static void RemoveTimer(TimerManager *self, TimerHandle *handle);

    Collection maCollections[2];          // +0x00  two 28-byte Collections
    f32 mfCallbackTime;                   // +0x38  time value handed to each callback
    TimerHandle *mpCurrentHandle;         // +0x3C  handle whose callback is running
    s32 miCurrentCollection;              // +0x40  Collection index for a deferred remove
    Collection::Node *mpDeferredRemoveNode; // +0x44  pending self-remove node (0 = none)
};

} // namespace core
} // namespace audio
} // namespace rw
