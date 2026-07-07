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
// This header is the canonical OWNING home for the four reconstructed member
// functions of that instance:
//
//     RealmcCore::allocator<,64>::DoInit       @ 0x82C455C0
//     RealmcCore::allocator<,64>::DoPopFront   @ 0x82C45350
//     RealmcCore::allocator<,64>::DoPushBack   @ 0x82C45A80
//     RealmcCore::allocator<,64>::DoFreeSubar  @ 0x82C453C8
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
//   +0x08  mpFrontBegin  (a1[2]) -- begin of the current front page
//   +0x0C  mpFrontCur    (a1[3]) -- front read cursor (starts at the page begin)
//   +0x10  mpFrontEnd    (a1[4]) -- one past the current front page (begin+0x100)
//   +0x14  mppFrontSlot  (a1[5]) -- the mppPageArray slot the front page lives in
//   +0x18  mpBackCur     (a1[6]) -- back write cursor inside the current back page
//   +0x1C  mpBackBegin   (a1[7]) -- begin of the current back page
//   +0x20  mpBackEnd     (a1[8]) -- one past the current back page (begin+0x100)
//   +0x24  mppBackSlot   (a1[9]) -- the mppPageArray slot the back page lives in
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
    char*  mpFrontBegin;  // +0x08
    char*  mpFrontCur;    // +0x0C
    char*  mpFrontEnd;    // +0x10
    char** mppFrontSlot;  // +0x14
    char*  mpBackCur;     // +0x18
    char*  mpBackBegin;   // +0x1C
    char*  mpBackEnd;     // +0x20
    char** mppBackSlot;   // +0x24

    // @ external (sub RealmcCore::allocator<,64>::DoReallocPt) -- grows the page-
    // pointer array when DoPushBack runs out of back slots. Defined by the sibling
    // Realmc allocator TU; declared here so DoPushBack can call it across the TU
    // boundary. (Un-homed dependency -- see home_notes.)
    void DoReallocPt(int nArg1, int nArg2);
};

} // namespace RealmcCore
