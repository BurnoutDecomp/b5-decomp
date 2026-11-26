// =====================================================================================
// rw::audio::core::EaXmaDec bodies (partial).
//
// EARenderWare "rwaudio" XMA (Xbox 360 hardware-format) stream decoder plug-in.
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width and side-effect. No LegacySrc source and no DecFIGS DWARF exist for this TU.
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
#include <ppcintrinsics.h>
#include <vectorintrinsics.h>
#include "rw/audio/core/DecoderRegistry.h" // complete DecoderDesc (the descriptor is DEFINED in this TU)

#include "rw/audio/core/PlugIn.h"                 // rw::audio::core::System (Alloc/PhysicalAlloc/Free)
#include <eathread/eathread_mutex.h> // vendor EA::Thread::Mutex (see the System.cpp vendor-mutex note)
#include "SDKs/XAudio/XmaHardware.h"              // XMACreateContext / XMADisableContext / XMAReleaseContext

#include "types.hpp" // uintptr_t
#include <cstring> // memcpy
#include <new>     // placement new

#if defined(_XBOX) && defined(_MSC_VER)
// The XDK size setting reproduces the unaligned copy and bit-window helper
// at 0x82B8FA18 and 0x82B8FB20, and keeps ParseToNextFrame's two calls out of line.
#pragma optimize("s", on)
#endif

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

// This codec's static registration descriptor (off_82F89394, .data). DEFINED here
// (2026-08-25, faithful-audio-engine phase A2 -- the mounted build links it). XEX
// recovery (big-endian .data @0x82F89394):
//   +0x00 0x82B93C78 EaXmaDec::GetSize            +0x04 0x82B93C98 CreateInstanceEvent
//   +0x08 0x82B8F7C0 ReleaseEvent                 +0x0C 0x82B96380 DecodeEvent
//   +0x10 mpNext = 0                              +0x14 muId = 0x45586D30 'EXm0'
static bool EaXmaDecCreateThunk(Decoder *decoder)
{
    return EaXmaDec::CreateInstanceEvent(static_cast<EaXmaDec *>(decoder));
}

static void EaXmaDecReleaseThunk(Decoder *decoder)
{
    static_cast<EaXmaDec *>(decoder)->ReleaseEvent();
}

static s32 EaXmaDecDecodeThunk(Decoder *decoder, DecoderBuffer *buffer, s32 count)
{
    return static_cast<EaXmaDec *>(decoder)->DecodeEvent(buffer, count);
}

DecoderDesc EaXmaDec::sDecoderDesc = {   // ARTIST 0x82F89394 (IDA off_82F89394)
    &EaXmaDec::GetSize,
    &EaXmaDecCreateThunk,
    &EaXmaDecReleaseThunk,
    &EaXmaDecDecodeThunk,
    0,
    0x45586D30u, // 'EXm0'
    0
};

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
        (reinterpret_cast<uintptr_t>(pAddress) + 7) & ~static_cast<uintptr_t>(7));
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

    System *lpSystem = System::GetInstance();
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
        static_cast<u8 *>(lpSystem->Alloc(luBlockSize, "Snd Xma Resources", 16, 0));

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

    System *lpSystem = System::GetInstance();
    lpSystem->Free(spPoolMutex, 0);
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
    return &sDecoderDesc;
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
void EaXmaDec::LeftShiftInBits(u32 *puAccum, const u8 *pStream, s32 iBitPos, s32 iNumBits)
{
    if (iNumBits > 0)
    {
        const u32 uPrior = *puAccum;
        const s32 iByte = iBitPos / 8;
        const s32 iLeft = iBitPos % 8;
        const s32 iRight = 32 - iNumBits;

        u32 uWindow = pStream[iByte];
        uWindow = (uWindow << 8) | pStream[iByte + 1];
        uWindow = (uWindow << 8) | pStream[iByte + 2];
        uWindow <<= 8;

        *puAccum = ((uWindow << iLeft) >> iRight) | (uPrior << iNumBits);
    }
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
// -------------------------------------------------------------------------------------
void EaXmaDec::ParseToNextFrame(const u8 **ppCursor, s32 *piBitOffset)
{
    // The bit offset is read through the pointer at each use (ARTIST adds it as the first
    // operand, 0x82B8FBD0 / 0x82B8FBE8, and re-reads it after the cursor store, 0x82B8FBFC).
    s32 iTailBits = 0;
    if (*piBitOffset > 0x3FD1)
        iTailBits = *piBitOffset - 0x3FD1;

    const s32 iFirstBits = 15 - iTailBits;
    const u8 *pStream = *ppCursor;
    u32 uFrameLen = 0;
    LeftShiftInBits(&uFrameLen, pStream, *piBitOffset + 32, iFirstBits);
    LeftShiftInBits(&uFrameLen, pStream, *piBitOffset + iFirstBits + 64, iTailBits);

    u32 uNext = *piBitOffset + uFrameLen;
    if (uNext >= 0x3FE0u)
    {
        *ppCursor = pStream + 2048;
        uNext = *piBitOffset + uFrameLen - 0x3FE0;
    }
    *piBitOffset = uNext;
}

// -------------------------------------------------------------------------------------
// XMAmemcpy @0x82B8FA18
// Copy `iSize` bytes from `pSrc` to `pDst` (topping up the XMA hardware input buffers). The
// XMA input buffers come from the 2048-byte-aligned pool at 0x82B8F580.
// ARTIST uses indexed VMX loads for the possibly unaligned source and
// aligned stores to the packet buffer. The remaining bytes use memcpy.
// -------------------------------------------------------------------------------------
void EaXmaDec::XMAmemcpy(void * __restrict pDst, const void * __restrict pSrc, s32 iSize)
{
    u8 *pWrite = static_cast<u8 *>(pDst);
    const u8 *pRead = static_cast<const u8 *>(pSrc);

    __dcbt(0, pRead);
    __dcbt(128, pRead);

    if (iSize >= 128)
    {
        u32 uBlocks = static_cast<u32>(iSize) / 128;
        iSize -= uBlocks * 128;
        do
        {
            const __vector4 vLeft0 = __lvlx(pRead, 0);
            const __vector4 vRight0 = __lvrx(pRead, 16);
            const __vector4 vLeft1 = __lvlx(pRead, 16);
            const __vector4 vRight1 = __lvrx(pRead, 32);
            const __vector4 vLeft2 = __lvlx(pRead, 32);
            const __vector4 vRight2 = __lvrx(pRead, 48);
            const __vector4 vLeft3 = __lvlx(pRead, 48);
            const __vector4 vRight3 = __lvrx(pRead, 64);
            const __vector4 vLeft4 = __lvlx(pRead, 64);
            const __vector4 vRight4 = __lvrx(pRead, 80);
            const __vector4 vLeft5 = __lvlx(pRead, 80);
            const __vector4 vRight5 = __lvrx(pRead, 96);
            const __vector4 vLeft6 = __lvlx(pRead, 96);
            const __vector4 vRight6 = __lvrx(pRead, 112);
            const __vector4 vLeft7 = __lvlx(pRead, 112);
            const __vector4 vRight7 = __lvrx(pRead, 128);
            pRead += 128;
            __dcbt(128, pRead);
            const __vector4 vBlock0 = __vor(vLeft0, vRight0);
            const __vector4 vBlock1 = __vor(vLeft1, vRight1);
            const __vector4 vBlock2 = __vor(vLeft2, vRight2);
            const __vector4 vBlock3 = __vor(vLeft3, vRight3);
            const __vector4 vBlock4 = __vor(vLeft4, vRight4);
            const __vector4 vBlock5 = __vor(vLeft5, vRight5);
            const __vector4 vBlock6 = __vor(vLeft6, vRight6);
            const __vector4 vBlock7 = __vor(vLeft7, vRight7);
            __stvx(vBlock0, pWrite, 0);
            __stvx(vBlock1, pWrite, 16);
            __stvx(vBlock2, pWrite, 32);
            __stvx(vBlock3, pWrite, 48);
            __stvx(vBlock4, pWrite, 64);
            __stvx(vBlock5, pWrite, 80);
            __stvx(vBlock6, pWrite, 96);
            __stvx(vBlock7, pWrite, 112);
            pWrite += 128;
        } while (--uBlocks);
    }

    if (iSize != 0)
        std::memcpy(pWrite, pRead, iSize);
}

} // namespace core
} // namespace audio
} // namespace rw

