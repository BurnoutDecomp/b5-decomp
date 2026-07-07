#pragma once

// ===========================================================================
// SDKs/Realmc/RealmcContainers.h
//
// The EASTL container instantiations that the X360 build parameterises on the
// custom `RealmcCore::allocator` (RealmcCore.h). IDA lumps every member of these
// instances under one mangled prefix (`RealmcCore::allocator<...>`), so this TU
// is a grab-bag of THREE distinct containers, homed together here because they
// share the one allocator backend and no other file owns them:
//
//   * String16 == eastl::basic_string<char16_t, RealmcCore::allocator>
//         RealmcCore::allocator<>::AllocateSelf     @ 0x82B55508
//         RealmcCore::allocator<>::RangeInitialize  @ 0x82B55580
//         RealmcCore::allocator<>::erase            @ 0x82B57758
//   * MessageList == eastl::list<RealmcCore::MessagePtr, RealmcCore::allocator>
//         RealmcCore::allocator<>::DoClear          @ 0x82C46770
//         RealmcCore::allocator<>::DoErase          @ 0x82C46110
//   * IntVector == eastl::vector<int, RealmcCore::allocator>
//         RealmcCore::allocator<>::DoInsertValue    @ 0x82C46DE8
//         RealmcCore::allocator<>::reserve          @ 0x82C470C8
//
// These are the container bodies the Wave-2 RealmcCore::MessageQueue TU blocked
// on (its list nodes route through MessageList::DoClear / DoErase). There is no
// Feb-2007 leak source and no DWARF for this TU, so SHAPE and BODIES both come
// from the X360 pseudocode + asm (BURNOUT_X360_ARTIST.XEX). `Realmc` is a vendor
// boundary, so its identifiers are preserved per the naming convention; every
// allocation routes through RealmcCore::allocator / g_pRealmcAllocator, exactly
// like the sibling RealmcCore.cpp / RealmcAllocator64.cpp reconstructions.
//
// POINTER-WIDTH NOTE (same rule as every recon). The X360 element/pointer math
// (`>>1` for 2-byte char16, `>>2` for 4-byte int/pointer, node free size 16) is
// over 4-byte pointers; on the 64-bit host the same logical counts are recovered
// by named-member pointer arithmetic and `sizeof`, so the byte spans differ but
// the element counts / links are identical. Members are reached BY NAME; the
// X360-absolute offsets in the comments are documentation, not host asserts.
// ===========================================================================

#include <cstddef>
#include <cstdint>

#include "SDKs/Realmc/RealmcCore.h"   // RealmcCore::allocator / g_pRealmcAllocator / MessagePtr

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// String16 -- eastl::basic_string<char16_t, RealmcCore::allocator>.
//
// LAYOUT (from the AllocateSelf / RangeInitialize store offsets; dword index n
// == X360 byte +0x4*n):
//   +0x00  mpBegin   (a1[0]) -- start of the char16 buffer (or the shared empty
//                              singleton gRealmcEmptyString16 when empty)
//   +0x04  mpEnd     (a1[1]) -- one past the last char16 (the NUL terminator)
//   +0x08  mpCapEnd  (a1[2]) -- one past the allocated char16 buffer
//   +0x0C  allocator name word -- AllocateSelf passes `this+0xC` as the (stateless)
//                              allocator's implicit `this`; kept for layout.
// ---------------------------------------------------------------------------
class String16
{
public:
    // @ 0x82B55508 -- when nCapacity <= 1, point begin/end at the shared empty
    //                 char16 singleton and capEnd one char16 past it (no alloc);
    //                 otherwise allocate nCapacity char16 through the backend and
    //                 seat begin = end = buffer, capEnd = buffer + nCapacity.
    //                 Returns the buffer (large path) or `this` (small path, the
    //                 X360 leaves r3 == the container pointer).
    void* AllocateSelf(unsigned int nCapacity);

    // @ 0x82B55580 -- initialise from the [pFirst, pLast) char16 range: reserve
    //                 (count + 1) via AllocateSelf, copy the range, set mpEnd and
    //                 NUL-terminate at mpEnd.
    void RangeInitialize(const char16_t* pFirst, const char16_t* pLast);

    // @ 0x82B57758 -- erase [pFirst, pLast): move the tail (incl. the NUL) down
    //                 onto pFirst and shrink mpEnd. Returns pFirst.
    char16_t* erase(char16_t* pFirst, char16_t* pLast);

    // ---- special members ----------------------------------------------------
    // The eastl::basic_string ctor/dtor the X360 folds inline into its owners
    // (e.g. RealmcCore::Trc's copy ctor / destructor), de-inlined here so those
    // owners construct/destroy their String16 members BY NAME instead of poking
    // the {begin,end,capEnd} words directly.

    // Default -- the empty string: begin = end = the shared empty char16 singleton
    // and capEnd one char16 past it (exactly the small path of AllocateSelf(1); no
    // allocation). This is the state the Trc copy ctor seats each option string in.
    String16();

    // Range copy -- construct directly from the [pFirst, pLast) char16 range via
    // RangeInitialize (allocates when the range is non-empty). Backs a String16
    // member that is copy-constructed from another (the Trc mMessage member).
    String16(const char16_t* pFirst, const char16_t* pLast);

    // Free the owned heap buffer when the string holds one -- capacity > 1 char16
    // AND begin is a real allocation (not the empty singleton / null). Matches the
    // X360 inlined ~String16: v5 = (capEnd - begin) >> 1; if (v5 > 1 && begin) free.
    ~String16();

