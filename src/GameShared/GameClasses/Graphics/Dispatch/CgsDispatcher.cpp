#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"   // rw::Resource / rw::ResourceDescriptor / rw::IResourceAllocator
#include <new>                   // ::operator new (the PC RW-heap thunk target)

// includes folded in from the CgsDispatchBin_w*.cpp partfiles (2026-09-15)
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include <intrin.h>   // _InterlockedIncrement -- the shared block counter

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

    // X360 strides the literal 384 == the 32-bit sizeof(DispatchList). On the x64
    // gate the named-member layout is wider, so the stride is the host sizeof
    // (semantic parity: "the luListId'th list of the array Construct allocated").
    const u32 KU_DISPATCH_LIST_STRIDE_BYTES =
        (sizeof(void*) == 4) ? 384u : static_cast<u32>(sizeof(DispatchList));
    u8* lpBase = reinterpret_cast<u8*>(m_paLists);
    return reinterpret_cast<DispatchList*>(lpBase + luListId * KU_DISPATCH_LIST_STRIDE_BYTES);
}

// @ 0x827F7310
// Construct the bin: record the size in quad-words, zero the cursors + packet
// state + shared-memory bookkeeping, then carve the command block from the rw
// resource allocator. The X360 descriptor asks lane 0 for align128(size)+128
// bytes at 128-byte alignment and takes the lane-0 pointer as the block.
void DispatchBin::Construct(u32 luSizeBytes, rw::IResourceAllocator* lpAllocator)
{
    m_uSize                = luSizeBytes >> 4;
    m_pBin                 = 0;
    m_pNextWord            = 0;
    mpMemoryCallback       = 0;      // X360 *a1 = 0 (leading callback/context words)
    mpMemoryContext        = 0;
    m_pPacketStart         = 0;
    m_pLastCommandInPacket = 0;
    m_pActiveAllocateMemoryBlock = 0;
    m_uSizeUsedLastTime    = 0;
    m_uUsedQwordsSnapshot  = 0;
    m_uSharedMemoryStart   = 0;
    m_uSharedMemoryBlockMax = 0;
    m_pSharedNextFreeAtomic = 0;

    rw::ResourceDescriptor lDescriptor;
    lDescriptor.m_baseResourceDescriptors[0].m_size      = ((luSizeBytes + 127u) & ~127u) + 128u;
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = 128u;
    for (u32 luLane = 1; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
    {
        lDescriptor.m_baseResourceDescriptors[luLane].m_size      = 0u;
        lDescriptor.m_baseResourceDescriptors[luLane].m_alignment = 1u;
    }

    rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, 0);
    m_pBin      = reinterpret_cast<DispatchCommand*>(lResource.m_baseResources[0]);
    m_pNextWord = m_pBin;
}

// @ 0x827F7790
// Allocate the per-frame list array and initialise the embedded bin + every list.
void DispatchFrame::Construct(u32 luNumLists, u32 luBinSizeBytes, rw::IResourceAllocator* lpAllocator)
{
    muNumDispatchLists = luNumLists;

    // X360 allocates 384 * n through the engine allocator (sub_82C08C00, the RW
    // heap thunk) with an overflow clamp; the x64 gate sizes by the host sizeof
    // and uses operator new (the same global heap the PC RW thunks resolve to).
    const u32 KU_STRIDE = (sizeof(void*) == 4) ? 384u : static_cast<u32>(sizeof(DispatchList));
    m_paLists = reinterpret_cast<DispatchList*>(::operator new(static_cast<size_t>(KU_STRIDE) * luNumLists));
    CGS_ASSERT(m_paLists != NULL, "m_paLists != NULL");

    m_Bin.Construct(luBinSizeBytes, lpAllocator);

    for (u32 luListId = 0; luListId < muNumDispatchLists; ++luListId)
    {
        DispatchList* lpList  = GetList(luListId);
        lpList->muCount         = 0;
        lpList->mpDispatchBin   = &m_Bin;
        lpList->muWord11C       = 0;
        lpList->muWord00        = 0;
        lpList->mpBlockListHead = 0;
        lpList->mpBlockListTail = 0;
        lpList->mpSortedKeys    = 0;
        lpList->m_pBinBase      = m_Bin.GetBase();
    }
}

