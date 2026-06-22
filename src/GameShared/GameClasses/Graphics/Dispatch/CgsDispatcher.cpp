#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// =============================================================================
// CgsDispatcher.cpp
//
// Bodies for the CgsGraphics render-dispatch bin/bucket family reconstructed in
// this TU. Store-for-store from the BURNOUT_X360_ARTIST.XEX disassembly:
//
//   CgsGraphics::DispatchBin::AllocateCommand        @ 0x827F9168
//   CgsGraphics::DispatchBin::AllocateMemoryFast     @ 0x822A0620
//   CgsGraphics::DispatchBin::BeginAllocateMemory    @ 0x827F9260
//   CgsGraphics::DispatchBin::BeginPacket            @ 0x822A06C0
//   CgsGraphics::DispatchBin::EndAllocateMemory      @ 0x827E6660
//   CgsGraphics::DispatchFrame::GetList              @ 0x822A0530  (header-TU func)
//
// All bin sizes/offsets are in DispatchCommand quad-word units; pointer
// subtraction `m_pNextWord - m_pBin` already yields a quad-word count because
// sizeof(DispatchCommand) == 16 (the PPC `>> 4` is the compiler's pointer-diff
// scaling, folded into the language-level subtraction here).
// =============================================================================

namespace CgsGraphics
{

// @ 0x827F9168
// Reserve (luNumExtraQwords + 1) quad-words for a command at the current write
// head, returning the command and recording it as the packet's last command.
DispatchCommand* DispatchBin::AllocateCommand(u32 luNumExtraQwords)
{
    CGS_ASSERT(m_pPacketStart != NULL, "m_pPacketStart != NULL");
    CGS_ASSERT(m_pActiveAllocateMemoryBlock == NULL,
               "m_pActiveAllocateMemoryBlock == NULL");

    if (static_cast<u32>(m_pNextWord - m_pBin) + luNumExtraQwords + 1u >= m_uSize)
    {
        HandleMemoryOverflow(luNumExtraQwords);
    }

    CGS_ASSERT(m_pLastCommandInPacket == NULL
                   || m_pLastCommandInPacket->GetPacketLength() == m_uSizeUsedLastTime,
               "m_pLastCommandInPacket->GetPacketLength() == m_uLastAllocationSize");

    DispatchCommand* lpCommand = m_pNextWord;
    m_uSizeUsedLastTime    = luNumExtraQwords;
    m_pLastCommandInPacket = lpCommand;
    m_pNextWord            = lpCommand + (luNumExtraQwords + 1u);
    return lpCommand;
}

// @ 0x822A0620
// Fast path used outside packet building: bump the write head by luQwords
// quad-words and return the old head. Must not be called mid-packet.
void* DispatchBin::AllocateMemoryFast(u32 luQwords)
{
    CGS_ASSERT(m_pPacketStart == NULL, "m_pPacketStart == NULL");

    if (static_cast<u32>(m_pNextWord - m_pBin) + luQwords >= m_uSize)
    {
        HandleMemoryOverflow(luQwords);
    }

    DispatchCommand* lpResult = m_pNextWord;
    m_pNextWord = lpResult + luQwords;
    return lpResult;
}

// @ 0x827F9260
// Begin a variable-size memory allocation: reserve up to luMaxQwords quad-words
// and mark the active block at the current write head.
void* DispatchBin::BeginAllocateMemory(u32 luMaxQwords)
{
    if (static_cast<u32>(m_pNextWord - m_pBin) + luMaxQwords >= m_uSize)
    {
        HandleMemoryOverflow(luMaxQwords);
    }

    CGS_ASSERT(m_pActiveAllocateMemoryBlock == NULL,
               "m_pActiveAllocateMemoryBlock == NULL");
    CGS_ASSERT(reinterpret_cast<void*>(m_pPacketStart) == reinterpret_cast<void*>(m_pNextWord),
               "m_pPacketStart == m_pNextWord");

    m_pActiveAllocateMemoryBlock = m_pNextWord;
    return m_pActiveAllocateMemoryBlock;
}

// @ 0x822A06C0
// Open a packet: stamp the packet start at the current write head.
void DispatchBin::BeginPacket()
{
    CGS_ASSERT(m_pNextWord != NULL, "m_pNextWord != NULL");
    CGS_ASSERT(m_pPacketStart == NULL, "m_pPacketStart == NULL");
    CGS_ASSERT(static_cast<u32>(m_pNextWord - m_pBin) < m_uSize,
               "uint32_t( m_pNextWord - m_pBin ) < m_uSize");

    m_pPacketStart = m_pNextWord;
}

// @ 0x827E6660
// Close a variable-size allocation: commit luActualQwords quad-words by
// advancing both the packet start and the write head past the block.
void DispatchBin::EndAllocateMemory(u32 luActualQwords)
{
    CGS_ASSERT(m_pActiveAllocateMemoryBlock != NULL,
               "m_pActiveAllocateMemoryBlock != NULL");
    CGS_ASSERT(reinterpret_cast<void*>(m_pPacketStart) == reinterpret_cast<void*>(m_pNextWord),
               "m_pPacketStart == m_pNextWord");
    CGS_ASSERT(reinterpret_cast<void*>(m_pPacketStart) == m_pActiveAllocateMemoryBlock,
               "m_pPacketStart == m_pActiveAllocateMemoryBlock");

    DispatchPacket* lpEnd = m_pPacketStart + luActualQwords;
    m_pActiveAllocateMemoryBlock = NULL;
    m_pPacketStart = lpEnd;
    m_pNextWord    = lpEnd;
}

// @ 0x822A0530  (header-TU function — DispatchFrame::GetList)
// Return the luListId'th DispatchList in m_paLists. Stride is sizeof(DispatchList)
// == 384 bytes; computed as a byte offset because DispatchList is forward-declared
// here (homed by a separate TU) and never dereferenced.
DispatchList* DispatchFrame::GetList(u32 luListId)
{
    CGS_ASSERT(m_paLists != NULL, "m_paLists != NULL");
    // Original streams "Invalid List ID = <id>" into the assert buffer; the
    // bounds condition is preserved here, the message rendered as a plain string.
    CGS_ASSERT(luListId < muNumDispatchLists, "Invalid List ID");

    const u32 KU_DISPATCH_LIST_STRIDE_BYTES = 384u;
    u8* lpBase = reinterpret_cast<u8*>(m_paLists);
    return reinterpret_cast<DispatchList*>(lpBase + luListId * KU_DISPATCH_LIST_STRIDE_BYTES);
}

} // namespace CgsGraphics
