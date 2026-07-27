#pragma once

#include <cstddef>   // offsetof
#include "types.hpp"

// =============================================================================
// CgsDispatcher.h  (GameShared/GameClasses/Graphics/Dispatch)
//
// Owning home for the CgsGraphics render-dispatch bin/bucket family. Layout is
// reconstructed from the BURNOUT_X360_ARTIST.XEX disassembly (authoritative for
// member offsets) cross-checked against the PS3 DWARF dump and the Feb-2007
// leak header for CgsDispatcher.h.
//
// X360-verified DispatchBin layout (word offsets from `this`, all 4 bytes):
//   0x00  mpMemoryCallback              void (*)(void*)
//   0x04  mpMemoryContext               void*
//   0x08  m_pBin                        DispatchCommand*   (a1[2])
//   0x0C  m_pNextWord                   DispatchCommand*   (a1[3])
//   0x10  m_pPacketStart                DispatchPacket*    (a1[4])
//   0x14  m_pLastCommandInPacket        DispatchCommand*   (a1[5])
//   0x18  m_uSize                       uint32_t           (a1[6], in quad-words)
//   0x1C  m_pActiveAllocateMemoryBlock  void*              (a1[7])
//   0x20  m_uSizeUsedLastTime           uint32_t           (a1[8])
//
// Note: the X360 build folds the leak's separate `m_uLastAllocationSize` idea
// onto the single m_uSizeUsedLastTime slot at 0x20 (AllocateCommand stashes the
// extra-qword count there, then GetPreviousTotalUsedBytes reads it back). The
// PS3-leak `bool m_bIsDuringAllocateMemory` field is NOT present in the X360
// layout (the X360 uses m_pActiveAllocateMemoryBlock != NULL as the in-progress
// flag), so it is intentionally omitted here to keep offsets exact.
// =============================================================================

namespace rw
{
    // Forward decl only — the real RenderWare resource allocator interface is
    // homed under b5-decomp/vendor/renderware/include/rw/rwcore_structs.h.
    // MUST stay `struct` to match that definition's class-key: MSVC bakes the
    // key into the mangling (V vs U), so a `class` forward decl here silently
    // forks every signature that takes rw::IResourceAllocator* (DispatchFrame::
    // Construct / BufferedDispatchFrame::Construct) into an unresolvable twin.
    struct IResourceAllocator;
}

namespace CgsGraphics
{

// A dispatch command is one 16-byte (quad-word) entry in a DispatchBin. The
// pointer arithmetic in DispatchBin (m_pNextWord - m_pBin, >> 4) proves
// sizeof(DispatchCommand) == 16. word0 packs the command type and length:
//   bits  [0 .. 23]  packet length in quad-words   (kSizeBits   = 24)
//   bits [24 .. 30]  command id                    (kCommandBits = 7)
// (CgsDispatcherCommands.h:63 DWARF; the command interpreters in
// CgsDispatcherCommands.cpp read GetCommandID/GetPacketLength off word0.)
struct DispatchCommand
{
    // Command-id values recovered from the asm/asserts (CgsDispatcherCommands.cpp):
    //   CALLBACKFN(0)              -- CallbackFn::Interpret asserts id == 0
    //   DRAWRENDERABLE(1)          -- SetupBuiltinInterpreters slot 1
    //   DRAWRENDERABLEMESH(2)      -- InterpretOcclusionQuery accepts id 2 or 3
    //   DRAWRENDERABLEMESHZONLY(3) -- ZOnly::AddToBin stamps word0 == 0x03000000
    enum E_CommandID
    {
        E_CALLBACKFN              = 0,
        E_DRAWRENDERABLE          = 1,
        E_DRAWRENDERABLEMESH      = 2,
        E_DRAWRENDERABLEMESHZONLY = 3
    };

    static const u32 KU_SIZE_BITS    = 24;   // DWARF kSizeBits
    static const u32 KU_COMMAND_BITS = 7;    // DWARF kCommandBits

    u32 muWords[4];

