// =====================================================================================
// rw::audio::core::EaXmaDec bodies (partial).
//
// EARenderWare "rwaudio" XMA (Xbox 360 hardware-format) stream decoder plug-in.
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this TU.
//
// This file homes the shared hardware-context POOL side of the codec plus the leaf
// helpers; the instance-lifecycle / decode-pump bodies live in the EaXmaDec_wG_*.cpp
// part files of this TU:
//   GetSize          @0x82B93C78   (this file)
//   FeedEvent        @0x82B96140   (this file)
//   LeftShiftInBits  @0x82B8FB20   (this file)
//   ParseToNextFrame @0x82B8FB88   (this file)
//   XMAmemcpy        @0x82B8FA18   (this file)
//   GetDecoderDesc   @0x82B93B88   (this file)
//   AllocateResources    @0x82B8F580  (this file)
//   DeallocateResources  @0x82B93BB0  (this file; no ledger identity -- exporter gap)
//   CreateInstanceEvent / ReleaseEvent / ~EaXmaDec   -> EaXmaDec_wG_02.cpp
//   DecodeEvent / CopySamplesOut / EnterRecoveryMode -> EaXmaDec_wG_03.cpp
//   Service / StartPair / Reset                      -> EaXmaDec_wG_04.cpp
// The pool statics (X360 byte_8327A2FE / off_8327A304..dword_8327A320 and the .data
// tunables dword_82F89388/90) are static members of EaXmaDec (EaXmaDec.h) DEFINED here,
// so every part file reaches the same storage. Recovered values:
// scratchpad/waveG/eaxma_dump.txt.
//
// The `_savegprlr_* / _restgprlr_*` calls in the pseudocode are the compiler's register
// save/restore prologue/epilogue helpers -- not source-level calls -- so they are dropped.
//
// LEDGER NOTE (exporter gap). DeallocateResources @0x82B93BB0 has NO ledger identity: the
// IDA export carries no per-function JSON for it, so it appears in neither `work show` nor
// the postmortem dossier for this TU (same gap recorded in scratchpad/waveE/AudioSystem.spec.md
// for the System teardown path). It is nonetheless a real function in the image -- declared
// in EaXmaDec.h and called by the committed rw::audio::core::System::Release (System.cpp),
// whose unresolved external this body closes. Its disassembly was recovered directly from the
// database and lives in scratchpad/waveG/eaxma_dump.txt; the reconstruction below is against
// that listing. The conductor should reconcile the ledger accordingly.
// =====================================================================================

#include "rw/audio/core/EaXmaDec.h"
#include "rw/audio/core/DecoderRegistry.h" // complete DecoderDesc (the descriptor is DEFINED in this TU)

#include "rw/audio/core/PlugIn.h"                 // rw::audio::core::System (Alloc/PhysicalAlloc/Free)
#include <eathread/eathread_mutex.h> // vendor EA::Thread::Mutex (see the System.cpp vendor-mutex note)
#include "SDKs/XAudio/XmaHardware.h"              // XMACreateContext / XMADisableContext / XMAReleaseContext

#include <cstdint> // uintptr_t
#include <cstring> // memcpy
#include <new>     // placement new

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// The shared hardware-context pool statics (see EaXmaDec.h for the X360 addresses).
// suNumContexts/suRecoveryTimeoutMs are initialised .data words in the image (0x100 / 4,
// scratchpad/waveG/eaxma_dump.txt); the rest are zero-initialised.
// -------------------------------------------------------------------------------------
bool EaXmaDec::sbResourcesAllocated = false;              // byte_8327A2FE
u32 EaXmaDec::suNumContexts = 256;                        // dword_82F89388 = 0x100
u32 EaXmaDec::suRecoveryTimeoutMs = 4;                    // dword_82F89390 = 4
EaXmaDec::ContextRecord *EaXmaDec::spRecords = 0;         // off_8327A304
u8 *EaXmaDec::spInputMemory = 0;                          // dword_8327A308
u8 *EaXmaDec::spOutputMemory = 0;                         // dword_8327A30C
EA::Thread::Mutex *EaXmaDec::spPoolMutex = 0;             // dword_8327A310
EaXmaDec::PoolNode *EaXmaDec::spInUseHead = 0;            // off_8327A314
EaXmaDec::PoolNode *EaXmaDec::spFreeHead = 0;             // off_8327A318
EaXmaDec::PoolNode *EaXmaDec::spFreeTail = 0;             // off_8327A31C
u32 EaXmaDec::suFreeCount = 0;                            // dword_8327A320

