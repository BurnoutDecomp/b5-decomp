#pragma once

// =====================================================================================
// rw::audio::core::StreamPool
//
// EARenderWare "rwaudio" streaming-pool: a small registry of shareable
// rw::core::filesys::Stream cursors, handed out by priority and reference-counted.
// Its ONLY consumer in this image is rw::audio::core::SndPlayer1 (three call sites).
//
// SOURCES, in the order they were trusted:
//   * BURNOUT_X360_ARTIST.XEX -- authoritative for every offset and side effect.
//     GetInstance @0x82B6BA68, AcquireStream @0x82B6BAB0, ReleaseStream @0x82B6BC48.
//   * references/Feb-2007/.../rwaudiocore/2.11.00/include/rw/audio/core/streampool.h --
//     the REAL vendor header. Every name below is its name, not a reconstruction:
//     StreamDesc / timeStamp / priority / pStreamLostCallback / pStreamLostContext /
//     pRwCoreStream / refCount / allocated / pad, and the inline AllocateStream helper.
//   * IDA Files/ProStreet08Milestone.pdb + .map (Oct-2007 X360 rwaudiocore) --
//     independently confirms the same layout and the two mangled signatures.
//   Decode: progress/scratch_dossiers/streampool_decode_codex.md and
//           progress/scratch_dossiers/streampool_acquire_release_codex.md
//
// =====================================================================================
// ⚠️⚠️ READ THIS BEFORE USING OR "FIXING" ANY OF IT ⚠️⚠️
//
// THE POOL REGISTRY IS NEVER POPULATED IN THE ARTIST BUILD, SO GetInstance ALWAYS
// RETURNS NULL -- ON THE REAL CONSOLE, NOT JUST HERE.
//
// GetInstance walks an intrusive list off the global at 0x83271C7C. Nothing in the
// entire image ever writes that global. Verified three independent ways:
//   1. every instruction word with displacement 0x1C7C -- five hits, and only one is
//      real (GetInstance's own lwz). The others resolve to 0x820A1C7C (rodata), to a
//      running pointer, or are data words that merely decode like instructions.
//      THERE IS NO `stw` TO IT ANYWHERE.
//   2. the literal 0x83271C7C appears ZERO times as a data word, so no pointer table
//      hands its address to a generic list helper.
//   3. 0x83271C7C maps past the end of the 0x105B000-byte image -- it is .bss, hence
//      zero at load -- so the walk exits down `li r3,0` every single time.
// Consistently, ARTIST contains no StreamPool constructor, no CreateInstance, and no
// boot-time pool creation, even though the Feb-2007 vendor header declares one.
//
// AcquireStream then dereferences its `this` with NO null guard (`lbz r11,0x28(r30)`
// @0x82B6BAD4), and SndPlayer1::PlayHandler passes the pool straight in while testing
// only the RESULT (@0x82BA431C). So SndPlayer1's stream-open path would FAULT ON RETAIL
// HARDWARE. It is compiled-in dead code, gated by RequestExternal::playType
// (@0x82BA42E4): 0 (resident) skips the pool entirely; 1 (streamed) and 2 (hybrid) walk
// into it. Retail works, therefore retail only ever hands a 'SnP1' voice RESIDENT
// requests -- real stream music goes through the game's own fork
// SndPlayer1_CgsStreamMod ('JStr') and the module's IStreamProvider, which an
// independent decode confirmed never touches this pool at all.
//
// ⚠️ THEREFORE THIS HOME IS FAITHFUL, NOT DEFENSIVE. An empty registry whose lookup
// fails is EXACT -- it fails for the same reason the console's does. Do NOT add a
// null-pool guard the console lacks: that would hide a genuine content divergence
// behind silently different behaviour, which is the opposite of what this port wants.
// =====================================================================================

#include "types.hpp"                        // f32, f64, s16, u8, u32
#include "rw/audio/core/ITask.h"            // ListDNode (the registry link)

namespace rw
{
namespace core
{
namespace filesys
{
    struct Stream;                          // homed: src/SDKs/EATech/rwcore/filesys/stream.h
}
}

namespace audio
{
namespace core
{

class System;

class StreamPool
{
public:
    // The vendor's own opaque handle type. AcquireStream returns a StreamDesc*, and
    // every consumer (SndPlayer1) stores it as an opaque pointer, so the vendor's
    // `typedef void *StreamHandle` is kept verbatim rather than "improved".
    typedef void *StreamHandle;
    typedef void (*StreamLostCallback)(void *apContext);