    // word0 >> 24 & 0x7F  (PPC: lbz word0; clrlwi r,r,25)
    u32 GetCommandID() const { return (muWords[0] >> KU_SIZE_BITS) & 0x7Fu; }
    // word0 & 0x00FFFFFF  (PPC: lwz; clrlwi r,r,8)
    u32 GetPacketLength() const { return muWords[0] & 0x00FFFFFFu; }
};

// A packet is just the leading command of a sequence; aliased in the engine.
typedef DispatchCommand DispatchPacket;

class DispatchFrame;
class DispatchBin;
class DispatchPacketInterpreter;
struct DispatchObjectContext;

// DispatchList — the per-list sort-key/key-block bucket. A list accumulates 64-bit
// sort-key records (one per Submit) into a linked chain of fixed-capacity KeyBlocks;
// the sort/merge pass later walks those records to order the bin's packets.
//
// X360-verified member offsets (word offsets from `this`, 4-byte pointers), attested
// by ReserveKey @ 0x822A0788, Submit @ 0x822A0808, AllocateKeyBlock @ 0x827FA730,
// PrepareSortJobInfo @ 0x827FA7D0, DispatchFrame::Construct @ 0x827F7790 and the
// DispatchAll* walkers (0x827F7660 / 0x827F2718 / 0x827FD7D0):
//   0x00  muWord00           u32               -- zeroed by Construct; not read by any
//                                                recovered function (role unknown, honest pad)
//   0x04  mpDispatchBin      DispatchBin*      -- the frame's embedded bin (assert
//                                                "mpDispatchBin != NULL" @0x827FA730)
//   0x08  m_pBinBase         DispatchCommand*  (a1[2]) -- base for packet-local offset
//   0x0C  muCount            u32               (a1[3]) -- total records submitted
//   0x10  mpBlockListHead    KeyBlock*         (a1[4]) -- first key block (walkers start here)
//   0x14  mpBlockListTail    KeyBlock*         (a1[5]) -- current (last) key block
//   0x18  mpSortedKeys       u64*              (a1[6]) -- flat sorted record array
//                                                (PrepareSortJobInfo fills; walkers read)
//   0x11C word zeroed by Construct/Reset (sort bookkeeping; honest tail span)
// The span past 0x1C out to the X360-verified sizeof(DispatchList) == 384 is NOT
// exercised by any recovered function and stays an honest reserved span.
class DispatchList
{
public:
    // One block in the key-block chain. Submit appends the 64-bit sort record at
    // mpKeys[muCount] and bumps muCount; ReserveKey rolls to a fresh block (via
    // AllocateKeyBlock) once muCount reaches muCapacity. Field offsets attested by
    // the asm: *block (0x00) = key array, +0x04 = count, +0x08 = cap, +0x0C = next
    // (AllocateKeyBlock chain append @0x827FA730; DispatchAllObjectToMesh walk).
    struct KeyBlock
    {
        u64*      mpKeys;      // 0x00 -- packed 64-bit sort records
        u32       muCount;     // 0x04 -- records used
        u32       muCapacity;  // 0x08 -- records this block can hold
        KeyBlock* mpNext;      // 0x0C -- next block in the chain (NULL = tail)
    };

    // AllocateKeyBlock @0x827FA730 sizes each block at 64 records; the block is carved
    // from the list's bin as [header][64 x u64 keys].
    static const u32 KU_KEYBLOCK_CAPACITY = 64u;

    // Packed 64-bit sort record: the sort key in the high bits, the packet's
    // bin-local quad-word offset in the low 20 bits.
    struct SortKey
    {
        static const u32 KU_MASK_OFFSET = 0x000FFFFFu;  // low 20 bits = packet-local qword offset
        static const u32 KU_SHIFT_KEY   = 20u;          // sort key occupies bits [20..]
    };

    // The per-list sort-job parameter block PrepareSortJobInfo fills (the leading
    // words of the 128-byte RadixSort job data @ jobData+20; BrnRendererModule
    // sub_823F5EA0 packages it). Only the three words PrepareSortJobInfo writes are
    // modelled; the job-side repackaging stays with the job system.
    struct SortJobInfo
    {
        KeyBlock* mpBlockListHead;  // +0x00 -- the (rewritten) head block
        u64*      mpaFlatKeys;      // +0x04 -- the flat 128-aligned key array
        u32       muKeyCount;       // +0x08 -- records to sort
    };