// The shared rwaudio System singleton (off_83271928). Its object is defined/owned by the
// System TU; the pool allocates and frees through it (Decoder.cpp/CMpegBase.cpp precedent).
extern "C" System *off_83271928;

// This codec's static registration descriptor (off_82F89394, .data). DEFINED here
// (2026-08-25, faithful-audio-engine phase A2 -- the mounted build links it). XEX
// recovery (big-endian .data @0x82F89394):
//   +0x00 0x82B93C78 EaXmaDec::GetSize            +0x04 0x82B93C98 CreateInstanceEvent
//   +0x08 0x82B8F7C0 ReleaseEvent                 +0x0C 0x82B96380 DecodeEvent
//   +0x10 mpNext = 0                              +0x14 muId = 0x45586D30 'EXm0'
// FLAG (host callback slots deferred): the four header words are GUEST function
// addresses; the host must not store guest addresses in pointer slots, and NO
// reconstructed code dispatches through the descriptor header yet (registration
// touches only mpNext/muId). The header stays the opaque zeroed span until the
// descriptor-dispatch consumer is reconstructed and types the slots to the host
// functions named above.
extern "C" DecoderDesc off_82F89394 = { {0}, 0, 0x45586D30u /* 'EXm0' */ };

// -------------------------------------------------------------------------------------
// The shared XMA hardware buffer geometry, in bytes. Unlike a C++ record stride these are
// HARDWARE / data-stream sizes carved out of two raw physical allocations, so the console
// immediates ARE the values and are reproduced literally:
//   0x800  one XMA packet == one hardware input buffer (two per context, fed alternately)
//   0x200  per-context decoder overlap/scratch area
//   0x1800 per-context decoded-s16 PCM output ring
//   0x1200 = 2 * 0x800 + 0x200 -- the per-context footprint of the input allocation
// -------------------------------------------------------------------------------------
static const u32 KU_XMA_PACKET_BYTES = 2048;
static const u32 KU_XMA_OVERLAP_BYTES = 512;
static const u32 KU_XMA_OUTPUT_RING_BYTES = 6144;
static const u32 KU_XMA_INPUT_BYTES_PER_CONTEXT =
    2 * KU_XMA_PACKET_BYTES + KU_XMA_OVERLAP_BYTES;

// Round a carved-block base up to 8, exactly as the X360 does with its `addi rN, rN, 7 /
// clrrwi rN, rN, 3` pairs -- but on the host's own pointer width (the identical helper in
// System.cpp:104 carves that allocation the same way). This is arithmetic on the raw
// allocation the pool objects are placed into, not offset access into a typed object.
static u8 *AlignUp8(u8 *pAddress)
{
    return reinterpret_cast<u8 *>(
        (reinterpret_cast<std::uintptr_t>(pAddress) + 7) & ~static_cast<std::uintptr_t>(7));
}