// @ 0x827FA8E0
// Start-of-frame rewind: snapshot the used-qword count, rewind the bin write
// head, then clear every list back to one fresh key block.
void DispatchFrame::Reset()
{
    CGS_ASSERT(m_paLists != NULL, "m_paLists != NULL");

    m_Bin.m_uUsedQwordsSnapshot = static_cast<u32>(m_Bin.m_pNextWord - m_Bin.m_pBin);
    m_Bin.m_pNextWord           = m_Bin.m_pBin;

    for (u32 luListId = 0; luListId < muNumDispatchLists; ++luListId)
    {
        DispatchList* lpList  = GetList(luListId);
        lpList->mpBlockListHead = 0;
        lpList->mpBlockListTail = 0;
        lpList->muCount         = 0;
        lpList->muWord11C       = 0;
        lpList->mpSortedKeys    = 0;
        lpList->AllocateKeyBlock();
    }
}

// @ 0x827E96A8
// End-of-life teardown: free the list array (operator delete[], the mirror of
// Construct's operator new[]) and rewind the embedded bin's bookkeeping. The
// X360 body inlines DispatchBin::Release here -- its assert string is
// "m_pPacketStart == NULL" attributed to CgsDispatcher.cpp:86 (the bin's own
// line) while the leading "m_paLists != NULL" assert is line 727 (the frame's) --
// hence the two asserts inside one function. Store map (frame words):
//   a1[0]  m_paLists                 -> operator delete__
//   a1[36] (0x90) m_Bin.m_pPacketStart  -> asserted NULL
//   a1[34] (0x88) m_Bin.m_pBin          -> cleared when non-NULL
//   a1[35] (0x8C) m_Bin.m_pNextWord     -> cleared
//   a1[64] (0x100) muNumDispatchLists   -> cleared
void DispatchFrame::Release()
{
    CGS_ASSERT(m_paLists != NULL, "m_paLists != NULL");

    // (X360 `operator delete__`; this TU's Construct carves the array with the
    // raw ::operator new stride helper, so the matching raw form is used here.)
    ::operator delete(m_paLists);
    m_paLists = 0;

    CGS_ASSERT(m_Bin.m_pPacketStart == NULL, "m_pPacketStart == NULL");

    if (m_Bin.m_pBin != NULL)
    {
        m_Bin.m_pBin = 0;
    }
    m_Bin.m_pNextWord  = 0;
    muNumDispatchLists = 0;
}

} // namespace CgsGraphics

// ============================================================================
// FOLDED FROM CgsDispatchBin_wG_Overflow.cpp (wave G) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// =============================================================================
// CgsDispatchBin_wG_Overflow.cpp  (GameShared/GameClasses/Graphics/Dispatch)
//
// CgsGraphics::DispatchBin::HandleMemoryOverflow -- the bin's out-of-room handler.
//
// Every caller checks `used + request >= size`, calls this, and then CARRIES ON
// WRITING at m_pNextWord, so the handler has exactly two outcomes: rebind the bin
// onto a fresh 16KB shared-memory block (the SPU job image; the bin is REBASED, it
// never grows), or a fatal formatted assert. No allocator fallback.
//
// On this build the rebind leg is unreachable: Construct zeroes the three
// shared-memory fields and only ConstructWithSharedBinMemory (SPU-side, dead on PC)
// ever sets them, so this function is its assert.
//
// FLAG: a shared-memory DispatchFrame is a packed 32-bit guest image, not a host
// object. The bin's back-pointer to its frame (bin+0x34 == frame+0xB4) and the
// frame's active-block address (frame+0x104) have no named members, so leg 1 reaches
// them through the guest word image -- the same exception CgsDispatcherCommands.cpp
// declares for this family. Every such read sits behind the m_uSharedMemoryStart
// guard and cannot execute on the host.
// =============================================================================


namespace CgsGraphics
{

namespace
{
    // The console claims a block with an interrupt-masked load-linked /
    // store-conditional increment and uses the NEW counter value; the claimed
    // block index is that value minus one. _InterlockedIncrement has the same
    // increment-and-return-new contract.
    inline u32 ClaimNextSharedBlock(u32* lpCounter)
    {
        return static_cast<u32>(
            _InterlockedIncrement(reinterpret_cast<long volatile*>(lpCounter)));
    }

    // The guest word image of a shared-memory DispatchFrame (see the banner).
    inline u32* SharedFrameWords(void* lpFrame)
    {
        return reinterpret_cast<u32*>(lpFrame);
    }