    // ---- Functions bodied by this TU (CgsGraphicsDispatchList.cpp) -----------
    DispatchList* ReserveKey();                                       // @ 0x822A0788
    DispatchList* Submit(s32 li32SortKey, DispatchCommand* lpPacket); // @ 0x822A0808
    // @ 0x827FA730 -- append a fresh 64-record KeyBlock (carved from the list's bin)
    // to the chain and make it the tail.
    DispatchList* AllocateKeyBlock();
    // @ 0x827FA7D0 -- flatten every key block into one 128-aligned array carved from
    // the list's bin, collapse the chain onto it, publish it as mpSortedKeys, and
    // fill lpJobInfo for the RadixSort job.
    DispatchList* PrepareSortJobInfo(SortJobInfo* lpJobInfo);
    // [PC leaf] The X360 sorts each list on a RadixSort job (RadixSortEntry
    // @0x82AD2020) between PrepareSortJobInfo and the dispatch walk. The PC
    // bring-up has no job scheduler yet, so this member runs the same
    // prepare-then-sort synchronously (std::stable_sort over the u64 records --
    // radix sort is stable, ascending; same resulting order).
    void SortForDispatch();

    // WorldEntityModule::RenderInstance @0x822D5AB0 reads the submitted-packet count
    // (muCount @0x0C) for its every-128th "send all shader constants" cadence.
    u32 GetCount() const { return muCount; }

    // ---- The dispatch walkers (bodied in CgsDispatcherCommands.cpp, matching the
    // ---- X360 file attribution of their assert strings) ----------------------
    // @ 0x827FD7D0 -- walk the (unsorted) key-block chain and expand every
    // DRAWRENDERABLE object command into per-mesh commands in lpMeshOnlyFrame.
    DispatchCommand* DispatchAllObjectToMesh(DispatchPacketInterpreter* lpInterpreter,
                                             DispatchFrame* lpMeshOnlyFrame,
                                             DispatchObjectContext* lpContext,
                                             s32 liFirst, s32 liCount);
    // @ 0x827F2718 -- walk the sorted records and issue the GPU draws for every
    // DRAWRENDERABLEMESH command. Returns the next record index (-1 when done) so
    // the occlusion-query variant can resume in 64-command bursts.
    s32 DispatchAllMeshes(DispatchPacketInterpreter* lpInterpreter,
                          DispatchObjectContext* lpContext,
                          u32 luFirst, s32 liCount);
    // @ 0x827F7660 -- walk the sorted records and interpret every Z-only command.
    void DispatchAllMeshesZOnly(DispatchPacketInterpreter* lpInterpreter,
                                DispatchObjectContext* lpContext);

    // ---- Declared-only surface (homed by other TUs) -------------------------
    // @ 0x827EE868 -- rebase this list's command/key pointers for main memory.
    DispatchList* RelocateForMainMemory(u32 luBinBase, u32 luBinOffset, u32 luListOffset);
    // @ 0x827E9438 -- re-link the block chain after a job-side flat copy (the
    // multi-threaded object-to-mesh path; not needed by the PC single-threaded path).
    DispatchList* ReconnectChainBlocks();
    // Append another list's records onto this one (the multi-threaded merge path;
    // no X360 export carries a standalone body -- declared for the MT path only).
    DispatchList* Append(DispatchList* lpOther);

private:
    friend class DispatchFrame;   // Construct/Reset initialise the per-list fields (@0x827F7790/@0x827FA8E0)

    // FLAG: 0x00 word zeroed by DispatchFrame::Construct but read by no recovered fn.
    u32              muWord00;             // 0x00
    DispatchBin*     mpDispatchBin;        // 0x04 (a1[1]) -- the owning frame's bin
    DispatchCommand* m_pBinBase;           // 0x08 (a1[2])
    u32              muCount;              // 0x0C (a1[3])
    KeyBlock*        mpBlockListHead;      // 0x10 (a1[4])
    KeyBlock*        mpBlockListTail;      // 0x14 (a1[5])
    u64*             mpSortedKeys;         // 0x18 (a1[6]) -- flat sorted record array
    // FLAG: opaque tail out to the X360-verified sizeof(DispatchList) == 384 (0x180).
    // Sized against the 32-bit X360 ABI; only the +0x11C word inside it is touched
    // (zeroed) by Construct/Reset -- modelled as the leading word of the span. (On the
    // LLP64 x64 gate the byte total legitimately differs — GetList uses the literal
    // 384 stride, never sizeof, so this span is documentary only.)
    u32              muWord11C;            // X360 0x11C (zeroed by Construct/Reset)
    u8               maReservedTail[0x180u - 0x20u - 0x4u];

