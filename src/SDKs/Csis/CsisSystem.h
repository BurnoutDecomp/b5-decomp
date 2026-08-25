#ifndef CSIS_SYSTEM_H
#define CSIS_SYSTEM_H

#include "types.hpp"

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
// SUBSCRIBED-CONTENT OBJECT (Csis::SystemContent below) -- Subscribe/Unsubscribe do
// NOT own this object; it is created by the audio content TU
// (CgsSound::Playback::CsisContent, the X360 caller of both). Subscribe walks it by
// the byte offsets the asm proves; this header models those offsets as NAMED fields
// so the bodies access them by name (no raw-offset reinterpret casts). The object is
// laid out as a header (counts + three array pointers + an intrusive list node)
// followed by three inline client arrays beginning at +0x28.
// ===========================================================================

namespace Csis
{

// ---------------------------------------------------------------------------
// A single 12-byte client record in the first two inline arrays (update- and
// destroy-notification clients). Subscribe relocates the +0x04 field from a
// content-relative offset to an absolute pointer (asm: lwz r8,4(node); add r8,r8,a1;
// stw r8,4(node)) and stamps the rolling serial into the +0x0A half-word. Unsubscribe
// writes -1 into the +0x08 word.
// ---------------------------------------------------------------------------
struct SystemClient12
{
    u8  maHeader[4];   // +0x00  (record header; not read by Subscribe/Unsubscribe)
    s32 miTargetOrOffset; // +0x04  content-relative offset on disk -> absolute pointer in Subscribe ⚠️ FLAG: 32-bit serialised slot; a 64-bit host pointer TRUNCATES here (see Subscribe)
    s32 miStatus;      // +0x08  set to -1 by Unsubscribe
    s16 miSerial;      // +0x0A  rolling serial id stamped by Subscribe
};  // sizeof == 0x0C (12)

// ---------------------------------------------------------------------------
// A single 16-byte client record in the third inline array. Subscribe relocates the
// +0x08 field (asm: lwz r8,8(node); add r8,r8,a1; stw r8,8(node)) and stamps the
// serial into +0x0E. Unsubscribe writes -1 into +0x0C.
// ---------------------------------------------------------------------------
struct SystemClient16
{
    u8  maHeader[8];   // +0x00  (record header; not read by Subscribe/Unsubscribe)
    s32 miTargetOrOffset; // +0x08  content-relative offset -> absolute pointer in Subscribe ⚠️ FLAG: 32-bit serialised slot; a 64-bit host pointer TRUNCATES here (see Subscribe)
    s32 miStatus;      // +0x0C  set to -1 by Unsubscribe
    s16 miSerial;      // +0x0E  rolling serial id stamped by Subscribe
};  // sizeof == 0x10 (16)

// ---------------------------------------------------------------------------
// The intrusive doubly-linked-list node embedded in every subscribed-content object
// at +0x20. The list links point at OTHER content objects' nodes (their own +0x20),
// not at the containing objects -- exactly as the X360 stores them: Subscribe does
// `node->next = head ; head_node->prev = node` and Unsubscribe stitches
// `prev->next = next ; next->prev = prev`. The global list head (off_8324E90C) is a
// pointer to a node.
// ---------------------------------------------------------------------------
struct SystemContentNode
{
    SystemContentNode* mpNext;  // +0x00 (node-relative; == owning content + 0x20)
    SystemContentNode* mpPrev;  // +0x04
};

// ---------------------------------------------------------------------------
// The subscribed-content object header (offsets proven by Subscribe/Unsubscribe).
// The three inline client arrays start at +0x28 (mauInlineArrays) and run
// contiguously: muNumUpdate SystemClient12, then muNumDestroy SystemClient12, then
// muNumThird SystemClient16. The list node at +0x20 links the object into the global
// subscribed-content list (off_8324E90C).
//
// FLAG: only the fields the two functions touch are recovered; +0x00..+0x09 and the
// trailing inline-array bytes belong to CgsSound::Playback::CsisContent (the owning
// TU) and are modelled as an honest opaque header span here.
// ---------------------------------------------------------------------------
struct SystemContent
{
    u8  maOpaqueHeader[0x0A];  // +0x00..+0x09  (owned by CgsSound CsisContent)
    u16 muNumUpdate;           // +0x0A  count of array-1 (update) clients
    u16 muNumDestroy;          // +0x0C  count of array-2 (destroy) clients
    u16 muNumThird;            // +0x0E  count of array-3 clients
    u8  maPad10[0x04];         // +0x10..+0x13  (unread here)
    SystemClient12* mpArray1;  // +0x14  -> &mauInlineArrays[0]
    SystemClient12* mpArray2;  // +0x18  -> mpArray1 + muNumUpdate
    SystemClient16* mpArray3;  // +0x1C  -> mpArray2 + muNumDestroy (as 16-byte records)
    SystemContentNode mListNode; // +0x20 intrusive list node {next @ +0x20, prev @ +0x24}
    u8  mauInlineArrays[1];    // +0x28  start of the contiguous inline client arrays
};

// ---------------------------------------------------------------------------
// Csis::System -- all members are static (the X360 functions take no `this`; they
// operate on process-global module data and the passed content object).
// ---------------------------------------------------------------------------
class System
{
public:
    // @ 0x82B0F1C0 (no dossier exported; behaviour attested from the ONLY caller,
    // RootSoundModule::Prepare @0x826FAF0C: r3 = &gCsisTestBedAlloc in, r3 flows
    // straight into Csis::System::Init). Install the Csis-wide allocator. FLAG:
    // the store target global + any validation are un-attested -- modelled as the
    // minimal store-and-return; the allocator is held as the opaque interface
    // pointer the caller passes (the CgsSound::TestBed::Allocator).
    static void* SetAllocator(void* apAllocator);

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
};

} // namespace Csis

#endif // CSIS_SYSTEM_H