// -------------------------------------------------------------------------------------
// AllocateResources @0x82B8F580
// One-shot construction of the shared XMA hardware-context pool, guarded by
// sbResourcesAllocated (set BEFORE any allocation, exactly as the asm orders the stores).
//
// Two physical ranges are taken through the System's physical hooks: the encoded-input
// range (0x1200 bytes per context = two 2048-byte packet buffers plus a 512-byte overlap
// area, all the overlap areas packed after all the packet buffers) and the decoded-output
// range (one 6144-byte s16 PCM ring per context). Both are 2048-aligned; the input range is
// additionally mapped write-combined (X360 XPhysicalAlloc protect word 0x404 =
// PAGE_READWRITE | PAGE_WRITECOMBINE) while the output range is plain PAGE_READWRITE (4).
//
// A third, ordinary allocation holds the pool mutex followed by the record table. The X360
// asked for `28 * n + 48` bytes and put the records at block + 48 -- 0x1C is the CONSOLE
// sizeof(ContextRecord) and 0x30 the CONSOLE sizeof(EA::Thread::Mutex). Both widen on the
// x64 host (five pointers per record, and the host mutex object), so the size and the carve
// are expressed with host sizeofs; using the console literals would under-allocate and the
// record table would run off the end of the block.
//
// Faithful oddities preserved: spInputMemory is nulled and then immediately overwritten by
// its own allocation; Mutex::Init runs unconditionally, even when the block allocation
// failed and spPoolMutex is therefore null; and the per-record loop runs to completion
// regardless of whether either physical range was obtained.
// -------------------------------------------------------------------------------------
void EaXmaDec::AllocateResources()
{
    if (sbResourcesAllocated)
        return;

    System *lpSystem = off_83271928;
    sbResourcesAllocated = true;

    spInputMemory = 0;
    spInputMemory = static_cast<u8 *>(
        System::PhysicalAlloc(lpSystem, KU_XMA_INPUT_BYTES_PER_CONTEXT * suNumContexts,
                              2048, 0x404, "XMA input buffers"));
    spOutputMemory = static_cast<u8 *>(
        System::PhysicalAlloc(lpSystem, KU_XMA_OUTPUT_RING_BYTES * suNumContexts,
                              2048, 4, "XMA output buffers"));

    // ---- mutex + record-table block (host sizes; see the header comment) -------------
    const usize kuMutexSlot = (sizeof(EA::Thread::Mutex) + 7) & ~static_cast<usize>(7);
    const u32 luBlockSize =
        static_cast<u32>(kuMutexSlot + sizeof(ContextRecord) * suNumContexts);

    u8 *lpBlock =
        static_cast<u8 *>(System::Alloc(lpSystem, luBlockSize, "Snd Xma Resources", 16, 0));

    // The X360's null test around the construction is the compiler's placement-new guard,
    // which `new (p) T` re-emits; on the null path the store leaves spPoolMutex null, which
    // is exactly what the asm's other branch does. asm: Mutex(p, r4 = 0, r5 = 1).
    spPoolMutex = new (lpBlock) EA::Thread::Mutex(0, true);

    // asm: MutexParameters(&stack, r4 = 1, r5 = 0), then Init through the (possibly null)
    // pool-mutex pointer with no guard -- faithful.
    EA::Thread::MutexParameters lMutexParams(true, 0);
    spPoolMutex->Init(&lMutexParams);

    spFreeHead = 0;
    spFreeTail = 0;
    suFreeCount = 0;
    spInUseHead = 0;
    spRecords = reinterpret_cast<ContextRecord *>(AlignUp8(lpBlock + sizeof(EA::Thread::Mutex)));

    for (u32 luIndex = 0; luIndex < suNumContexts; ++luIndex)
    {
        ContextRecord &lRecord = spRecords[luIndex];

        lRecord.mpContext = 0;
        XMACreateContext(&lRecord.mpContext);

        // Buffer carve out of the two physical ranges. The input range holds every
        // context's two packet buffers first (4096 bytes each context), then every
        // context's overlap area -- asm: ((8 * n + i) << 9) == 4096 * n + 512 * i.
        lRecord.mpInputBuffer0 = spInputMemory + 2 * KU_XMA_PACKET_BYTES * luIndex;
        lRecord.mpInputBuffer1 = lRecord.mpInputBuffer0 + KU_XMA_PACKET_BYTES;
        lRecord.mpOverlapBuffer = spInputMemory +
                                  2 * KU_XMA_PACKET_BYTES * suNumContexts +
                                  KU_XMA_OVERLAP_BYTES * luIndex;
        lRecord.mpOutputBuffer = spOutputMemory + KU_XMA_OUTPUT_RING_BYTES * luIndex;

        XMADisableContext(lRecord.mpContext, 1);

        // Append the record's node to the TAIL of the free list.
        PoolNode *lpNode = &lRecord.mNode;
        lpNode->mpNext = 0;
        lpNode->mpPrev = spFreeTail;
        if (spFreeHead)
            spFreeTail->mpNext = lpNode;
        else
            spFreeHead = lpNode;
        spFreeTail = lpNode;
        ++suFreeCount;
    }
}

