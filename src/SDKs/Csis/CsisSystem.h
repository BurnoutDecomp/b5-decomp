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
struct SystemContent;
struct SystemContentNode;






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
