#pragma once

// ===========================================================================
// RealmcCore::allocator<,64> -- the Realmc chunked-deque page allocator (the
// X360 template instance `RealmcCore::allocator<T, 64>`). It is a segmented
// ring of fixed 256-byte pages reached through a heap-allocated array of page
// pointers, with independent front (read/pop) and back (write/push) cursors --
// the classic std::deque-style block manager. The "64" template argument is the
// element granularity the X360 page-count math divides the byte capacity by
// (a2 >> 6 == a2 / 64).
//
// This header is the canonical OWNING home for the reconstructed member
// functions of that instance:
//
//     RealmcCore::allocator<,64>::allocator    @ 0x82C45B48  (export sym `Rea`)
//     RealmcCore::allocator<,64>::~allocator   @ 0x82C45A10  (export sym `Re`)
//     RealmcCore::allocator<,64>::DoInit       @ 0x82C455C0
//     RealmcCore::allocator<,64>::DoPopFront   @ 0x82C45350
//     RealmcCore::allocator<,64>::DoPushBack   @ 0x82C45A80
//     RealmcCore::allocator<,64>::DoFreeSubar  @ 0x82C453C8
//
// The two truncated export symbols `Rea` / `Re` were identified in wave L by
// xref, not by name: their only X360 callers are the MemcardState ctor/dtor
// (0x82C47328 / 0x82C46470), and their bodies zero / free exactly this class's
// ten members. See scratchpad/waveL/MemcardState.spec.md section 1.
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers are preserved per the naming convention.
// All pages and the page-pointer array are obtained from / returned to the
// shared Realmc allocator backend (g_pRealmcAllocator, in RealmcCore.h), stamped
// with the "RealmcCore::allocator" tag, exactly like the stateless adaptor there.
//
// LAYOUT (from the DoInit store offsets; dword indices a1[n] == byte +0x4*n):
//   +0x00  mppPageArray  (a1[0]) -- base of the heap char*[] page-pointer array
//   +0x04  mnPageSlots   (a1[1]) -- number of slots in mppPageArray
//   +0x08  mpFrontCur    (a1[2]) -- front READ cursor inside the current front page
//   +0x0C  mpFrontBegin  (a1[3]) -- begin of the current front page
//   +0x10  mpFrontEnd    (a1[4]) -- one past the current front page (begin+0x100)
//   +0x14  mppFrontSlot  (a1[5]) -- the mppPageArray slot the front page lives in
//   +0x18  mpBackCur     (a1[6]) -- back WRITE cursor inside the current back page
//   +0x1C  mpBackBegin   (a1[7]) -- begin of the current back page
//   +0x20  mpBackEnd     (a1[8]) -- one past the current back page (begin+0x100)
//   +0x24  mppBackSlot   (a1[9]) -- the mppPageArray slot the back page lives in
//
// CORRECTION (wave L): the +0x08 / +0x0C pair was both DOCUMENTED and NAMED the
// other way round here before this pass -- the comment claimed +0x08 was the page
// begin and +0x0C the read cursor. It is the reverse. The two quartets are the
// classic std::deque iterator {cur, first, last, node} and are laid out
// identically -- front at +0x08/+0x0C/+0x10/+0x14, back at +0x18/+0x1C/+0x20/+0x24.
// MEASURED proof (the bodies were always offset-faithful; only the names/comments
// inverted, which is exactly how a later sweep would have "fixed" working code):
//   * DoInit @ 0x82C455C0 re-seats the front as +0x0C = page, +0x10 = page+0x100,
//     +0x08 = page, and the back as +0x1C = page, +0x20 = page+0x100,
//     +0x18 = page + ((capacity & 0x3F) << 2). Same store order, so +0x08 is the
//     front's counterpart of +0x18 -- i.e. the cursor. DoPopFront @ 0x82C45350 and
//     DoPushBack @ 0x82C45A80 repeat that same (first, last, cur) order.
//   * DoPopFront FREES *(this+0x0C) for 0x100 bytes before advancing the slot --
//     what is freed is a page begin, never a cursor.
//   * MemcardState::GetWaitingToStartTask @ 0x82C46528 loads the element from
//     *(deque+0x08), adds 4 and stores it back to +0x08, comparing against +0x10
//     (page end) -- +0x08 is the moving read cursor.
//
// mppFrontSlot / mppBackSlot are pointers INTO mppPageArray (char**), so they are
// dereferenced to reach the live front/back page; the cursors are char* into the
// 256-byte pages. The X360-absolute offsets above are reproduced by member NAME,
// not asserted on the 64-bit host (host pointers are 8 bytes wide).
// ===========================================================================