    // Never called; pins the one pointer-invariant offset fact.
    static void _AssertLayout()
    {
        static_assert(offsetof(DispatchList, m_pBinBase) == 0x08 || sizeof(void*) == 8,
                      "DispatchList::m_pBinBase must sit at 0x08 on the 32-bit target");
    }
};

class DispatchBin
{
public:
    // CgsDispatcher.h:66 / :71 — block sizing constants (PS3 DWARF).
    static const u32 KU_BLOCK_SIZE_IN_QUAD_WORDS = 1024u;
    static const u32 KU_BLOCK_SIZE_IN_BYTES      = 16384u;

    // ---- Functions owned/bodied by this TU ----------------------------------
    DispatchCommand* AllocateCommand(u32 luNumExtraQwords);   // @ 0x827F9168
    void*            AllocateMemoryFast(u32 luQwords);        // @ 0x822A0620
    void*            BeginAllocateMemory(u32 luMaxQwords);    // @ 0x827F9260
    void             BeginPacket();                           // @ 0x822A06C0
    // @ 0x827F7310 -- size the bin (luSizeBytes >> 4 qwords), zero the cursors and
    // shared-memory bookkeeping, then carve the command block from the rw resource
    // allocator (lane 0: align128(size)+128 bytes @ 128 alignment).
    void             Construct(u32 luSizeBytes, rw::IResourceAllocator* lpAllocator);

    // DispatchAllMeshes/DrawRenderable::Interpret headroom check + the walkers'
    // packet decode (m_pBinBase + qword offset) need these reads.
    DispatchCommand* GetBase()          { return m_pBin; }
    u32              GetSizeQwords() const { return m_uSize; }
    u32              GetUsedQwords() const
    {
        return static_cast<u32>(m_pNextWord - m_pBin);
    }

    // WorldEntityModule::GenerateDispatchLists @0x822D5AB0 inline: take the packet
    // chain built since BeginPacket (read + clear m_pPacketStart/m_pLastCommandInPacket,
    // the X360 frame+0x90/+0x94 pokes) for DispatchList::Submit.
    DispatchCommand* EndPacket()
    {
        DispatchCommand* lpPacket = reinterpret_cast<DispatchCommand*>(m_pPacketStart);
        m_pPacketStart = 0;
        m_pLastCommandInPacket = 0;
        return lpPacket;
    }
    void             EndAllocateMemory(u32 luActualQwords);   // @ 0x827E6660

    // Inline accessor preserved verbatim from the Feb-2007 leak (ground truth).
    u32 GetPreviousTotalUsedBytes() const
    {
        return m_uSizeUsedLastTime * static_cast<u32>(sizeof(DispatchCommand));
    }

    // ---- Declared-only surface (homed elsewhere / other TUs) ----------------
    void             HandleMemoryOverflow(u32 luRequestedQuadWords);

private:
    friend class DispatchFrame;   // Reset @0x827FA8E0 rewinds the cursors directly