    // @0x82B6BA68 -- walk the registry for `auGuid`. ⚠️ ALWAYS RETURNS NULL in this
    // build; see the banner. Reproduced as the real list walk (rather than a bare
    // `return 0`) so that the reason it fails is the console's reason.
    static StreamPool *GetInstance(u32 auGuid);

    // @0x82B6BAB0 -- three scans: reuse an entry already lent to this context, else
    // take the first unallocated entry, else evict the lowest-priority entry.
    // ⚠️ The priority arrives in f1. Hex-Rays invents trailing integer parameters when
    // float arguments occupy ABI slots 2..4, and has already produced two wrong
    // signatures in this project; this one is the ProStreet .map's mangled signature
    // `?AcquireStream@StreamPool@core@audio@rw@@QAAPAXMP6AXPAX@Z0@Z` -- (float,
    // void(*)(void*), void*) -- and nothing more.
    StreamHandle AcquireStream(f32 afPriority, StreamLostCallback apfnStreamLost,
                               void *apStreamLostContext);

    // @0x82B6BC48 -- drop a reference; at zero, Kill the underlying stream and free
    // the slot.
    void ReleaseStream(StreamHandle apStreamHandle);

    // Vendor inline accessors (no ARTIST body of their own -- they are inlined at their
    // call sites). Kept because they are the vendor's public surface for the handle.
    void SetStreamPriority(StreamHandle apStreamHandle, f32 afPriority);
    rw::core::filesys::Stream *GetRwCoreStream(StreamHandle apStreamHandle);

private:
    // ---- StreamDesc: one lendable stream slot. Console sizeof 0x20. -----------------
    // ⚠️ THAT 0x20 DOES NOT SURVIVE x64: the record holds two data pointers and a
    // function pointer. Every scan below indexes `mpStreamDesc[i]` as a typed array, so
    // the console stride never appears. Do not reintroduce it.
    struct StreamDesc
    {
        f64 timeStamp;                          // +0x00 seeded from System::mfSystemTime;
                                                //       breaks equal-priority eviction ties
        f32 priority;                           // +0x08 *** the eviction key ***
        StreamLostCallback pStreamLostCallback; // +0x0C *** WIDENS *** called on eviction
        void *pStreamLostContext;               // +0x10 *** WIDENS *** also the reuse key
        rw::core::filesys::Stream *pRwCoreStream;// +0x14 *** WIDENS ***
        s16 refCount;                           // +0x18 ⚠️ SIGNED. ReleaseStream's
                                                //       `extsh.` @0x82B6BC64 sign-extends
                                                //       the decremented 16-bit value before
                                                //       testing it, so an over-release goes
                                                //       NEGATIVE and never re-frees.
        u8  allocated;                          // +0x1A
        char pad;                               // +0x1B vendor name; no ARTIST access
    };

    // The vendor inline helper, corroborated store-for-store by ARTIST at
    // 0x82B6BB84..0x82B6BBAC and 0x82B6BC04..0x82B6BC30.
    StreamDesc *AllocateStream(StreamDesc *apStreamDesc, f32 afPriority,
                               StreamLostCallback apfnStreamLost,
                               void *apStreamLostContext);

public:
    // ---- layout (console offsets documentary; host widths, by-name access) ----------
    System *mpSystem;             // +0x00 (its mfSystemTime stamps every allocation)
    StreamDesc *mpStreamDesc;     // +0x04 the slot array
    // FLAG [+0x08 .. +0x23 NOT ATTESTED]. No body this tree decodes touches these
    // bytes. The Feb-2007 vendor header puts `TimerClient mTimerClient` and
    // `rw::IResourceAllocator *mpAllocator` here, but that is the 2.11 header against a
    // 3.03 image and the OFFSETS ARE NOT ARTIST-CONFIRMED, so they are not declared as
    // real members. This placeholder exists only to keep the declaration order honest;
    // ⚠️ its SIZE is meaningless on the host and nothing may reach through it.
    u8 mUnattested08[0x24 - 0x08];
    u32 mGuid;                    // +0x24 matched by GetInstance (@0x82B6BA84)
    u8  mNumStreams;              // +0x28 the scan bound (@0x82B6BAD4)
    ListDNode mListNode;          // +0x2C the registry link.
                                  // ⚠️ GetInstance recovers the owner by SUBTRACTING the
                                  // console literal 0x2C (@0x82B6BA78). On the host that
                                  // literal is wrong; the walk uses offsetof instead.
};

} // namespace core
} // namespace audio
} // namespace rw