    const u32 KU_SHARED_BLOCK_SHIFT = 14u;   // 16384 bytes per shared block
    const u32 KU_FRAME_ACTIVE_BLOCK_WORD = 0x104u / 4u;
}

void DispatchBin::HandleMemoryOverflow(u32 luRequestedQuadWords)
{
    bool lbHandled              = false;
    bool lbSharedMemoryExhausted = false;

    if (m_uSharedMemoryStart != 0)
    {
        CGS_ASSERT(m_pSharedNextFreeAtomic != NULL,
                   "mpSharedBinBlockNextFree_Atomic != NULL");

        // Claim the next block; the index is (new counter value - 1).
        const u32 luBlockIndex = ClaimNextSharedBlock(m_pSharedNextFreeAtomic) - 1u;

        if (luBlockIndex >= m_uSharedMemoryBlockMax)
        {
            lbSharedMemoryExhausted = true;
        }
        else
        {
            // The bin's owning shared-memory frame (bin+0x34 == frame+0xB4).
            void* lpFrameImage = reinterpret_cast<void*>(
                static_cast<uintptr_t>(SharedFrameWords(this)[0x34u / 4u]));
            DispatchFrame* lpFrame = reinterpret_cast<DispatchFrame*>(lpFrameImage);

            // Push the block the bin has been filling out to shared memory before
            // rebinding onto the new one.
            DispatchFrame::FlushBlockToSharedMemory(lpFrame);

            const u32 luBlockAddress =
                (luBlockIndex << KU_SHARED_BLOCK_SHIFT) + m_uSharedMemoryStart;
            DispatchCommand* lpBlock = reinterpret_cast<DispatchCommand*>(
                static_cast<uintptr_t>(luBlockAddress));

            // The rewind happens UNCONDITIONALLY here -- the console stores all
            // eight words BEFORE the request-size test below, so an over-large
            // request still leaves the bin rebound on the fresh block (and then
            // asserts).
            m_pPacketStart               = 0;
            m_pActiveAllocateMemoryBlock = 0;
            m_pLastCommandInPacket       = 0;
            m_uSizeUsedLastTime          = 0;
            m_uUsedQwordsSnapshot        = 0;
            m_pBin                       = lpBlock;
            m_uSize                      = KU_BLOCK_SIZE_IN_QUAD_WORDS;   // 1024
            m_pNextWord                  = lpBlock;

            // One block is the ceiling: a request bigger than a whole block can
            // never be satisfied by rebasing, so it falls through to the assert.
            if (luRequestedQuadWords <= KU_BLOCK_SIZE_IN_QUAD_WORDS)
            {
                SharedFrameWords(lpFrame)[KU_FRAME_ACTIVE_BLOCK_WORD] = luBlockAddress;

                // Notify the owner that the bin moved (the SPU-side relocation hook).
                // FLAG: the console dispatches this callback UNCONDITIONALLY. The
                // null test is the one deviation in this body, and it is on a leg
                // that cannot execute here (see the banner); Construct zeroes
                // mpMemoryCallback, so an unguarded call would be a null call the
                // moment anybody wires the shared path up on the host.
                if (mpMemoryCallback != 0)
                {
                    mpMemoryCallback(mpMemoryContext);
                }
                lbHandled = true;
            }
        }
    }

    if (!lbHandled)
    {
        // The console builds ONE message in the shared assert buffer through a
        // stack StrStream and fires it in CgsDispatcher.cpp. Built on the
        // stack here, the same shape CgsBox.h / CgsID.cpp already use (the +1 is
        // the documented one-past-the-end guard byte StrStream::Append needs).
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE + 1];
        CgsDev::StrStream lStrStream(lacMessageBuffer,
                                     CgsDev::Assert::KI_MESSAGEBUFFERSIZE);

        lStrStream << "CgsGraphics::DispatchBin::HandleMemoryOverflow() failed";
        lStrStream << (lbSharedMemoryExhausted ? "; shared memory exhausted.\n" : ".\n");
        lStrStream << "this=";
        lStrStream << static_cast<void*>(this);
        lStrStream << " bin=";
        lStrStream << static_cast<void*>(m_pBin);
        lStrStream << " capacity=";
        lStrStream << m_uSize;
        lStrStream << " request=";
        lStrStream << luRequestedQuadWords;
        lStrStream << " available=";
        lStrStream << (m_uSize - GetUsedQwords());

        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lacMessageBuffer, __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }
}

} // namespace CgsGraphics