// -------------------------------------------------------------------------------------
// DeallocateResources @0x82B93BB0
// Tear the shared pool down again, guarded by (and clearing) the same one-shot flag: hand
// every hardware context back, destroy + free the mutex/record-table block, then release
// both physical ranges. Called by System::Release during audio-system teardown.
//
// Faithful details: the record loop walks spRecords by index with no null check; the mutex
// destructor and System::Free run on the (unchecked) spPoolMutex pointer; and the free/in-use
// list heads and suFreeCount are deliberately NOT cleared -- the binary leaves them dangling.
// The X360 call at 0x82B93C18 lands on the shared empty thunk because the console's
// ~EA::Thread::Mutex was trivial there; the host mutex has a real destructor, so the explicit
// destructor call is what reproduces the source (Delay.cpp precedent).
//
// (Reconstructed from the raw disassembly in scratchpad/waveG/eaxma_dump.txt -- this function
// has no ledger identity; see the exporter-gap note in the file header.)
// -------------------------------------------------------------------------------------
void EaXmaDec::DeallocateResources()
{
    if (!sbResourcesAllocated)
        return;

    sbResourcesAllocated = false;

    for (u32 luIndex = 0; luIndex < suNumContexts; ++luIndex)
        XMAReleaseContext(spRecords[luIndex].mpContext);

    spPoolMutex->~Mutex();

    System *lpSystem = off_83271928;
    System::Free(lpSystem, spPoolMutex, 0);
    spRecords = 0;
    spPoolMutex = 0;

    System::PhysicalFree(lpSystem, spInputMemory);
    System::PhysicalFree(lpSystem, spOutputMemory);
    spInputMemory = 0;
    spOutputMemory = 0;
}

