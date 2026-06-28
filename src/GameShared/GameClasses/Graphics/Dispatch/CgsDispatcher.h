#pragma once

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
    // homed under b5-decomp/vendor/renderware/include/.
    class IResourceAllocator;
}

namespace CgsGraphics
{

// A dispatch command is one 16-byte (quad-word) entry in a DispatchBin. The
// pointer arithmetic in DispatchBin (m_pNextWord - m_pBin, >> 4) proves
// sizeof(DispatchCommand) == 16. The low 24 bits of the first word hold the
// packet length in quad-words (GetPacketLength == word0 & 0x00FFFFFF); the top
// byte carries command-type/flag bits not modelled here.
struct DispatchCommand
{
    u32 muWords[4];

    // word0 & 0x00FFFFFF  (PPC: lwz; clrlwi r,r,8)
    u32 GetPacketLength() const { return muWords[0] & 0x00FFFFFFu; }
};

// A packet is just the leading command of a sequence; aliased in the engine.
typedef DispatchCommand DispatchPacket;

class DispatchFrame;

// DispatchList is homed by a separate TU (the sort-key/block list). GetList only
// needs the X360-verified stride (sizeof(DispatchList) == 384) and never
// dereferences it, so a forward declaration suffices here.
class DispatchList;

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
    void             EndAllocateMemory(u32 luActualQwords);   // @ 0x827E6660

    // Inline accessor preserved verbatim from the Feb-2007 leak (ground truth).
    u32 GetPreviousTotalUsedBytes() const
    {
        return m_uSizeUsedLastTime * static_cast<u32>(sizeof(DispatchCommand));
    }

    // ---- Declared-only surface (homed elsewhere / other TUs) ----------------
    void             HandleMemoryOverflow(u32 luRequestedQuadWords);

private:
    void           (*mpMemoryCallback)(void*);          // 0x00
    void*            mpMemoryContext;                   // 0x04
    DispatchCommand* m_pBin;                            // 0x08
    DispatchCommand* m_pNextWord;                       // 0x0C
    DispatchPacket*  m_pPacketStart;                    // 0x10
    DispatchCommand* m_pLastCommandInPacket;            // 0x14
    u32              m_uSize;                            // 0x18 (quad-words)
    void*            m_pActiveAllocateMemoryBlock;      // 0x1C
    u32              m_uSizeUsedLastTime;               // 0x20
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
    void          Construct(u32 luNumLists, u32 luBinSizeQwords, rw::IResourceAllocator* lpAllocator);
    // @ 0x827E96A8 — free the list array and reset the embedded bin bookkeeping.
    void          Release();
    DispatchList* GetList(u32 luListId);                // @ 0x822A0530

    // The embedded per-frame dispatch bin (m_Bin, @ 0x80). BufferedDispatchFrame::
    // GetDispatchBinForWrite hands this out (X360: frame + 0x80).
    DispatchBin&  GetBin() { return m_Bin; }

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