    void           (*mpMemoryCallback)(void*);          // 0x00
    void*            mpMemoryContext;                   // 0x04
    DispatchCommand* m_pBin;                            // 0x08
    DispatchCommand* m_pNextWord;                       // 0x0C
    DispatchPacket*  m_pPacketStart;                    // 0x10
    DispatchCommand* m_pLastCommandInPacket;            // 0x14
    u32              m_uSize;                            // 0x18 (quad-words)
    void*            m_pActiveAllocateMemoryBlock;      // 0x1C
    u32              m_uSizeUsedLastTime;               // 0x20
    // Trailing bookkeeping attested by Construct @0x827F7310 (zeroed), Reset
    // @0x827FA8E0 (+0x24 <- used-qword snapshot) and ConstructWithSharedBinMemory
    // @0x827EE7C8 (+0x28/+0x2C/+0x30 = the SPU shared-memory block parameters).
    u32              m_uUsedQwordsSnapshot;             // 0x24
    u32              m_uSharedMemoryStart;              // 0x28 (SPU path; dead on PC)
    u32              m_uSharedMemoryBlockMax;           // 0x2C (SPU path; dead on PC)
    u32*             m_pSharedNextFreeAtomic;           // 0x30 (SPU path; dead on PC)
};

// DispatchFrame owns an array of DispatchLists plus an embedded bin. Only the
// two members touched by GetList are modelled with X360-verified offsets:
//   0x000  m_paLists           DispatchList*   (a1[0])
//   0x100  muNumDispatchLists  uint32_t        (a1[64])
// The X360 GetList stride for the list array is 384 bytes (== sizeof every
// DispatchList; PPC: a2*3 << 7). The 0xFC-byte span between m_paLists and
// muNumDispatchLists holds the embedded DispatchBin (m_Bin) plus the
// shared-bin / relocation bookkeeping that the leak header lists but that are
// not exercised by any function in this TU. Rather than fabricate those
// intermediate fields as X360 facts, this gap is a HONEST reserved span sized
// to land muNumDispatchLists at its verified 0x100 offset.
class DispatchFrame
{
public:
    // @ 0x827F7790 — allocate the list array (luNumLists * sizeof(DispatchList))
    // and Construct the embedded bin + per-list state.
    void          Construct(u32 luNumLists, u32 luBinSizeBytes, rw::IResourceAllocator* lpAllocator);
    // @ 0x827FA8E0 — start-of-frame rewind: reset the bin cursors (snapshotting the
    // used-qword count) and clear every list back to one fresh key block.
    void          Reset();
    // @ 0x827E96A8 — free the list array and reset the embedded bin bookkeeping.
    void          Release();
    DispatchList* GetList(u32 luListId);                // @ 0x822A0530

    // The embedded per-frame dispatch bin (m_Bin, @ 0x80). BufferedDispatchFrame::
    // GetDispatchBinForWrite hands this out (X360: frame + 0x80).
    DispatchBin&  GetBin() { return m_Bin; }

    // ---- Shared-memory output path (bodied in CgsDispatcherCommands.cpp) ------
    // These operate on the SPU/shared-memory frame image (the produced bin output
    // block + relocation bookkeeping past 0x100). The members past muNumDispatchLists
    // are part of the larger SPU job-state frame and are reached as that serialised
    // image, so these take `this` as a raw frame and are not re-typed here.
    //
    // @ 0x827EE7C8 -- build the embedded bin + every output list against shared mem.
    static DispatchFrame* ConstructWithSharedBinMemory(
        DispatchFrame* lpResult, DispatchList* lpaDispatchListArray,
        u32 luDispatchListCount, u32 luDispatchBinMasterAddress,
        u32 luSharedMemoryStartAddress, u32* lpSharedMemoryBlockNextFreeAtomic,
        u32 luSharedMemoryBlockMax);
    // @ 0x827EE970 -- relocate every list in the frame for main-memory addressing.
    static DispatchFrame* RelocateForMainMemory(DispatchFrame* lpResult,
        u32 luBinBase, u32 luBinOffset, u32 luListOffset);
    // @ 0x827F72A8 -- flush the produced bin block into its active shared block.
    static DispatchFrame* FlushBlockToSharedMemory(DispatchFrame* lpResult);

private:
    DispatchList* m_paLists;                            // 0x000
    // FLAG: opaque span between m_paLists and the embedded bin (per-list relocation
    // / shared-bin bookkeeping the leak header lists but no TU here reads). Sized to
    // land m_Bin at its X360-verified 0x80 offset.
    u8            maReservedPreBin[0x80u - sizeof(DispatchList*)];
    DispatchBin   m_Bin;                                // 0x080 (embedded bin)
    // FLAG: opaque span between the embedded bin and muNumDispatchLists (shared-bin
    // self-pointer a1[45] + relocation bookkeeping). Sized to land
    // muNumDispatchLists at its X360-verified 0x100 offset.
    u8            maReservedPostBin[0x100u - 0x80u - sizeof(DispatchBin)];
    u32           muNumDispatchLists;                   // 0x100
};

} // namespace CgsGraphics