// FOLDED FROM EaXmaDec_wG_02.cpp
// =====================================================================================
// rw::audio::core::EaXmaDec bodies -- part 2 of the TU: the INSTANCE LIFECYCLE.
//
// EARenderWare "rwaudio" XMA (Xbox 360 hardware-format) stream decoder plug-in.
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width and side-effect. No LegacySrc source and no DecFIGS DWARF exist for this
// TU. The class, its nested pool types and the shared pool statics live in EaXmaDec.h;
// the statics are DEFINED in EaXmaDec.cpp, which also carries the full function roster.
//
//   CreateInstanceEvent   @0x82B93C98   (DecoderDesc slot +0x04)
//   ReleaseEvent          @0x82B8F7C0   (DecoderDesc slot +0x08)
//   ~EaXmaDec             @0x82B93ED0   (vtable slot [1], the deleting-destructor thunk)
//
// These three are the two ends of a pool record's life: CreateInstanceEvent claims one
// shared hardware-context record per stereo pair off the free list, ReleaseEvent hands
// every one of them back. Both walk the same intrusive doubly-linked free/in-use lists
// under the shared pool mutex (EaXmaDec::spPoolMutex).
//
// MUTEX. The asm passes `&dword_8215A740` as EA::Thread::Mutex::Lock's second argument;
// that word is 0xFFFFFFFF (scratchpad/waveG/eaxma_dump.txt) = EA::Thread::kTimeoutNone,
// i.e. Lock's default argument -- so the source form is a plain `Lock()` (the same
// reading waveE established for the identical unk_8214B050 site).
//
// LIST LINKAGE. The lists thread the PoolNode embedded in each ContextRecord: the asm
// stores the record address + 0x14 into the head/tail globals, and recovers the owning
// record with `node - 0x14`. Those are CONSOLE layout words -- on the x64 host the
// record's four pointers widen, so the node sits at offsetof(ContextRecord, mNode) = 40,
// not 0x14. Every conversion below therefore goes through &record->mNode / offsetof,
// never a literal displacement.
//
// The `_savegprlr_19` / `_restgprlr_19` calls in the pseudocode are the compiler's
// register save/restore prologue/epilogue helpers -- not source-level calls -- so they
// are dropped.
// =====================================================================================

#include "rw/audio/core/EaXmaDec.h"

#include <eathread/eathread_mutex.h> // vendor EA::Thread::Mutex (see the System.cpp vendor-mutex note)

#include <cstddef> // offsetof
#include <cstring> // memset
#include <new>     // placement new (the vptr install over the raw allocation)

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// CreateInstanceEvent @0x82B93C98
// DecoderDesc "create instance" callback (+0x04 slot), run over the raw GetSize()
// allocation after the framework has seeded the Decoder base fields (notably
// mucChannelCount). It installs the EaXmaDec identity, lays the per-pair table into the
// instance's aligned tail, and claims one shared hardware-context record per stereo pair.
//
// Order matters and follows the asm exactly:
//  * the channel budget is read from mucChannelCount (+0x2E) BEFORE the vptr store
//    (`lbz r27, 0x2E(r31)` @0x82B93CBC precedes `stw r11, 0(r31)` @0x82B93CC4) -- the
//    placement-new below is that single vptr store (`off_8215A928`), so caching first is
//    what keeps the read faithful;
//  * the tail carve is `(this + 0x5F) & ~7` on the console = align8(this + 0x58), i.e.
//    the first 8-aligned address past the FIXED header. 0x58/0x1C/28 are CONSOLE layout
//    words: the host header and the pointer-bearing DecPair records are wider, so the
//    carve and the memset are written in host sizeof form (this must agree with GetSize,
//    which sizes the allocation the same way -- console literals here would put the pair
//    table inside the object and corrupt memory);
//  * the claim loop keeps running after a failure (it does not break) and the channel
//    budget is only consumed by a successful claim -- both faithful.
// Returns true when every pair got a record; on any failure every record claimed by this
// instance is returned to the free list and false is returned.
// -------------------------------------------------------------------------------------
bool EaXmaDec::CreateInstanceEvent(EaXmaDec *pSelf)
{
    // r27 -- read before the vptr store (see above).
    u32 uChannelBudget = pSelf->mucChannelCount;

    // The asm's lone `stw off_8215A928, 0(r31)`: install the EaXmaDec vtable over the
    // raw allocation. EaXmaDec's implicit default constructor writes nothing else (the
    // Decoder base has no constructor body and no member is initialised), so this is
    // store-for-store the single word the binary writes.
    new (static_cast<void *>(pSelf)) EaXmaDec;

    const u32 uPairCount = (uChannelBudget + 1) >> 1; // ceil(channels / 2)

    // Host form of `(this + 0x5F) & 0xFFFFFFF8`.
    pSelf->mpPairs = reinterpret_cast<DecPair *>(
        (reinterpret_cast<usize>(pSelf) + sizeof(EaXmaDec) + 7) & ~static_cast<usize>(7));

    pSelf->miSamplesRemaining = 0;    // +0x48
    pSelf->miPendingInputBytes = 0;   // +0x3C
    pSelf->miOutputSampleBias = 0;    // +0x4C
    pSelf->mbFirstDecode = true;      // +0x55
    pSelf->mbRecoveryMode = false;    // +0x54
    pSelf->muPairCount = uPairCount;  // +0x44

    // X360: `memset(pairs, 0, 0x1C * uPairCount)` -- host stride.
    std::memset(pSelf->mpPairs, 0, sizeof(DecPair) * uPairCount);

    bool bFailed = false;
    for (u32 uPairIndex = 0; uPairIndex < uPairCount; ++uPairIndex)
    {
        spPoolMutex->Lock(); // kTimeoutNone (see the file header)

        // Pop the free-list HEAD.
        PoolNode *lpNode = spFreeHead;
        if (lpNode)
        {
            PoolNode *lpNext = lpNode->mpNext;
            spFreeHead = lpNext;
            if (lpNext)
                lpNext->mpPrev = 0;
            else
                spFreeTail = 0;
            --suFreeCount;
        }

        if (lpNode)
        {
            // Push it onto the in-use list HEAD.
            lpNode->mpPrev = 0;
            lpNode->mpNext = spInUseHead;
            if (spInUseHead)
                spInUseHead->mpPrev = lpNode;
            spInUseHead = lpNode;

            spPoolMutex->Unlock();

            DecPair &lPair = pSelf->mpPairs[uPairIndex];

            // Recover the owning record from the linked node (console: `node - 0x14`).
            lPair.mpRecord = reinterpret_cast<ContextRecord *>(
                reinterpret_cast<u8 *>(lpNode) - offsetof(ContextRecord, mNode));

            // Spend the channel budget: a full stereo pair takes two, a trailing mono
            // pair takes the last one (and leaves the budget alone).
            if (uChannelBudget <= 1)
            {
                lPair.mucChannels = 1;
            }
            else
            {
                uChannelBudget -= 2;
                lPair.mucChannels = 2;
            }
        }
        else
        {
            spPoolMutex->Unlock();
            bFailed = true;
        }
    }

    if (bFailed)
    {
        // Unwind: hand every record this instance did claim back to the pool. The lock is
        // taken and released once per record, exactly as the asm does it.
        for (u32 uPairIndex = 0; uPairIndex < uPairCount; ++uPairIndex)
        {
            DecPair &lPair = pSelf->mpPairs[uPairIndex];
            if (lPair.mpRecord)
            {
                spPoolMutex->Lock();

                PoolNode *lpNode = &lPair.mpRecord->mNode;

                // Unlink from the in-use list: fix the head first, then the generic
                // neighbour relinking.
                if (lpNode == spInUseHead)
                    spInUseHead = spInUseHead->mpNext;
                if (lpNode->mpPrev)
                    lpNode->mpPrev->mpNext = lpNode->mpNext;
                if (lpNode->mpNext)
                    lpNode->mpNext->mpPrev = lpNode->mpPrev;

                // Append to the free-list TAIL. The emptiness test reads spFreeHead while
                // the link is written through spFreeTail -- faithful to the asm's five
                // stores (the same idiom AllocateResources and ReleaseEvent use).
                lpNode->mpNext = 0;
                lpNode->mpPrev = spFreeTail;
                if (spFreeHead)
                    spFreeTail->mpNext = lpNode;
                else
                    spFreeHead = lpNode;
                spFreeTail = lpNode;
                ++suFreeCount;

                spPoolMutex->Unlock();
            }
        }
        return false;
    }

    return true;
}

