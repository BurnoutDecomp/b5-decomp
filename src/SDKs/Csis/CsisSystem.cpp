// X360-faithful Csis::System -- the process-global Csis registry lock + content
// subscription manager linked into BURNOUT_X360_ARTIST.XEX. Source of truth:
// per-addr exports under .ida-exports/BURNOUT_X360_ARTIST.XEX/ (X360 asm is
// authoritative; there is no leak source / DWARF for this TU). See CsisSystem.h
// for the layout and the process-global module-data symbols.

#include <cstdint>    // std::uintptr_t (content-base relocation add)
#include <Windows.h>  // CreateMutexA / WaitForSingleObject / ReleaseMutex / HANDLE

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
    for (u32 luLane = 1; luLane < 4; ++luLane)
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

// ---------------------------------------------------------------------------
// System::Subscribe @ 0x82B0F2A8
//
// Resolve the three inline client-array base pointers from the content header:
//   mpArray1 (+0x14) = content + 0x28
//   mpArray2 (+0x18) = mpArray1 + 12 * muNumUpdate
//   mpArray3 (+0x1C) = mpArray2 + 12 * muNumDestroy   (byte stride 12; array3
//                                                       records are 16 bytes each)
// Then, for each of the three arrays, walk every record:
//   * relocate the record's content-relative target to an absolute pointer by
//     adding the content base address (arrays 1/2: +0x04; array 3: +0x08)
//   * stamp a fresh rolling serial (arrays 1/2: +0x0A; array 3: +0x0E)
// Finally push the content object onto the head of the global subscribed-content
// doubly-linked list (node at +0x20 next / +0x24 prev). Returns 0.
//
// (The asm carries the serial in r11 across all three loops and writes it back to
// word_8324E908 after each loop; reproduced here with gwCsisSerial.)
// ---------------------------------------------------------------------------
int System::Subscribe(SystemContent* pContent)
{
    const uintptr_t uContentBase = reinterpret_cast<uintptr_t>(pContent);

    SystemClient24* pArray1 =
        reinterpret_cast<SystemClient24*>(pContent->mauInlineArrays);
    pContent->mpArray1 = pArray1;

    SystemClient24* pArray2 = pArray1 + pContent->muNumUpdate;
    pContent->mpArray2 = pArray2;

    SystemClient32* pArray3 = reinterpret_cast<SystemClient32*>(
        pArray2 + pContent->muNumDestroy);
    pContent->mpArray3 = pArray3;

    short wSerial = gwCsisSerial;  // lhz word_8324E908

    // ---- array 1 (update clients, 12-byte records) --------------------------
    if (pContent->muNumUpdate > 0)
    {
        for (u16 i = 0; i < pContent->muNumUpdate; ++i)
        {
            SystemClient24* pNode = pContent->mpArray1 + i;
            pNode->muTargetOrOffset += uContentBase;
            wSerial = NextSerial(wSerial);
            pNode->mState.mLive.miSerial = wSerial;
        }
        gwCsisSerial = wSerial;  // sth r11, word_8324E908
    }

    // ---- array 2 (destroy clients, 12-byte records) -------------------------
    if (pContent->muNumDestroy > 0)
    {
        for (u16 i = 0; i < pContent->muNumDestroy; ++i)
        {
            SystemClient24* pNode = pContent->mpArray2 + i;
            pNode->muTargetOrOffset += uContentBase;
            wSerial = NextSerial(wSerial);
            pNode->mState.mLive.miSerial = wSerial;
        }
        gwCsisSerial = wSerial;
    }

    // ---- array 3 (16-byte records) ------------------------------------------
    if (pContent->muNumThird > 0)
    {
        for (u16 i = 0; i < pContent->muNumThird; ++i)
        {
            SystemClient32* pNode = pContent->mpArray3 + i;
            pNode->muTargetOrOffset += uContentBase;
            wSerial = NextSerial(wSerial);
            pNode->mState.mLive.miSerial = wSerial;
        }
        gwCsisSerial = wSerial;
    }

    // ---- push onto the subscribed-content list ------------------------------
    //   node = content + 0x20
    //   node->next = old head node (stw r9, 0(node))
    //   node->prev = 0            (stw 0, 4(node))
    //   if (old head) old_head_node->prev = node  (stw node, 4(r9))
    //   off_8324E90C = node       (stw node, off_8324E90C)
    //
    // The list links point at OTHER nodes (each content's own +0x20), so this is a
    // node-space push; reproduced by name on mListNode.
    SystemContentNode* pNode = &pContent->mListNode;  // content + 0x20
    pNode->mpNext = gpCsisListHead;                   // node->next = old head
    pNode->mpPrev = 0;                                // node->prev = null
    if (gpCsisListHead)
        gpCsisListHead->mpPrev = pNode;               // old_head->prev = node
    gpCsisListHead = pNode;                           // head = node

    return 0;
}

