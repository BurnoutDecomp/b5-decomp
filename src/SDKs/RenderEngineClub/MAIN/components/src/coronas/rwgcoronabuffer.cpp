#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronabuffer.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   renderengine::CoronaBuffer::GetResourceDescriptor @ 0x8228D6C0
//   renderengine::CoronaBuffer::Initialize            @ 0x822850D8
//
// (2026-08-17, coronas step 1: the two bodies are unchanged; only the descriptor type moved with the
// class into GameSource/Graphics/BrnCoronaManager.h -- see this file's header shim. It is now spelled
// CoronaBuffer::ResourceDescriptor5, a NESTED type, because the flat `renderengine::
// ResourceDescriptor5` it used to be was a third spelling of rw::BaseResourceDescriptors<5>.)
//
// The three corrections carried in from carlights step 1, all from the ASM (which overrides the
// Hex-Rays pseudocode -- AGENTS.md "Verify calling conventions against the ASSEMBLY"):
//
// 1. THE DECLARATIONS DID NOT MATCH THE HEADER. This file used to declare its own local
//    `class CoronaBuffer` with NON-STATIC `void* GetResourceDescriptor(void*, const u32*)` and
//    `void* Initialize(void**, const u32*)`, while the committed header declared them STATIC with
//    typed parameters -- different mangled names, i.e. LNK2019 x2 the moment
//    BrnCoronaManager::Construct linked. Measured at the time with dumpbin /SYMBOLS:
//      ?GetResourceDescriptor@CoronaBuffer@renderengine@@QEAAPEAXPEAXPEBI@Z   (Q = non-static)
//      ?Initialize@CoronaBuffer@renderengine@@QEAAPEAXPEAPEAXPEBI@Z
//    It now includes the header and defines exactly what the header declares.
//
// 2. GetResourceDescriptor SIZES ONLY SLOT 0. The pseudocode reads
//      result[2] = v2; result[4] = v2; result[6] = v2; result[8] = v2;
//    -- i.e. all five slots sized at the stride -- but the asm stores r10, and `li r10, 0` two
//    instructions earlier:
//      0x8228D6D0  li   r10, 0
//      0x8228D6F8  stw  r10, 0(r3)     0x8228D6FC  stw r10, 8(r3)     0x8228D700  stw r10, 0x10(r3)
//      0x8228D704  stw  r10, 0x18(r3)  0x8228D708  stw r10, 0x20(r3)
//    and only THEN does the 64-bit `std r11, 0(r3)` (assembled at 0x8228D6EC-F4 from
//    {stride, 16}) put the stride + alignment 16 into slot 0. So the table is
//      { {stride, 16}, {0,1}, {0,1}, {0,1}, {0,1} }
//    and the old form asked the allocator for FIVE blocks of 32784 bytes instead of one.
//
// 3. THE SECOND Initialize ARGUMENT IS THE PARAMETERS, NOT A DESCRIPTOR. Its caller
//    BrnCoronaManager::Construct @0x823FCEB8/0x823FCEC8 passes the same params block it handed
//    GetResourceDescriptor, whose first word is the corona count (512, `li r11, 0x200` @0x823FCE1C)
//    -- so `*result = *a2` is `muNumCoronas = params.miNumCoronas`, not a descriptor copy.

namespace renderengine
{
    // X360 0x8228D6C0. The buffer is one block: a header followed by `count` 64-byte records.
    CoronaBuffer::ResourceDescriptor5* CoronaBuffer::GetResourceDescriptor(
        ResourceDescriptor5* pDescriptor, Parameters* pParameters)
    {
        // `slwi r9,r9,6 ; addi r9,r9,0x10` -- 64 bytes per Corona plus the 16-byte header lane.
        const u32 luBlockSize = (static_cast<u32>(pParameters->miNumCoronas) << 6) + 16u;

        for (int liSlot = 0; liSlot < 5; ++liSlot)
        {
            pDescriptor->maEntries[liSlot].muSize      = 0u;
            pDescriptor->maEntries[liSlot].muAlignment = 1u;
        }

        pDescriptor->maEntries[0].muSize      = luBlockSize;
        pDescriptor->maEntries[0].muAlignment = 16u;
        return pDescriptor;
    }

    // X360 0x822850D8. Lay the header down in the carved block and point mpData at the first
    // 16-byte-aligned address past it.
    CoronaBuffer* CoronaBuffer::Initialize(CoronaBuffer** ppBuffer, Parameters* pParameters)
    {
        CoronaBuffer* lpBuffer = *ppBuffer;
        if (lpBuffer == 0)
            return 0;   // [PC guard] a starved allocator lane -- console-impossible (its heap is
                        // carved up front), a live failure mode of the PC LinearResourceAllocator

        lpBuffer->muNumCoronas = static_cast<u32>(pParameters->miNumCoronas);

        // `addi r10,r3,0x17 ; clrrwi r11,r10,4` == (base + 8 + 15) & ~15, i.e. "align 16 past an
        // 8-byte console header". sizeof(CoronaBuffer) is 8 on the console and 16 here (a u32 and
        // a 64-bit pointer), so the host form has to round past the HOST header, not the guest
        // one -- and because the block is 16-aligned by the descriptor above, both land on
        // base+16 and the GetResourceDescriptor size (16 + count*64) still fits exactly.
        const usize luBase = reinterpret_cast<usize>(lpBuffer);
        lpBuffer->mpData = reinterpret_cast<Corona*>((luBase + sizeof(CoronaBuffer) + 15u) & ~static_cast<usize>(15u));

        return lpBuffer;
    }
}
