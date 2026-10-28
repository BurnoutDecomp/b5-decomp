// X360-faithful Csis::System -- the process-global Csis registry lock + content
// subscription manager linked into BURNOUT_X360_ARTIST.XEX. Source of truth:
// per-addr exports under .ida-exports/BURNOUT_X360_ARTIST.XEX/ (X360 asm is
// authoritative; there is no leak source / DWARF for this TU). See CsisSystem.h
// for the layout and the process-global module-data symbols.

#include <cstdint>    // std::uintptr_t (content-base relocation add)
#include <xtl.h>  // CreateMutexA / WaitForSingleObject / ReleaseMutex / HANDLE

#include "SDKs/Csis/CsisSystem.h"
#include "SDKs/Csis/CsisClassHandle.h"
#include "rw/rwcore_structs.h"

#include <cstring>

namespace
{
    // X360 process-global module data driving Csis::System (see CsisSystem.h):
    //   dword_8324E8FC -- the global registry mutex HANDLE.
    //   byte_8324E901  -- the inited flag.
    //   off_8324E90C   -- head of the subscribed-content list (a node pointer).
    //   word_8324E908  -- the rolling non-negative 16-bit serial counter.
    HANDLE                     ghCsisMutex    = 0;  // dword_8324E8FC
    unsigned char              gbCsisInited   = 0;  // byte_8324E901
    Csis::SystemContentNode*   gpCsisListHead = 0;  // off_8324E90C
    short                      gwCsisSerial   = 0;  // word_8324E908

    // off_8324E904 and byte_8324E900: the Csis-wide allocator and its installed
    // latch. ARTIST CreateInstanceFast allocates through this exact global.
    rw::IResourceAllocator*    gpCsisAllocator = 0;
    unsigned char              gbCsisAllocatorSet = 0;
}

namespace Csis
{

// ---------------------------------------------------------------------------
// System::SetAllocator @ 0x82B0F1C0.
// if (byte_8324E901) return -7; off_8324E904=allocator;
// byte_8324E900=1; return 0.
// ---------------------------------------------------------------------------
int System::SetAllocator(rw::IResourceAllocator* apAllocator)
{
    if (gbCsisInited)
        return -7;
    gpCsisAllocator = apAllocator;
    gbCsisAllocatorSet = 1;
    return 0;
}

void* System::Allocate(size_t auSize, const char* apcName, u32 auAlignment)
{
    if (!gbCsisAllocatorSet || !gpCsisAllocator)
        return 0;

    rw::ResourceDescriptor lDescriptor = {};
    lDescriptor.m_baseResourceDescriptors[0].m_size = static_cast<u32>(auSize);
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = auAlignment;
    for (u32 luLane = 1; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
        lDescriptor.m_baseResourceDescriptors[luLane].m_alignment = 1;
    return gpCsisAllocator->DoAllocate(lDescriptor, apcName).m_baseResources[0];
}

void System::Free(void* apBlock)
{
    if (!apBlock || !gpCsisAllocator)
        return;
    rw::Resource lResource = {};
    lResource.m_baseResources[0] = apBlock;
    gpCsisAllocator->DoFree(lResource);
}

// ---------------------------------------------------------------------------
// System::Init @ 0x82B0F1F8
//
//   r3 = CreateMutexA(0, 0, 0) ; stw r3, dword_8324E8FC
//   stw 0,  off_8324E90C   -> subscribed-content list head = null
//   stb 1,  byte_8324E901  -> inited = 1
//   li  r3, 0 ; blr        -> return 0
// ---------------------------------------------------------------------------
int System::Init()
{
    ghCsisMutex    = ::CreateMutexA(0, FALSE, 0);
    gpCsisListHead = 0;
    gbCsisInited   = 1;
    return 0;
}

// ---------------------------------------------------------------------------
// System::IsInited @ 0x82B10040
//
//   lbz r3, byte_8324E901 ; blr
// ---------------------------------------------------------------------------
int System::IsInited()
{
    return gbCsisInited;
}

// ---------------------------------------------------------------------------
// System::Lock @ 0x82B0F248
//
//   WaitForSingleObject(dword_8324E8FC, 0xFFFFFFFF) ; li r3, 0 ; blr
// ---------------------------------------------------------------------------
int System::Lock()
{
    ::WaitForSingleObject(ghCsisMutex, 0xFFFFFFFFu);  // INFINITE
    return 0;
}

// ---------------------------------------------------------------------------
// System::Unlock @ 0x82B0F278
//
//   ReleaseMutex(dword_8324E8FC) ; li r3, 0 ; blr
// ---------------------------------------------------------------------------
int System::Unlock()
{
    ::ReleaseMutex(ghCsisMutex);
    return 0;
}

namespace
{
    // The rolling serial Subscribe stamps onto each client: ++serial, but if the
    // 16-bit sign-extended value goes negative it resets to 1 (and latches 1 into
    // word_8324E908). Mirrors the asm's `addi r11,r11,1 ; extsh. r11 ; bge + ; li
    // r11,1 ; sth r11,word_8324E908`.
    inline short NextSerial(short wSerial)
    {
        short wNext = static_cast<short>(wSerial + 1);  // addi +1 ; extsh.
        if (wNext < 0)
        {
            wNext        = 1;   // li r11, 1
            gwCsisSerial = 1;   // sth r11, word_8324E908
        }
        return wNext;
    }
}




} // namespace Csis