// -------------------------------------------------------------------------------------
// ReleaseEvent @0x82B8F7C0
// DecoderDesc "release instance" callback (+0x08 slot): return every pair's shared
// hardware-context record to the pool. Unlike CreateInstanceEvent's unwind path this
// takes the pool mutex ONCE, around the whole loop, and dereferences each pair's record
// WITHOUT a null check -- both are what the binary does, and both are reproduced here.
// The pair's mpRecord is not cleared afterwards (the instance is being torn down).
// -------------------------------------------------------------------------------------
void EaXmaDec::ReleaseEvent()
{
    spPoolMutex->Lock(); // kTimeoutNone (see the file header)

    for (u32 uPairIndex = 0; uPairIndex < muPairCount; ++uPairIndex)
    {
        PoolNode *lpNode = &mpPairs[uPairIndex].mpRecord->mNode;

        // Unlink from the in-use list (head fix, then generic relinking).
        if (lpNode == spInUseHead)
            spInUseHead = spInUseHead->mpNext;
        if (lpNode->mpPrev)
            lpNode->mpPrev->mpNext = lpNode->mpNext;
        if (lpNode->mpNext)
            lpNode->mpNext->mpPrev = lpNode->mpPrev;

        // Append to the free-list TAIL (same five stores as above: the test reads
        // spFreeHead, the link is written through spFreeTail).
        lpNode->mpNext = 0;
        lpNode->mpPrev = spFreeTail;
        if (spFreeHead)
            spFreeTail->mpNext = lpNode;
        else
            spFreeHead = lpNode;
        spFreeTail = lpNode;

        ++suFreeCount;
    }

    spPoolMutex->Unlock();
}

// -------------------------------------------------------------------------------------
// ~EaXmaDec @0x82B93ED0 (exported as `vector deleting destructor', vtable slot [1])
// The X360 thunk reinstalls the Decoder base vtable (off_8214B1CC -- the inlined trivial
// ~Decoder) and, when the deleting flag is set, calls operator delete. Per the
// Decoder-family convention already applied in Decoder.cpp and HELPER_CMpegBase.cpp
// (native-C++ codecs, as opposed to the PlugIn family's explicit VectorDeletingDestructor
// members), that whole thunk is compiler-generated from this virtual destructor, so only
// the source destructor is written -- and its body is empty in the binary.
//
// In particular the pool records are NOT returned here: the asm's only stores are the
// vtable word and the conditional free. Handing the records back is ReleaseEvent's job.
// -------------------------------------------------------------------------------------
EaXmaDec::~EaXmaDec()
{
}

} // namespace core
} // namespace audio
} // namespace rw

// FOLDED FROM EaXmaDec_wG_03.cpp
// =====================================================================================
// rw::audio::core::EaXmaDec bodies (part 3 of the TU): the decode pump.
//
//   DecodeEvent       @0x82B96380
//   CopySamplesOut    @0x82B8FC18
//   EnterRecoveryMode @0x82B96148
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width, branch and side-effect. No LegacySrc source and no DecFIGS DWARF exist
// for this TU. The pool side of the codec, the leaf bit-stream helpers and the static
// member definitions live in EaXmaDec.cpp; the instance lifecycle in EaXmaDec_wG_02.cpp;
// the input pump (Service / StartPair / Reset) in EaXmaDec_wG_04.cpp.
//
// CROSS-CUTTING NOTES (they recur below and are not repeated at every site):
//  * `_savegprlr_17` / `_restgprlr_17` in the listing are the compiler's register
//    save/restore helpers, not source-level calls, so they are dropped.
//  * `twllei rX, 0` immediately after a `divwu` is the PPC compiler's divide-by-zero trap
//    guard emitted for the plain C division; the divisions are written normally.
//  * `dcbt` / `dcbz` are cache-management hints with no architectural data effect here
//    (`dcbz` only primes lines the code then fully overwrites); they are not reproduced.
//  * The per-pair table is indexed with the console stride 0x1C in the asm (`addi r,r,0x1C`
//    walking in lockstep with the pair index). That literal is the CONSOLE sizeof(DecPair);
//    on the x64 host the record's pointers widen, so every access here is `mpPairs[index]`
//    -- reproducing the byte stride would walk off the table and corrupt memory.
//  * The decoded PCM sitting in a hardware output ring is produced by the XMA hardware
//    decoder, so it is read as NATIVE s16 (no byte swap). Only the encoded XMA stream words
//    that Service/StartPair pull out of request data are serialized big-endian.
// =====================================================================================

#include "rw/audio/core/EaXmaDec.h"

#include "SDKs/XAudio/XmaHardware.h" // XmaContext + the XMA* hardware HAL

#include <cstring> // memset

// Platform tick source (XDK / Win32); declared in the established platform-API style, cf.
// SDKs/XCam/XCamPacketizer.h and SDKs/XCam/XCamQOSController.h.


