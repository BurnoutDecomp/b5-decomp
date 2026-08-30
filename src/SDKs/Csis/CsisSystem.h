#ifndef CSIS_SYSTEM_H
#define CSIS_SYSTEM_H

#include "types.hpp"

#include <cstddef>

namespace rw { struct IResourceAllocator; }

// ===========================================================================
// SDKs/Csis/CsisSystem.h
//
// Csis::System -- the process-global registry/lock + content-subscription manager
// in the "Csis" class/interface system (a vendor library boundary in
// BURNOUT_X360_ARTIST.XEX, the Csis::* family used by the AEMS / CgsSound audio
// content code). This header is the canonical OWNING home for:
//
//     Csis::System::Init        @ 0x82B0F1F8  (create the global mutex; mark inited)
//     Csis::System::IsInited    @ 0x82B10040  (read the inited flag)
//     Csis::System::Lock        @ 0x82B0F248  (WaitForSingleObject the mutex)
//     Csis::System::Unlock      @ 0x82B0F278  (ReleaseMutex)
//     Csis::System::Subscribe   @ 0x82B0F2A8  (register a content object's clients)
//     Csis::System::Unsubscribe @ 0x82B0F428  (deregister it)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below is
// reconstructed purely from the X360 pseudocode + asm. `Csis` is a vendor library
// boundary, so its identifiers (Csis, System) are preserved verbatim per the
// naming convention.
//
// PROCESS-GLOBAL STATE (X360 module-data symbols, asm-authoritative):
//   * dword_8324E8FC -- the global registry mutex HANDLE (CreateMutexA in Init;
//                       Lock/Unlock target it). Modelled as a file-static HANDLE.
//   * byte_8324E901  -- the "Csis::System inited" flag (Init sets 1; IsInited reads).
//   * off_8324E90C   -- head of the intrusive doubly-linked list of subscribed
//                       content objects (Subscribe pushes, Unsubscribe unlinks).
//   * word_8324E908  -- a rolling 16-bit non-negative serial counter Subscribe
//                       stamps onto each registered client (wraps to 1, never 0/neg).
//
// The PC target consumes the native-64 MOIR image from the Xbox One ABI arbiter.
// Its three array pointers, 16-byte list node, and 24/24/32-byte client records
// are wider than ARTIST's 32-bit geometry. The serialized offsets are pinned below.
// ===========================================================================