// -------------------------------------------------------------------------------------
// GetDecoderDesc @0x82B93B88
// Hand back the address of this codec's static registration descriptor, building the shared
// hardware-context pool first (AllocateResources is one-shot, so every later call is just the
// flag test). The sole caller is DecoderRegistry::RegisterStandardRunTimeDecoders.
// -------------------------------------------------------------------------------------
DecoderDesc *EaXmaDec::GetDecoderDesc()
{
    AllocateResources();
    return &off_82F89394;
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B93C78
// Report the codec's per-instance allocation footprint: one pair record per stereo pair
// -- ceil(channels/2) of them -- laid at the first 8-aligned address past the fixed
// header (that is where CreateInstanceEvent puts the pair table). The required alignment
// (16) is written through the out-param. The descriptor framework passes the channel
// count in the first register.
//
// X360: `28 * ((ch + 1) >> 1) + 88` -- 0x58 fixed header + 0x1C per pair record. Those
// are CONSOLE layout words: on the x64 host the header and the pointer-bearing DecPair
// records widen, so the footprint is expressed in host sizeof form (the console literals
// would under-allocate and CreateInstanceEvent's tail carve would corrupt memory).
// -------------------------------------------------------------------------------------
u32 EaXmaDec::GetSize(u32 uNumChannels, u32 *puAlignment)
{
    *puAlignment = 16;
    return static_cast<u32>(((sizeof(EaXmaDec) + 7) & ~static_cast<usize>(7)) +
                            sizeof(DecPair) * ((uNumChannels + 1) >> 1));
}

// -------------------------------------------------------------------------------------
// FeedEvent @0x82B96140
// "Feed more input" callback (vtable slot 0; overrides the Decoder base no-op): a straight
// tail-call onto the internal Service pump (the asm is a bare `b Service`, so `this`
// passes straight through). void to match the base slot (see Decoder.h): the tail-call
// leaves Service's r3 in place only incidentally, and no reconstructed caller reads it.
// -------------------------------------------------------------------------------------
void EaXmaDec::FeedEvent()
{
    Service();
}

// -------------------------------------------------------------------------------------
// LeftShiftInBits @0x82B8FB20
// Extract `iNumBits` big-endian bits beginning at bit position `iBitPos` from the packed
// byte stream `pStream`, and shift them into the low end of the 32-bit accumulator, after
// shifting the accumulator's prior contents up by `iNumBits`. `pStream` is external packed
// XMA data, so it is walked as a raw byte buffer. No-op when iNumBits <= 0.
//
// A 24-bit big-endian window (three bytes starting at iBitPos/8) is assembled into the top
// of a 32-bit word, shifted left by the intra-byte bit offset (iBitPos % 8), then shifted
// down by (32 - iNumBits) so the requested field lands in the low bits.
// -------------------------------------------------------------------------------------
u32 *EaXmaDec::LeftShiftInBits(u32 *puAccum, const u8 *pStream, s32 iBitPos, s32 iNumBits)
{
    if (iNumBits > 0)
    {
        const u8 *pByte = pStream + iBitPos / 8;
        u32 uWindow = (static_cast<u32>(pByte[0]) << 24) |
                      (static_cast<u32>(pByte[1]) << 16) |
                      (static_cast<u32>(pByte[2]) << 8);
        u32 uBits = (uWindow << (iBitPos % 8)) >> (32 - iNumBits);
        *puAccum = uBits | (*puAccum << iNumBits);
    }
    return puAccum;
}

// -------------------------------------------------------------------------------------
// ParseToNextFrame @0x82B8FB88
// Advance a (byte-cursor, bit-offset) pair across one XMA frame. The frame-length field is
// decoded out of the bitstream with two LeftShiftInBits reads (split around the packet's
// 15-bit header budget when the offset is near the packet tail, `iBitPos > 0x3FD1`), then
// added to the running bit offset. When the offset crosses the 0x3FE0-bit packet boundary,
// the byte cursor wraps onto the next 2048-byte (0x800) XMA packet and the offset rolls back
// by one packet's worth of bits (0x3FE0).
//
// The X360 code leaves the LeftShiftInBits accumulator pointer in r3 on return, but every
// caller ignores it, so the source shape is a void frame-walk.
// -------------------------------------------------------------------------------------
void EaXmaDec::ParseToNextFrame(const u8 **ppCursor, s32 *piBitOffset)
{
    s32 iBitOffset = *piBitOffset;
    s32 iTailBits = 0;
    if (iBitOffset > 0x3FD1)
        iTailBits = iBitOffset - 0x3FD1;

    const u8 *pStream = *ppCursor;
    u32 uFrameLen = 0;
    LeftShiftInBits(&uFrameLen, pStream, iBitOffset + 32, 15 - iTailBits);
    LeftShiftInBits(&uFrameLen, pStream, iBitOffset + (15 - iTailBits) + 64, iTailBits);

    s32 iNext = iBitOffset + static_cast<s32>(uFrameLen);
    if (static_cast<u32>(iNext) >= 0x3FE0u)
    {
        *ppCursor = pStream + 2048;
        iNext = *piBitOffset + static_cast<s32>(uFrameLen) - 0x3FE0;
    }
    *piBitOffset = iNext;
}

// -------------------------------------------------------------------------------------
// XMAmemcpy @0x82B8FA18
// Copy `iSize` bytes from `pSrc` to `pDst` (topping up the XMA hardware input buffers). The
// X360 body is a hand-unrolled VMX unaligned block copy (lvlx/lvrx -> stvx128 over 128-byte
// blocks) with a scalar memcpy tail; semantically a plain byte copy. A member, but the asm
// overwrites r3 (this) with pDst before use, so the instance is unreferenced. Returns pDst.
// -------------------------------------------------------------------------------------
void *EaXmaDec::XMAmemcpy(void *pDst, const void *pSrc, s32 iSize)
{
    return std::memcpy(pDst, pSrc, static_cast<usize>(iSize));
}

} // namespace core
} // namespace audio
} // namespace rw