namespace rw
{
namespace audio
{
namespace core
{

// s16 -> f32 scale, the rodata word flt_820ADBE8 (= 1/32768) the VMX path expresses as
// `vcfsx v,v,15` and the scalar tails multiply in explicitly. Same constant and name as
// the committed Pcm16BigDec.cpp.
static const f32 KF_PCM16_SCALE = 1.0f / 32768.0f;

// Decoded-PCM ring size, bytes (one hardware output buffer; 0x1800 in the asm, and the
// 0x18 = 6144/256 block count Reset programs into XmaContextInit).
static const s32 KI_OUTPUT_RING_BYTES = 6144;

// XMA frame length in samples; the request start/end sample arithmetic is quantised to it.
static const s32 KI_XMA_FRAME_SAMPLES = 512;

// Sample bias applied to a request that does not carry the "already aligned" flag
// (muReserved10): three quarters of a frame of decoder priming.
static const s32 KI_XMA_PRIME_SAMPLES = 384;

// -------------------------------------------------------------------------------------
// CopySamplesOut @0x82B8FC18
//
// Convert decoded s16 PCM out of one hardware output ring into planar f32, de-interleaving
// a stereo pair into pOutLeft/pOutRight (mono writes pOutLeft only and never touches
// pOutRight). The ring wrap is handled by two segments: `iFramesFirst` frames read from
// pRing + (iRingByteOffset & ~1), then `iFramesSecond` frames read from the ring base.
//
// A member in the binary (every call site passes the instance in r3) but the asm overwrites
// r3 before use, so `this` is unreferenced -- the same shape the committed XMAmemcpy has.
//
// DE-VECTORISATION JUSTIFICATION. The X360 body is hand-written VMX: 128-byte unaligned
// block loads (lvlx/lvrx + vor), a `vperm` de-interleave, vupkhsh/vupklsh sign-extension
// and `vcfsx v,v,15` fixed-point conversion, with stvlx/stvrx unaligned stores. The scalar
// loops below are NOT a paraphrase of that -- they are the binary's OWN remainder tails,
// which every vector loop falls through to:
//   * mono  tail @0x82B8FD60 / @0x82B8FE08: `lha` (signed s16 load) -> fcfid -> frsp ->
//     `fmuls f13, f13, flt_820ADBE8` -> `stfs`, cursor += 2, output += 4;
//   * stereo tail @0x82B900FC: two `lha`s from consecutive halfwords, the first stored to
//     r11 (pOutLeft), the second to r8 (pOutRight), both advancing by 4.
// and `vcfsx v,v,15` is exactly a multiply by 2^-15 = flt_820ADBE8, while the vperm mask
// built on the stack (00 01 04 05 08 09 0C 0D | 02 03 06 07 0A 0B 0E 0F, stored as the
// words 0x00010405/0x08090C0D/0x02030607/0x0A0B0E0F @0x82B8FEB4..) is exactly the stereo
// L/R de-interleave the scalar tail performs pairwise. So the per-sample math and the
// element order are attested identical by the same function; the vector loops are a
// throughput optimisation over them. Same treatment the committed XMAmemcpy applies to its
// pure VMX block copy.
//
// The ring holds hardware-decoder output, so the s16s are read natively (no byte swap):
// this is decoded PCM in memory, not serialized resource data.
// -------------------------------------------------------------------------------------
void EaXmaDec::CopySamplesOut(f32 *pOutLeft, f32 *pOutRight, const s16 *pRing,
                              s32 iRingByteOffset, s32 iNumChannels, s32 iFramesFirst,
                              s32 iFramesSecond)
{
    // `srawi r8, r7, 1; slwi r8, r8, 1` -- the byte offset is forced even (it indexes s16
    // frames) before being added to the ring base, in both the mono and the stereo path.
    const s16 *lpSource = reinterpret_cast<const s16 *>(
        reinterpret_cast<const u8 *>(pRing) + (iRingByteOffset & ~1));

    if (iNumChannels == 1)
    {
        // Mono is the asm's fall-through path (`cmpwi cr6, r8, 1; bne cr6, <stereo>`).
        for (s32 iFrame = 0; iFrame < iFramesFirst; ++iFrame)
            *pOutLeft++ = static_cast<f32>(*lpSource++) * KF_PCM16_SCALE;

        lpSource = pRing; // segment 2 restarts at the ring base (`mr r9, r6`)
        for (s32 iFrame = 0; iFrame < iFramesSecond; ++iFrame)
            *pOutLeft++ = static_cast<f32>(*lpSource++) * KF_PCM16_SCALE;
    }
    else
    {
        for (s32 iFrame = 0; iFrame < iFramesFirst; ++iFrame)
        {
            *pOutLeft++ = static_cast<f32>(*lpSource++) * KF_PCM16_SCALE;
            *pOutRight++ = static_cast<f32>(*lpSource++) * KF_PCM16_SCALE;
        }

        lpSource = pRing; // segment 2 restarts at the ring base (`mr r9, r6`)
        for (s32 iFrame = 0; iFrame < iFramesSecond; ++iFrame)
        {
            *pOutLeft++ = static_cast<f32>(*lpSource++) * KF_PCM16_SCALE;
            *pOutRight++ = static_cast<f32>(*lpSource++) * KF_PCM16_SCALE;
        }
    }
}

// -------------------------------------------------------------------------------------
// EnterRecoveryMode @0x82B96148
//
// The hardware has stalled (DecodeEvent's wedge probe or its stall timeout fired). Latch
// recovery mode -- DecodeEvent then zero-fills its output instead of decoding -- re-arm
// every hardware context with the current stream mode, drop all per-pair streaming state,
// step the request read cursor one slot past the decode cursor, and re-prime the input
// pump.
// -------------------------------------------------------------------------------------
void EaXmaDec::EnterRecoveryMode()
{
    mbRecoveryMode = true;
    Reset(muStreamMode);

    // The asm walks the pair table by pointer, re-deriving `mpPairs + 0x1C * muPairCount`
    // as the loop bound on every iteration; neither operand is written in the loop, so the
    // index form below is the same walk (and avoids the console stride -- see the header
    // note). Store order is exactly the asm's.
    for (u32 uPair = 0; uPair < muPairCount; ++uPair)
    {
        DecPair &lPair = mpPairs[uPair];
        lPair.miInputBytesRemaining = 0;
        lPair.muRingReadOffset = 0;
        lPair.mbNextInputIsBuffer1 = false;
        lPair.muSamplesQueued = 0;
        lPair.mpInputCursor = 0;
    }

    // The asm stores mucRequestDecodeIndex into mucRequestReadIndex and then immediately
    // stores the incremented byte over it (@0x82B961B8 / @0x82B961CC) -- the intermediate
    // value is never observable, so the two stores are collapsed into one.
    mucRequestReadIndex = static_cast<u8>(mucRequestDecodeIndex + 1);
    if (mucRequestReadIndex >= mucRequestCount)
        mucRequestReadIndex = 0;

    miPendingInputBytes = 0;
    Service();
}

// -------------------------------------------------------------------------------------
// DecodeEvent @0x82B96380
//
// The decode pump, and the codec's DecoderDesc decode callback (desc slot +0x0C). Produces
// up to iNumSamples planar f32 samples per channel into pOutput, one request-chunk at a
// time: pull the next request when the current one is exhausted, drain each pair's hardware
// output ring through CopySamplesOut, keep the hardware read offset re-synced to the
// software cursor, pump Service when a ring is short, and fall into recovery mode (zero
// fill) when the hardware wedges or stalls past suRecoveryTimeoutMs. Returns the sample
// count produced.
//
// iCounted (r29) and iRecoveryReason (r18) are initialised ONCE at function entry, not per
// chunk: r29 carries the last chunk's produced-sample count into the next iteration's
// `iProduced += iCounted` when a chunk copies nothing, and r18 latches the recovery reason
// across chunks. Both are function-scope here to match.
// -------------------------------------------------------------------------------------
s32 EaXmaDec::DecodeEvent(DecoderBuffer *pOutput, s32 iNumSamples)
{
    if (!iNumSamples)
        return 0;

    s32 iProduced = 0;           // r20 -- total samples handed back
    s32 iCounted = 0;            // r29 -- samples the last chunk actually produced
    s32 iRecoveryReason = 0;     // r18 -- 1 = stall timeout, 2 = wedged context
    s32 iRemaining = iNumSamples; // r19

    do
    {
        if (!miSamplesRemaining)
        {
            // Pull the request at the decode cursor. The asm nulls the pointer when the
            // slot is empty (miEndSample == 0) and then dereferences it UNCONDITIONALLY
            // three instructions later (`lbz r11, 0x10(r7)` @0x82B963EC, `lwz r11, 8(r7)`
            // @0x82B96408). That is the shape the binary ships -- the null is only ever a
            // marker for the mpCurrentRequest store below, and the caller is expected never
            // to reach here with an empty slot -- so it is reproduced verbatim rather than
            // "fixed" with a guard the hardware build does not have.
            DecoderRequest *lpRequest = &RequestQueue()[mucRequestDecodeIndex];
            if (!lpRequest->miEndSample)
                lpRequest = 0;

            // ⭐ CONFIRMED AND NAMED 2026-08-28 (phase E). This TU had already deduced from
            // its own `lbz` that +0x10 is a single BYTE, and noted that the then-frozen
            // Decoder.h modelled it as the ring slot's word. Raw-decoding the ring's
            // producer, Decoder::Feed @0x82B67920, settles both the width (it writes it
            // with `stb`) and the meaning: the sound players pass "this is NOT a decoder
            // reset", i.e. mucContinue means the chunk continues the current stream.
            // That reading is exactly what this arithmetic wants -- a continuing stream is
            // already primed, and only a fresh one needs the XMA priming samples added.
            const s32 iBias = lpRequest->mucContinue ? 0 : KI_XMA_PRIME_SAMPLES;
            const s32 iStartSample = lpRequest->miStartSample;

            // Signed division (`srawi`+`addze`+`slwi`): the offset of the request's start
            // sample inside its 512-sample XMA frame, plus the bias carried over from the
            // previous request's tail.
            const s32 iQueueDelta =
                iStartSample -
                ((iStartSample + iBias) / KI_XMA_FRAME_SAMPLES) * KI_XMA_FRAME_SAMPLES +
                iBias + miOutputSampleBias;

            for (u32 uPair = 0; uPair < muPairCount; ++uPair)
                mpPairs[uPair].muSamplesQueued += static_cast<u32>(iQueueDelta);

            miOutputSampleBias = 0;
            mpCurrentRequest = lpRequest;
            mbRecoveryMode = false;
            miSamplesRemaining = lpRequest->miEndSample - lpRequest->miStartSample;
        }

        // This chunk: whatever is left of the request, capped by what the caller still wants.
        s32 iChunk = miSamplesRemaining; // r26
        if (iRemaining <= iChunk)
            iChunk = iRemaining;

        u32 uPairIndex = 0; // r24
        u32 uRetry = 0;     // r27 -- 0 or 1 only; see the tick anchor note below

        if (!mbRecoveryMode)
        {
            // The pair walk does not advance while uRetry is set: a pair whose ring was
            // short is re-examined after Service until it fills, wedges or times out.
            while (uPairIndex < muPairCount || uRetry)
            {
                DecPair &lPair = mpPairs[uPairIndex];
                XmaContext *lpContext = lPair.mpRecord->mpContext;

                // Bytes the hardware has written past the software read cursor. The write
                // offset is reported in 256-byte units (`slwi r11, r3, 8`).
                s32 iAvailableBytes = static_cast<s32>(XMAGetOutputBufferWriteOffset(lpContext) << 8) -
                                      static_cast<s32>(lPair.muRingReadOffset);
                const u32 uHardwareReadOffset = XMAGetOutputBufferReadOffset(lpContext);

                // Wrapped (or exactly full) ring: the writer is behind the reader in the
                // linear sense, so add a whole ring. Not on the very first decode, and not
                // when a zero difference is merely an invalidated (drained) buffer.
                if (iAvailableBytes <= 0 && !mbFirstDecode &&
                    (iAvailableBytes != 0 || !XMAIsOutputBufferValid(lpContext)))
                {
                    iAvailableBytes += KI_OUTPUT_RING_BYTES;
                }

                // Discard frames the request-pull queued up for skipping (the start-sample
                // offset inside the first XMA frame), as far as the ring allows.
                if (lPair.muSamplesQueued != 0 && iAvailableBytes != 0)
                {
                    const u32 uChannels = lPair.mucChannels;
                    const u32 uQueuedBytes = 2 * uChannels * lPair.muSamplesQueued;

                    u32 uSkipBytes = static_cast<u32>(iAvailableBytes);
                    if (uSkipBytes > uQueuedBytes)
                        uSkipBytes = uQueuedBytes;

                    const u32 uAdvanced = lPair.muRingReadOffset + uSkipBytes;
                    lPair.muRingReadOffset = uAdvanced;
                    if (uAdvanced >= static_cast<u32>(KI_OUTPUT_RING_BYTES))
                        lPair.muRingReadOffset = uAdvanced - KI_OUTPUT_RING_BYTES;

                    iAvailableBytes -= static_cast<s32>(uSkipBytes);
                    lPair.muSamplesQueued -= uSkipBytes / (2 * uChannels);
                }

                const s32 iChannels = lPair.mucChannels;
                if (iAvailableBytes >= 2 * iChannels * iChunk && iChunk != 0)
                {
                    const u32 uRingOffset = lPair.muRingReadOffset;
                    uRetry = 0;

                    // Frames available before the ring wraps; anything past that is read
                    // from the ring base as a second segment.
                    const s32 iFramesToWrap =
                        static_cast<s32>((static_cast<u32>(KI_OUTPUT_RING_BYTES) - uRingOffset) /
                                         static_cast<u32>(2 * iChannels));

                    s32 iFramesSecond = 0;
                    s32 iFramesFirst = iChunk;
                    if (iChunk > iFramesToWrap)
                    {
                        iFramesSecond = iChunk - iFramesToWrap;
                        iFramesFirst = iFramesToWrap;
                    }

                    // Planar destination for this pair's channels. A pair carries 1 or 2
                    // channels (CreateInstanceEvent), which is why two slots suffice; the
                    // console reserves a larger stack scratch array and fills mucChannels
                    // of it. NOTE the attested asymmetry: these pointers take NO iProduced
                    // offset, while the recovery memset below DOES -- so on a chunk that
                    // decodes, the second and later chunks of one call overwrite the first.
                    // The asm builds them from pOutput alone (@0x82B965D8..@0x82B96608).
                    f32 *alpChannelOut[2] = {0, 0};
                    if (lPair.mucChannels != 0)
                    {
                        f32 *lpChannel = pOutput->mpData + 2u * pOutput->muStride * uPairIndex;
                        for (u32 uChannel = 0; uChannel < lPair.mucChannels; ++uChannel)
                        {
                            alpChannelOut[uChannel] = lpChannel;
                            lpChannel += pOutput->muStride;
                        }
                    }

                    CopySamplesOut(alpChannelOut[0], alpChannelOut[1],
                                   reinterpret_cast<const s16 *>(lPair.mpRecord->mpOutputBuffer),
                                   static_cast<s32>(uRingOffset), iChannels, iFramesFirst,
                                   iFramesSecond);

                    if (iFramesSecond)
                        lPair.muRingReadOffset = static_cast<u32>(2 * lPair.mucChannels * iFramesSecond);
                    else
                        lPair.muRingReadOffset =
                            uRingOffset + static_cast<u32>(2 * lPair.mucChannels * iFramesFirst);

                    iCounted = iChunk;
                }
                else
                {
                    uRetry = 1;
                    Service();
                }

                // Re-sync the hardware read offset to the software cursor. The hardware
                // counts in subframes: 4 * (bytes / (channels * 1024)) * channels.
                const u32 uCurrentOffset = lPair.muRingReadOffset;
                u32 uNewReadOffset;
                if (uCurrentOffset == static_cast<u32>(KI_OUTPUT_RING_BYTES))
                {
                    uNewReadOffset = 0;
                    lPair.muRingReadOffset = 0;
                }
                else
                {
                    const u32 uChannels = lPair.mucChannels;
                    uNewReadOffset = 4 * (uCurrentOffset / (uChannels * 1024)) * uChannels;
                }

                if (uHardwareReadOffset != uNewReadOffset)
                {
                    XMADisableContext(lpContext, 1);
                    XMASetOutputBufferReadOffset(lpContext, uNewReadOffset);
                    XMASetOutputBufferValid(lpContext);
                    XMAEnableContext(lpContext);
                }

                if (uRetry)
                {
                    // PROBABLE SHIPPED BUG, reproduced: the elapsed-time anchor is uRetry
                    // itself, which is the constant 1 on this path, so the binary computes
                    // GetTickCount() - 1 (`subf r30, r27, r3` @0x82B966F8) rather than the
                    // difference from a start tick. The stall timeout below therefore fires
                    // on the very first retry (tick-1 >= 4 ms is true for any live tick).
                    // The asm has no other write to r27, so this is what shipped.
                    const u32 uElapsedMs = GetTickCount() - uRetry;

                    XMAEnableContext(lpContext);

                    // Hardware-wedge probe: a released context, or either input-buffer
                    // status byte with bit 0x80 set (`clrrwi.` keeps only that bit).
                    XmaContext *lpProbe = lPair.mpRecord->mpContext;
                    bool bWedged = true;
                    if (lpProbe && !(lpProbe->mucStatus08 & 0x80) && !(lpProbe->mucStatus0C & 0x80))
                        bWedged = false;

                    if (bWedged)
                    {
                        iRecoveryReason = 2;
                        EnterRecoveryMode();
                        break;
                    }

                    if (uElapsedMs >= suRecoveryTimeoutMs)
                        iRecoveryReason = 1;

                    if (iRecoveryReason)
                    {
                        EnterRecoveryMode();
                        break;
                    }
                    // Otherwise retry this same pair -- uPairIndex is deliberately not
                    // advanced (the asm branches straight back to the loop head).
                }
                else
                {
                    ++uPairIndex;
                }
            }
        }

        if (mbRecoveryMode)
        {
            // Nothing was decoded: claim the whole chunk and hand back silence. Unlike the
            // copy path above, this one DOES bias the destination by iProduced -- the
            // asymmetry is in the binary (`add r11, r11, r20` @0x82B9679C).
            iCounted = iChunk;
            for (u32 uChannel = 0; uChannel < mucChannelCount; ++uChannel)
            {
                std::memset(pOutput->mpData + pOutput->muStride * uChannel + iProduced, 0,
                            static_cast<usize>(iChunk) * sizeof(f32));
            }
        }

        iProduced += iCounted;
        iRemaining -= iChunk;
        miSamplesRemaining -= iChunk;

        // Request boundary: carry the alignment slack of the request's end sample into the
        // next request, so the next start lands on an XMA frame boundary again.
        if (miSamplesRemaining == 0 && iChunk != 0)
        {
            DecoderRequest *lpRequest = mpCurrentRequest;
            s32 iEndSample = lpRequest->miEndSample;
            // Same rule as above: only a chunk that does NOT continue the current stream
            // carries the XMA priming samples (see Decoder::Feed's mucContinue).
            if (!lpRequest->mucContinue)
                iEndSample += KI_XMA_PRIME_SAMPLES;

            const s32 iFramePhase = iEndSample & (KI_XMA_FRAME_SAMPLES - 1); // `clrlwi. r11,r11,23`
            if (iFramePhase)
                miOutputSampleBias = KI_XMA_FRAME_SAMPLES - iFramePhase;

            if (mbRecoveryMode)
                miOutputSampleBias = 0;
        }

        Service();
    } while (iRemaining);

    return iProduced;
}

} // namespace core
} // namespace audio
} // namespace rw

