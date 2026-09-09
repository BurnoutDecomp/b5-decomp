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

#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

#include <intrin.h>   // _InterlockedIncrement -- the shared block counter

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
