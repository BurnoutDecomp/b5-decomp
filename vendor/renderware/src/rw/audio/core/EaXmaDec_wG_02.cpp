// =====================================================================================
// rw::audio::core::EaXmaDec bodies -- part 2 of the TU: the INSTANCE LIFECYCLE.
//
// EARenderWare "rwaudio" XMA (Xbox 360 hardware-format) stream decoder plug-in.
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this
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