// FOLDED FROM EaXmaDec_wG_04.cpp
// =====================================================================================
// rw::audio::core::EaXmaDec -- input-streaming bodies (part 4 of the TU).
//
//   Service   @0x82B95EB8   (this file)
//   StartPair @0x82B95A70   (this file)
//   Reset     @0x82B8F8D0   (this file)
//
// The pool side of the codec plus the leaf helpers live in EaXmaDec.cpp; the instance
// lifecycle lives in EaXmaDec_wG_02.cpp and the decode pump in EaXmaDec_wG_03.cpp. The
// class, its nested records and the shared pool statics are declared/defined there --
// see rw/audio/core/EaXmaDec.h.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width, branch and side-effect. No LegacySrc source and no DecFIGS DWARF exist
// for this TU. The `_savegprlr_* / _restgprlr_*` calls in the pseudocode are the
// compiler's register save/restore helpers, not source-level calls, so they are dropped.
//
// THESE THREE BODIES TOGETHER ARE THE INPUT SIDE of the codec: Service tops up the two
// 2048-byte hardware input buffers of every stereo pair and, when all queued input has
// been handed over, pulls the next Decoder request and re-seeds every pair from it via
// StartPair; Reset (re)programs the XMA hardware contexts from the pool records.
//
// ENDIANNESS. Everything Service/StartPair read out of the request payload -- the leading
// slice-size word, the XMA packet headers and the seek-table entries -- is SERIALIZED
// big-endian resource data, so it is assembled byte-wise through ReadU32BE below (the
// console's plain `lwz` was already big-endian; StartPair's own asm spells the packet
// header out as four `lbz`/`stb` pairs). Nothing here reads decoded PCM.
//
// HOST-vs-CONSOLE SIZES. The pair table is indexed as mpPairs[i] and the hardware init
// record is zeroed with sizeof(): the console's 0x1C pair stride and 0x38 init-record
// size are 32-bit layout words that do not survive the x64 widening; they are kept only
// in the comments. Every constant that DOES survive is a stream/hardware format value
// (the 2048-byte XMA packet, the 512-sample XMA frame, the 0x3FE0-bit packet payload,
// the 4-byte serialized words), noted at each site.
// =====================================================================================