#include <cstddef>

namespace RealmcCore
{

class Allocator64
{
public:
    // @ 0x82C45B48 (export-truncated symbol `Rea`) -- the deque constructor: zero
    //   every member, then seat the page ring via DoInit(nByteCapacity). MEASURED:
    //   ten `stw r11(=0)` at +0x00..+0x24 (the member init-list), then
    //   `bl DoInit` with r3 = this and r4 passed straight through, then
    //   `mr r3, r31 / blr` (returns this). The zeroing is belt-and-braces: DoInit
    //   rewrites all ten fields.
    //   PARAMETER COUNT -- MEASURED, AND DELIBERATELY NARROWED. The host signature
    //   below is (this, nByteCapacity); it is a narrowing of the X360 one, NOT a
    //   claim that the X360 one was the same. Do not "restore" a third parameter,
    //   and do not re-derive this as "the ctor takes one parameter": the sole X360
    //   call site, MemcardState::MemcardState @ 0x82C47328, passes THREE registers.
    //     0x82C47340  addi r5, r1, 0x80+var_30   ; r5 = &<1-byte stack temporary>
    //     0x82C47344  addi r3, r31, 0x1C         ; r3 = &maStartWaitingQueue
    //     0x82C4734C  li   r4, 0                 ; r4 = capacity 0
    //     0x82C47364  bl   Rea
    //   r5 is not reloaded until 0x82C47384 (for the NEXT call), so that address is
    //   live into `Rea`; IDA's own frame gives `char v12; // [sp+50h] [-30h] BYREF`
    //   and renders the call `Rea(a1 + 28, 0, &v12)` -- a 1-byte, never-written
    //   stack object, i.e. an empty/stateless allocator temporary passed by
    //   reference (the usual EASTL container-allocator argument).
    //   NEITHER CALLEE READS IT: `Rea` @ 0x82C45B48 does only `mr r31, r3`, ten
    //   zero-stores and `bl DoInit`; DoInit @ 0x82C455C0 opens `mr r25, r4 /
    //   mr r31, r3` and WRITES r5 at 0x82C45618 as the allocator TAG argument
    //   before ever reading it. The parameter is therefore DROPPED on the host --
    //   it carries no state and the host allocator is reached through
    //   g_pRealmcAllocator -- but it does exist in the X360 signature.
    //   Sole X360 caller: MemcardState::MemcardState @ 0x82C47328, capacity 0.
    //   Body home: RealmcAllocator64.cpp.
    explicit Allocator64(unsigned int nByteCapacity);

    // @ 0x82C45A10 (export-truncated symbol `Re`) -- the deque destructor: when
    //   the page array exists, free every live page in the slot range
    //   [mppFrontSlot, mppBackSlot + 1) via DoFreeSubar, then free the page-pointer
    //   array itself through the backend (vtable +12). MEASURED:
    //     lwz r11,0(this); beq exit                     ; if (mppPageArray)
    //     lwz r11,0x24(this); lwz r4,0x14(this); addi r5,r11,4; bl DoFreeSubar
    //     lwz r4,0(this); beq exit                      ; redundant re-check
    //     lwz r10,4(this); slwi r5,r10,2; ...+0xC ...   ; Free(array, 4*mnPageSlots)
    //   HOST WIDTH: the `+ 4` is ONE page-array slot -> `+ 1` in host `char**`
    //   arithmetic, and `slwi ..,2` is the console 4-byte slot width ->
    //   `sizeof(char*) * mnPageSlots` on the host. Neither console literal may
    //   survive in code.
    //   PAIRED WITH THE ALLOCATION -- DO NOT CHANGE ONE WITHOUT THE OTHER. This
    //   sized free must use the SAME element width as the matching allocation in
    //   DoInit (`allocate(sizeof(char*) * luSlots, 0)`, RealmcAllocator64.cpp).
    //   A console-width `4 * mnPageSlots` on either side alone is a live heap
    //   mismatch on the LLP64 host: MemcardState's capacity-0 deque takes the
    //   `max(.., 8)` slot floor, so it would allocate 32 bytes and free 64.
    //   Sole X360 caller: ~MemcardState @ 0x82C46470.
    //   Body home: RealmcAllocator64.cpp.
    ~Allocator64();