// ---------------------------------------------------------------------------
// System::Unsubscribe @ 0x82B0F428
//
// For each of the three inline client arrays, write -1 into every record's status
// word (arrays 1/2: +0x08; array 3: +0x0C). Then unlink the content node from the
// global subscribed-content list:
//   node = content + 0x20
//   if (node == off_8324E90C) off_8324E90C = node->next   -> pop head
//   if (node->prev)  node->prev->next = node->next         -> stitch forward
//   if (node->next)  node->next->prev = node->prev         -> stitch backward
// Returns 0.
// ---------------------------------------------------------------------------
int System::Unsubscribe(SystemContent* pContent)
{
    // ---- array 1: status = -1 ----------------------------------------------
    if (pContent->muNumUpdate > 0)
    {
        for (u16 i = 0; i < pContent->muNumUpdate; ++i)
            (pContent->mpArray1 + i)->mState.miStatus = -1;
    }

    // ---- array 2: status = -1 ----------------------------------------------
    if (pContent->muNumDestroy > 0)
    {
        for (u16 i = 0; i < pContent->muNumDestroy; ++i)
            (pContent->mpArray2 + i)->mState.miStatus = -1;
    }

    // ---- array 3: status = -1 ----------------------------------------------
    if (pContent->muNumThird > 0)
    {
        for (u16 i = 0; i < pContent->muNumThird; ++i)
            (pContent->mpArray3 + i)->mState.miStatus = -1;
    }

    // ---- unlink from the subscribed-content list ----------------------------
    //   node = content + 0x20
    //   if (node == head)  head = node->next            -> pop head
    //   if (node->prev)    node->prev->next = node->next -> stitch forward
    //   if (node->next)    node->next->prev = node->prev -> stitch backward
    SystemContentNode* pNode = &pContent->mListNode;  // content + 0x20

    if (pNode == gpCsisListHead)
        gpCsisListHead = pNode->mpNext;               // pop head

    if (pNode->mpPrev)
        pNode->mpPrev->mpNext = pNode->mpNext;        // prev->next = next

    if (pNode->mpNext)
        pNode->mpNext->mpPrev = pNode->mpPrev;        // next->prev = prev

    return 0;
}

int System::ResolveInterface(u32 auKind, const InterfaceId* apId,
                             void** appDescriptor, s32* apiToken)
{
    if (!apId || !appDescriptor || !apiToken)
        return -3;

    for (SystemContentNode* lpNode = gpCsisListHead; lpNode; lpNode = lpNode->mpNext)
    {
        SystemContent* lpContent = reinterpret_cast<SystemContent*>(
            reinterpret_cast<u8*>(lpNode) - offsetof(SystemContent, mListNode));
        if (lpContent->muSystemId != apId->muSystemId)
            continue;

        if (auKind == 0 || auKind == 1)
        {
            SystemClient24* lpEntries = auKind == 0
                ? lpContent->mpArray2 : lpContent->mpArray1;
            const u16 luCount = auKind == 0
                ? lpContent->muNumDestroy : lpContent->muNumUpdate;
            for (u16 luIndex = 0; luIndex < luCount; ++luIndex)
            {
                SystemClient24& lrEntry = lpEntries[luIndex];
                const char* lpcName = reinterpret_cast<const char*>(lrEntry.muTargetOrOffset);
                if (lrEntry.mState.mLive.muInterfaceId == apId->muInterfaceId &&
                    lpcName && apId->mpName && std::strcmp(lpcName, apId->mpName) == 0)
                {
                    *appDescriptor = &lrEntry;
                    *apiToken = lrEntry.mState.miStatus;
                    return 0;
                }
            }
        }
        else if (auKind == 2)
        {
            for (u16 luIndex = 0; luIndex < lpContent->muNumThird; ++luIndex)
            {
                SystemClient32& lrEntry = lpContent->mpArray3[luIndex];
                const char* lpcName = reinterpret_cast<const char*>(lrEntry.muTargetOrOffset);
                if (lrEntry.mState.mLive.muInterfaceId == apId->muInterfaceId &&
                    lpcName && apId->mpName && std::strcmp(lpcName, apId->mpName) == 0)
                {
                    *appDescriptor = &lrEntry;
                    *apiToken = lrEntry.mState.miStatus;
                    return 0;
                }
            }
        }
    }

    *appDescriptor = 0;
    *apiToken = -6;
    return -6;
}

} // namespace Csis