#include "rw/audio/core/EaXmaDec.h"

#include "rw/audio/core/Unpack0.h"  // Unpack0::UnpackUInt32 (packed seek-table varints)
#include "SDKs/XAudio/XmaHardware.h" // XmaContext / XmaContextInit + the XMA* HAL

#include <cstring> // memset (the console's stack-record zero fill)

namespace rw
{
namespace audio
{
namespace core
{

namespace
{

// -------------------------------------------------------------------------------------
// Read one 32-bit big-endian word out of a serialized XMA byte stream (request payload,
// packet header or seek table). On the X360 these were plain word loads (and, for the
// packet header, four byte loads reassembled in address order -- see StartPair's asm at
// 0x82B95AB4..0x82B95AD8); on the little-endian host the bytes must be combined
// explicitly. Same convention as the committed EaXmaDec::LeftShiftInBits.
// -------------------------------------------------------------------------------------
inline u32 ReadU32BE(const u8 *pBytes)
{
    return (static_cast<u32>(pBytes[0]) << 24) | (static_cast<u32>(pBytes[1]) << 16) |
           (static_cast<u32>(pBytes[2]) << 8) | static_cast<u32>(pBytes[3]);
}

} // namespace

// -------------------------------------------------------------------------------------
// Service @0x82B95EB8
// The codec's input pump (also the body behind the FeedEvent vtable slot).
//
// Each turn of the outer loop first makes a feed pass over every stereo pair: if BOTH of
// a pair's hardware input buffers still hold undecoded packets there is no room, so the
// context is merely (re-)enabled; otherwise the pair's remaining encoded bytes are handed
// over <=2048 bytes at a time, alternating between input buffer 0 and 1, until either the
// pair runs dry or the buffer that is next in line is still marked valid. A first-frame
// bit offset left behind by StartPair is programmed into the hardware with the feed that
// carries its packet.
//
// When the feed pass has drained everything that was queued (miPendingInputBytes == 0)
// the pump takes the next request off the Decoder ring, resets the hardware from the
// stream's mode bits on the very first request, and re-seeds every pair from the request
// payload via StartPair -- then loops back to feed the freshly queued bytes. It returns
// when input is still pending or the request ring is empty.
// -------------------------------------------------------------------------------------
void EaXmaDec::Service()
{
    for (;;)
    {
        // ---- feed pass over every stereo pair -----------------------------------------
        // (the console re-reads muPairCount each turn; the pair table is walked by the
        // console 0x1C stride, expressed here as the host mpPairs[] index.)
        for (u32 uPair = 0; uPair < muPairCount; ++uPair)
        {
            DecPair &lPair = mpPairs[uPair];

            // No null check on mpRecord anywhere in this pump -- faithful to the asm.
            if (XMAIsInputBuffer0Valid(lPair.mpRecord->mpContext) &&
                XMAIsInputBuffer1Valid(lPair.mpRecord->mpContext))
            {
                // Both packet buffers are still loaded: nothing to feed, just keep the
                // hardware context running.
                XMAEnableContext(lPair.mpRecord->mpContext);
            }
            else
            {
                while (lPair.miInputBytesRemaining)
                {
                    XmaContext *lpContext = lPair.mpRecord->mpContext;

                    if (lPair.mbNextInputIsBuffer1)
                    {
                        // The buffer next in line has not been consumed yet: stop feeding
                        // this pair (the console `bne loc_82B95F1C` leaves the while).
                        if (XMAIsInputBuffer1Valid(lpContext))
                            break;

                        // 2048 = one XMA packet, the hardware input-buffer size. The
                        // console compares UNSIGNED (`cmplwi 0x800`).
                        u32 uChunk = static_cast<u32>(lPair.miInputBytesRemaining);
                        if (uChunk >= 2048u)
                            uChunk = 2048u;

                        XMAmemcpy(lPair.mpRecord->mpInputBuffer1, lPair.mpInputCursor,
                                  static_cast<s32>(uChunk));
                        XMADisableContext(lPair.mpRecord->mpContext, 1);
                        XMASetInputBuffer1Valid(lPair.mpRecord->mpContext);

                        // A pending first-frame bit offset belongs to the packet that has
                        // just been loaded; +32 skips the packet's own 4-byte header.
                        if (lPair.miPendingBitOffset > 0)
                        {
                            XMASetInputBufferReadOffset(
                                lPair.mpRecord->mpContext,
                                static_cast<u32>(lPair.miPendingBitOffset + 32));
                            lPair.miPendingBitOffset = 0;
                        }

                        XMAEnableContext(lPair.mpRecord->mpContext);

                        lPair.miInputBytesRemaining -= static_cast<s32>(uChunk);
                        miPendingInputBytes -= static_cast<s32>(uChunk);
                        lPair.mbNextInputIsBuffer1 = false;
                        lPair.mpInputCursor += uChunk;
                    }
                    else
                    {
                        if (XMAIsInputBuffer0Valid(lpContext))
                            break;

                        u32 uChunk = static_cast<u32>(lPair.miInputBytesRemaining);
                        if (uChunk >= 2048u)
                            uChunk = 2048u;

                        XMAmemcpy(lPair.mpRecord->mpInputBuffer0, lPair.mpInputCursor,
                                  static_cast<s32>(uChunk));
                        XMADisableContext(lPair.mpRecord->mpContext, 1);
                        XMASetInputBuffer0Valid(lPair.mpRecord->mpContext);

                        if (lPair.miPendingBitOffset > 0)
                        {
                            XMASetInputBufferReadOffset(
                                lPair.mpRecord->mpContext,
                                static_cast<u32>(lPair.miPendingBitOffset + 32));
                            lPair.miPendingBitOffset = 0;
                        }

                        XMAEnableContext(lPair.mpRecord->mpContext);

                        lPair.miInputBytesRemaining -= static_cast<s32>(uChunk);
                        miPendingInputBytes -= static_cast<s32>(uChunk);
                        lPair.mbNextInputIsBuffer1 = true;
                        lPair.mpInputCursor += uChunk;
                    }
                }
            }
        }

        // ---- pull the next request once everything queued has been handed over --------
        if (miPendingInputBytes)
            break;

        DecoderRequest *lpRequest = GetCurrentRequestDesc();
        if (!lpRequest)
            break;

        // Request+0x00 holds the pointer to this request's encoded XMA payload (the ring
        // entry used to be pinned to 20 bytes, which forced the pointer through a 32-bit word;
        // that wrinkle is retired -- the field is a real host pointer now, named by Decoder::Feed).
        const u8 *lpData =
            static_cast<const u8 *>(lpRequest->mpFedData);

        if (mbFirstDecode)
        {
            // The stream's leading serialized word carries the XMA stream mode in its low
            // two bits; the hardware is programmed with it once, on the first request.
            Reset(ReadU32BE(lpData) & 3u);
            mbFirstDecode = false;
        }

        // Each pair owns a consecutive slice of the payload; StartPair returns the slice's
        // byte size, by which the cursor advances to the next pair's slice.
        for (u32 uPair = 0; uPair < muPairCount; ++uPair)
        {
            lpData += StartPair(&mpPairs[uPair], uPair, lpRequest, lpData);
            miPendingInputBytes += mpPairs[uPair].miInputBytesRemaining;
        }
    }
}

// -------------------------------------------------------------------------------------
// StartPair @0x82B95A70
// Position one stereo pair at a request's start sample inside that pair's slice of the
// encoded payload, and report the slice's byte size.
//
// The slice begins with a serialized word whose top 30 bits are its byte size; the packed
// XMA data follows. The start sample is converted to an XMA frame index (512 samples per
// frame; requests that do not carry the "continuation" flag are padded by 384 samples of
// decoder priming). If the request supplies a seek table, this pair's entry is a packed
// run of per-packet frame counts (Unpack0::UnpackUInt32 varints) which is walked whole
// 2048-byte packets at a time until the target frame lies inside the current packet; the
// remainder is then walked frame by frame with ParseToNextFrame.
//
// The result is written back as the pair's input cursor, its first-frame bit offset (to
// be programmed by Service with the feed that carries this packet) and the number of
// encoded bytes still to feed -- the slice size less whatever the seek/parse walk skipped.
// -------------------------------------------------------------------------------------
s32 EaXmaDec::StartPair(DecPair *pPair, u32 uPairIndex, const DecoderRequest *pRequest,
                        const u8 *pPairData)
{
    // Leading serialized word (big-endian): this pair's slice size in bytes, in the top
    // 30 bits.
    const u32 uSliceBytes = ReadU32BE(pPairData) >> 2;

    // Target XMA frame within the stream. Signed division by the 512-sample frame (the
    // asm's `srawi 9` + `addze` is the compiler's round-toward-zero idiom, written here
    // as the plain division it came from). The flag word at Request+0x10 is read as a
    // BYTE by the console (`lbz 0x10`), i.e. the field is byte-sized in the source and
    // the modelled u32 only ever holds a 0/1 flag -- so the test is a plain truthiness
    // test (same reading as the committed Pcm16BigDec::DecodeEvent).
    s32 iTargetFrame = pRequest->miStartSample / 512;
    if (!pRequest->mucContinue)
        iTargetFrame = (pRequest->miStartSample + 384) / 512;

    const u8 *const lpSliceBase = pPairData + 4; // past the 4-byte serialized size word
    const u8 *lpCursor = lpSliceBase;

    // First-frame bit offset out of the XMA packet header: bits 11..25 of the packet's
    // leading big-endian word. 0x3FE0 is the packet's payload size in bits, so a header
    // pointing at or past it means "no frame starts in this packet" -> -1.
    s32 iBitOffset = static_cast<s32>((ReadU32BE(lpCursor) >> 11) & 0x7FFFu);
    if (iBitOffset >= 0x3FE0)
        iBitOffset = -1;

    if (pRequest->mpSeekData)
    {
        // Request+0x04 holds the seek table's base pointer, whose first words are per-pair
        // byte offsets into the packed frame-count runs.
        // ⭐ 2026-08-29: this used to read `muReserved04`, a u32, and launder it back with
        // `static_cast<usize>`. The comment here already knew it was a pointer ("again in a
        // 32-bit ring word") -- but on x64 the store side truncated it, so this base was
        // only ever going to be correct by accident. The field is a real `const u8 *` now
        // and the cast is gone; see the mpSeekData note in Decoder.h for the full chain.
        const u8 *const lpSeekBase = pRequest->mpSeekData;
        const u8 *lpPacked = lpSeekBase + ReadU32BE(lpSeekBase + 4 * uPairIndex);

        // Skip whole 2048-byte packets while the target frame lies beyond the running
        // frame total. The comparisons are SIGNED (`cmpw`).
        s32 iFramesSkipped = 0;
        u32 uFramesInPacket = 0;
        lpPacked += Unpack0::UnpackUInt32(lpPacked, &uFramesInPacket);
        if (iTargetFrame >= static_cast<s32>(uFramesInPacket))
        {
            do
            {
                iFramesSkipped += static_cast<s32>(uFramesInPacket);
                lpCursor += 2048;
                lpPacked += Unpack0::UnpackUInt32(lpPacked, &uFramesInPacket);
            }
            while (iTargetFrame >= static_cast<s32>(uFramesInPacket) + iFramesSkipped);
        }
        iTargetFrame -= iFramesSkipped;

        // Re-read the packet header at the (possibly advanced) cursor. The console does
        // this unconditionally inside the seek-table branch -- even when no packet was
        // skipped, in which case it recomputes the same value -- so it is kept as-is.
        iBitOffset = static_cast<s32>((ReadU32BE(lpCursor) >> 11) & 0x7FFFu);
        if (iBitOffset >= 0x3FE0)
            iBitOffset = -1;
    }

    // Walk the rest of the way frame by frame.
    while (iTargetFrame > 0)
    {
        ParseToNextFrame(&lpCursor, &iBitOffset);
        --iTargetFrame;
    }

    pPair->mpInputCursor = lpCursor;
    pPair->miPendingBitOffset = iBitOffset;
    // Bytes still to feed: the slice, less the 4-byte size word, less whatever the walk
    // skipped (lpSliceBase - lpCursor is <= 0).
    pPair->miInputBytesRemaining = static_cast<s32>(lpSliceBase - lpCursor) +
                                   static_cast<s32>(uSliceBytes) - 4;
    return static_cast<s32>(uSliceBytes);
}

// -------------------------------------------------------------------------------------
// Reset @0x82B8F8D0
// (Re)initialise every pair's XMA hardware context from its pool record: build the init
// record on the stack, stop the context, initialise it, arm the output ring and both
// encoded-input packet buffers, and start it again. The stream mode is remembered in
// muStreamMode so EnterRecoveryMode can replay this without the stream header.
//
// The console zeroes 0x38 stack bytes (seven doubleword stores) before filling the record
// in; on the host the record's pointer fields widen, so the fill is sizeof()-driven.
// -------------------------------------------------------------------------------------
void EaXmaDec::Reset(u32 uStreamMode)
{
    muStreamMode = uStreamMode;

    for (u32 uPair = 0; uPair < muPairCount; ++uPair)
    {
        DecPair &lPair = mpPairs[uPair];
        ContextRecord *lpRecord = lPair.mpRecord;

        XmaContextInit lInit;
        std::memset(&lInit, 0, sizeof(lInit)); // console: 0x38 bytes

        lInit.mpInputBuffer0 = lpRecord->mpInputBuffer0;
        lInit.muInputBuffer0Packets = 1;
        lInit.mpInputBuffer1 = lpRecord->mpInputBuffer1;
        lInit.muInputBuffer1Packets = 1;
        lInit.muUnknown10 = 0x20;
        lInit.mpOutputBuffer = lpRecord->mpOutputBuffer;
        lInit.muOutputBufferBlocks = 0x18; // the 6144-byte ring in 256-byte blocks
        lInit.mpOverlapBuffer = lpRecord->mpOverlapBuffer;
        lInit.muUnknown20 = 4;
        lInit.muChannelsMinusOne = static_cast<u32>(lPair.mucChannels) - 1;
        lInit.muStreamMode = uStreamMode;

        XMADisableContext(lpRecord->mpContext, 1);
        XMAInitializeContext(lpRecord->mpContext, &lInit);
        XMASetOutputBufferValid(lpRecord->mpContext);
        XMASetInputBuffer0(lpRecord->mpContext, lpRecord->mpInputBuffer0, 1);
        XMASetInputBuffer1(lpRecord->mpContext, lpRecord->mpInputBuffer1, 1);
        XMAEnableContext(lpRecord->mpContext);
    }
}

} // namespace core
} // namespace audio
} // namespace rw