    // @ 0x82B579E8 (owned by its own String16 TU) -- assign the [pFirst, pLast)
    // char16 range over the current contents (reusing or reallocating storage),
    // returning mpBegin. Declared here so the cross-TU callers (the Trc copy ctor
    // and RealmcCore::Trc::_SetMsgOptions) compile against it; the body is not part
    // of this TU. (Same declare-across-the-TU-boundary pattern as IntVector::DoRealloc.)
    char16_t* assign(const char16_t* pFirst, const char16_t* pLast);

    // Range accessors so owners can hand [Begin(), End()) to RangeInitialize / assign
    // without touching the private pointer words.
    char16_t* Begin() const { return mpBegin; }
    char16_t* End()   const { return mpEnd; }

private:
    char16_t*   mpBegin;         // +0x00
    char16_t*   mpEnd;           // +0x04
    char16_t*   mpCapEnd;        // +0x08
    const char* mpAllocatorName; // +0x0C (stateless allocator name word)
};

// ---------------------------------------------------------------------------
// MessageList -- eastl::list<RealmcCore::MessagePtr, RealmcCore::allocator>.
//
// A circular intrusive list. The head sentinel {next, prev} points at itself
// when empty; each real node is a 16-byte (X360) {next, prev, MessagePtr value}.
// ---------------------------------------------------------------------------
struct MessageListNodeBase
{
    MessageListNodeBase* mpNext;  // +0x00
    MessageListNodeBase* mpPrev;  // +0x04
};

struct MessageListNode : MessageListNodeBase
{
    MessagePtr maValue;           // +0x08 (the held MessagePtr; dtor'd on erase/clear)
};

class MessageList
{
public:
    // @ 0x82C46770 -- walk from the first node to the sentinel, destroy each
    //                 node's MessagePtr and free the node (16 bytes on X360) via
    //                 the backend. Backs MessageQueue::~MessageQueue's list teardown.
    void DoClear();

    // @ 0x82C46110 -- unlink pNode from the list, destroy its MessagePtr, and free
    //                 the node (16 bytes on X360) via the backend. The list `this`
    //                 is unused (the node self-links), matching the X360 asm.
    void DoErase(MessageListNode* pNode);

private:
    MessageListNodeBase mSentinel;        // +0x00 {next, prev} (self-referential when empty)
    const char*         mpAllocatorName;  // +0x08 (stateless allocator name word)
};

// ---------------------------------------------------------------------------
// IntVector -- eastl::vector<int, RealmcCore::allocator>.
//
// LAYOUT (from the DoInsertValue / reserve store offsets):
//   +0x00  mpBegin   (a1[0])
//   +0x04  mpEnd     (a1[1])
//   +0x08  mpCapEnd  (a1[2])
//   +0x0C  allocator name word
// ---------------------------------------------------------------------------
class IntVector
{
public:
    // @ 0x82C46DE8 -- insert *pValue at pPos. When full (mpEnd == mpCapEnd),
    //                 reallocate to max(2*size, 1), copy prefix + value + suffix,
    //                 free the old buffer; otherwise shift [pPos, mpEnd) up one
    //                 slot (fixing pValue when it aliases the moved range) and
    //                 write the value at pPos.
    void DoInsertValue(int* pPos, const int* pValue);

    // @ 0x82C470C8 -- grow capacity to nCapacity elements when it exceeds the
    //                 current capacity: DoRealloc a fresh buffer, free the old one,
    //                 and reseat begin/end/capEnd.
    void reserve(unsigned int nCapacity);

    // ---- element-level helpers de-inlined from RealmcCore::MemcardState ----
    // These reproduce the eastl::vector front()/back()/push_back()/pop_back()
    // operations the X360 folded inline into MemcardState's task-stack methods
    // (GetMainTask / GetCurrentTask / StartTask / StopTask / StopAndStartTask).
    // They are NOT their own X360 functions; homing them here keeps MemcardState
    // free of raw offset access into this container.

    // front() with the X360's empty guard: 0 when begin == end, else *mpBegin.
    int GetFront() const { return mpBegin == mpEnd ? 0 : *mpBegin; }

    // back() with the X360's empty guard: 0 when begin == end, else *(mpEnd - 1).
    int GetBack() const { return mpBegin == mpEnd ? 0 : mpEnd[-1]; }

    // pop_back(): drop the last element (the X360 decrements mpEnd unconditionally).
    void pop_back() { --mpEnd; }

    // push_back(): when at capacity (mpEnd >= mpCapEnd) grow through DoInsertValue
    //   at mpEnd; otherwise advance mpEnd first, then write the value at the old
    //   slot (matching the X360 store order).
    void push_back(const int& iValue)
    {
        if (mpEnd >= mpCapEnd)
        {
            DoInsertValue(mpEnd, &iValue);
        }
        else
        {
            int* pSlot = mpEnd;
            ++mpEnd;
            if (pSlot)
                *pSlot = iValue;
        }
    }

private:
    // @ external (eastl::vector<int,RealmcCore::allocator>::DoRealloc<int*>) --
    // allocates the grown buffer and moves the live elements. Defined by its own
    // TU; declared here so reserve() can call it across the TU boundary.
    int* DoRealloc(unsigned int nCapacity, int* pBegin, int* pEnd);

    int*        mpBegin;         // +0x00
    int*        mpEnd;           // +0x04
    int*        mpCapEnd;        // +0x08
    const char* mpAllocatorName; // +0x0C (stateless allocator name word)
};

} // namespace RealmcCore