namespace Csis
{

struct InterfaceId;

// ---------------------------------------------------------------------------
// Native-64 counterpart of ARTIST's 12-byte record. The serialized interface id
// shares the status word which Unsubscribe invalidates, exactly as on the console.
// ---------------------------------------------------------------------------
struct SystemClient24
{
    u64       muRuntimeLink;    // +0x00
    uintptr_t muTargetOrOffset; // +0x08, image offset -> host pointer
    union
    {
        struct { u16 muInterfaceId; s16 miSerial; } mLive; // +0x10/+0x12
        s32 miStatus;                                     // +0x10
    } mState;
    u32 muPadding;              // +0x14
};

// ---------------------------------------------------------------------------
// Native-64 counterpart of ARTIST's 16-byte third-array record.
// ---------------------------------------------------------------------------
struct SystemClient32
{
    u8        maRuntimeHeader[16];
    uintptr_t muTargetOrOffset; // +0x10, image offset -> host pointer
    union
    {
        struct { u16 muInterfaceId; s16 miSerial; } mLive; // +0x18/+0x1A
        s32 miStatus;                                     // +0x18
    } mState;
    u32 muPadding;              // +0x1C
};

// ---------------------------------------------------------------------------
// The intrusive doubly-linked-list node embedded in every subscribed-content object
// at +0x30. The list links point at OTHER content objects' nodes,
// not at the containing objects -- exactly as the X360 stores them: Subscribe does
// `node->next = head ; head_node->prev = node` and Unsubscribe stitches
// `prev->next = next ; next->prev = prev`. The global list head (off_8324E90C) is a
// pointer to a node.
// ---------------------------------------------------------------------------
struct SystemContentNode
{
    SystemContentNode* mpNext;  // +0x00
    SystemContentNode* mpPrev;  // +0x08
};

// ---------------------------------------------------------------------------
// Native-64 subscribed-content header. Arrays start at +0x40 and run
// contiguously: update SystemClient24, destroy SystemClient24, third SystemClient32.
// ---------------------------------------------------------------------------
struct SystemContent
{
    u8  maOpaqueHeader[0x0A];  // +0x00..+0x09 (MOIR magic/version/layout)
    u16 muNumUpdate;           // +0x0A  count of array-1 (update) clients
    u16 muNumDestroy;          // +0x0C  count of array-2 (destroy) clients
    u16 muNumThird;            // +0x0E  count of array-3 clients
    u16 muSystemId;            // +0x10
    u8  maPad12[0x06];         // +0x12..+0x17
    SystemClient24* mpArray1;  // +0x18
    SystemClient24* mpArray2;  // +0x20
    SystemClient32* mpArray3;  // +0x28
    SystemContentNode mListNode; // +0x30
    u8  mauInlineArrays[1];    // +0x40
};

static_assert(sizeof(SystemClient24) == 0x18, "native CSIS client-24 layout");
static_assert(sizeof(SystemClient32) == 0x20, "native CSIS client-32 layout");
static_assert(offsetof(SystemContent, mpArray1) == 0x18, "native CSIS array offset");
static_assert(offsetof(SystemContent, mListNode) == 0x30, "native CSIS node offset");
static_assert(offsetof(SystemContent, mauInlineArrays) == 0x40, "native CSIS data offset");

// ---------------------------------------------------------------------------
// Csis::System -- all members are static (the X360 functions take no `this`; they
// operate on process-global module data and the passed content object).
// ---------------------------------------------------------------------------
class System
{
public:
    // @ 0x82B0F1C0. Install the Csis-wide allocator before Init; return -7 once
    // the system is already initialized, otherwise 0. ARTIST stores this pointer
    // in off_8324E904, which CreateInstanceFast uses to allocate "CsisAlloc"
    // records and all zero-ref paths use to free them.
    static int SetAllocator(rw::IResourceAllocator* apAllocator);

    // Typed host counterparts of off_8324E904's Alloc/Free virtual calls. The
    // native rw interface exposes those calls through its resource descriptor
    // face on PC, so these preserve ownership without relying on vtable offsets.
    static void* Allocate(size_t auSize, const char* apcName, u32 auAlignment);
    static void Free(void* apBlock);

    // @ 0x82B0F1F8 -- create the global registry mutex (CreateMutexA(0,0,0)), clear
    // the subscribed-content list head, set the inited flag. Returns 0.
    static int Init();

    // @ 0x82B10040 -- return the inited flag (byte_8324E901).
    static int IsInited();

    // @ 0x82B0F248 -- acquire the global mutex (WaitForSingleObject INFINITE). Returns 0.
    static int Lock();

    // @ 0x82B0F278 -- release the global mutex (ReleaseMutex). Returns 0.
    static int Unlock();

    // @ 0x82B0F2A8 -- register pContent: resolve its three inline client-array base
    // pointers, relocate each client's content-relative target to an absolute pointer
    // and stamp it with a fresh rolling serial, then push pContent onto the global
    // subscribed-content list. Returns 0.
    static int Subscribe(SystemContent* pContent);

    // @ 0x82B0F428 -- deregister pContent: mark every client's status word -1 and
    // unlink pContent from the global subscribed-content list. Returns 0.
    static int Unsubscribe(SystemContent* pContent);

    // Native-64 SetHandle core. kind: 0 class, 1 function, 2 global variable.
    static int ResolveInterface(u32 auKind, const InterfaceId* apId,
                                void** appDescriptor, s32* apiToken);
};

} // namespace Csis

#endif // CSIS_SYSTEM_H