    static const std::size_t KU_PageBytes  = 0x100;  // 256-byte page
    static const std::size_t KU_ElementLog = 6;       // a2 >> 6 == a2 / 64

    // @ 0x82C455C0 -- size the page-pointer array to max((nByteCapacity>>6)+3, 8)
    //                 slots, allocate it, pre-allocate (nByteCapacity>>6)+1 pages
    //                 into a centred run of slots, and seat the front cursor on
    //                 the first page and the back cursor at
    //                 ((nByteCapacity & 0x3F) << 2) into the last page. Returns
    //                 the last allocation result (the X360 returns r3).
    void* DoInit(unsigned int nByteCapacity);

    // @ 0x82C45350 -- free the current front page (through the backend) and
    //                 advance mppFrontSlot to the next slot, reseating the front
    //                 begin/cur/end on that page. Returns the backend free result
    //                 (or this, when there was no page to free).
    void* DoPopFront();

    // @ 0x82C45A80 -- push the dword *pValue at the back: grow the page array
    //                 (DoReallocPt) when the back slot has reached capacity,
    //                 allocate a fresh 256-byte page into the next slot, write
    //                 the value at the current back cursor, then advance the back
    //                 slot/begin/cur/end onto the new page. Returns the new page.
    void* DoPushBack(const int* pValue);

    // @ 0x82C453C8 -- free every non-null page pointer in the half-open array
    //                 range [ppBegin, ppEnd) through the backend (256 bytes each).
    static void DoFreeSubar(char** ppBegin, char** ppEnd);

    // ---- element-level helpers de-inlined from RealmcCore::MemcardState ----
    // These reproduce the deque front-dequeue / back-enqueue the X360 folded
    // inline into MemcardState::GetWaitingToStartTask / PutInStartWaitingQueue.
    // They operate on 4-byte (int) elements -- the same element model DoPushBack
    // commits to -- and defer to DoPopFront / DoPushBack at a page boundary. They
    // are NOT their own X360 functions; homing them here keeps MemcardState free
    // of raw offset access into this deque.

    // Front-dequeue one element (X360 GetWaitingToStartTask): 0 when empty (the
    //   front cursor has met the back cursor), else read the front int and advance
    //   the front cursor, freeing the exhausted page via DoPopFront at a boundary.
    int PopFront();

    // Back-enqueue one element (X360 PutInStartWaitingQueue): write iValue at the
    //   back cursor and advance it, allocating a fresh page via DoPushBack when the
    //   current page is full.
    void PushBack(int iValue);

private:
    char** mppPageArray;  // +0x00
    int    mnPageSlots;   // +0x04
    char*  mpFrontCur;    // +0x08  read cursor  (deque iterator `cur`)
    char*  mpFrontBegin;  // +0x0C  page begin   (deque iterator `first`)
    char*  mpFrontEnd;    // +0x10  page end     (deque iterator `last`)
    char** mppFrontSlot;  // +0x14  owning slot  (deque iterator `node`)
    char*  mpBackCur;     // +0x18  write cursor (deque iterator `cur`)
    char*  mpBackBegin;   // +0x1C  page begin   (deque iterator `first`)
    char*  mpBackEnd;     // +0x20  page end     (deque iterator `last`)
    char** mppBackSlot;   // +0x24  owning slot  (deque iterator `node`)

    // @ external (sub RealmcCore::allocator<,64>::DoReallocPt) -- grows the page-
    // pointer array when DoPushBack runs out of back slots. Defined by the sibling
    // Realmc allocator TU; declared here so DoPushBack can call it across the TU
    // boundary. (Un-homed dependency -- see home_notes.)
    void DoReallocPt(int nArg1, int nArg2);
};

} // namespace RealmcCore
