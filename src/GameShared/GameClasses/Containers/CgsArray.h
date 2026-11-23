#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstdlib>   // std::qsort (Array<T,N>::QSort)

// CgsContainers::Array<T, N> - the fixed-capacity inline array of the Cgs containers.
// Namespace, member set, names and lines from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameShared/GameClasses/Containers/CgsArray.h); the shape of
// the header (namespace CgsContainers, the class declaring its members, then the inline
// definitions after it) from the LegacySrc copy of the same header; every body from ARTIST.
// ARTIST names every out-of-line copy CgsContainers::Array<T,N>::..., e.g.
// CgsContainers::Array<int,20>::Append @0x821FBCD0.
//
// LAYOUT (ARTIST): the inline element buffer, then one count word that holds the -1 sentinel
// until Construct/Clear and the live length afterwards. The DWARF names the pair maElements
// (h:171) and muLength (h:172, uint32_t; sentinel KU_INVALID = 0xFFFFFFFF, h:49). The tree
// keeps the names miCount / KI_UNCONSTRUCTED (same width and bit pattern) because explicit
// specializations in other directories read them.
//
// SOURCE LINES: ARTIST bakes this header's line numbers into every inlined assert (li r5,
// <line> before CgsDev::Assert::FireAssert, file string @0x82005634). The DWARF puts each
// definition on the line named below; its asserts sit ten lines lower, on the lines ARTIST
// passes. The comment lines inside each body hold those positions.
//
//
namespace CgsContainers
{
    template <typename T, u32 N> class Set;

// STILL DIFFERENT from ARTIST (recorded, not yet restored):
// - The out-of-space (226/290), insert-index (312), bounds (539/557) and QSort (636) asserts
//   stream their message into CgsDev::Assert::gpcMessageBuffer through a StrStream on the
//   console; here they pass a plain string.
// - The DWARF signatures differ where explicit instantiations in other directories depend on
//   ours: AppendArray is a member template on the other array's capacity
//   (??$AppendArray@$0CA@@...), FindFirstInstanceOf returns uint32_t (KU_INVALID),
//   InsertBefore takes a T&, QSort returns void.
// - h:201 Construct(uint32_t) is declared by the DWARF; no ARTIST copy attests its body, so it
//   is not reconstructed.
//
//
//
template <typename T, u32 N>
class Array
{
public:
    static const s32 KI_UNCONSTRUCTED = -1; // DWARF h:49 KU_INVALID (uint32_t 0xFFFFFFFF)
    static const u32 KU_SIZE          = N;

    Array() { miCount = KI_UNCONSTRUCTED; }

    // The default constructor (h:52) leaves the count word on the KI_UNCONSTRUCTED(-1)
    // sentinel. Owners whose constructors are inlined show it as a bare `stw -1` into the
    // count; the out-of-line copy is the Array<Array<BehaviourHelperIndex,28>,28>
    // instantiation built by BehaviourManager::BehaviourManager (inner counts +0x70 = -1 per
    // element, outer count +0xCB0 = -1 last).

    // Construct h:186 / Clear h:355: empty but usable (count 0).
    inline void Construct();

    // Append h:215: push one element.
    inline void Append(const T& lrElement);

    // AppendArray h:235: append every live element of another array.
    inline void AppendArray(const Array<T, N>& lrSource);

    // AppendSet h:258: append a set's elements in encounter order.
    template <u32 M>
    inline void AppendSet(const Set<T, M>& lSet);

    // Grow h:279: reserve the next free slot and return it unwritten.
    inline T* Grow();

    // InsertBefore h:302: insert so the element sits at luIndex.
    inline void InsertBefore(u32 luIndex, const T& lrElement);

    // GetLength h:326 / GetCapacity h:341: the checked live count and the capacity.
    inline u32 GetLength() const;
    inline u32 GetCapacity() const;

    // Clear h:355.
    inline void Clear();

    // Erase h:370 (order-preserving), EraseFast h:395 (unordered), EraseInstancesOf h:416.
    inline void Erase(u32 luIndex);
    inline void EraseFast(u32 luIndex);
    inline void EraseInstancesOf(const T& lrElement);

    // CountInstancesOf h:443, FindFirstInstanceOf h:470, Contains h:496, IsFull h:512.
    inline u32 CountInstancesOf(const T& lrElement) const;
    inline s32 FindFirstInstanceOf(const T& lrElement) const;
    inline bool Contains(const T& lrElement) const;
    inline bool IsFull() const;

    // GetItem h:528 (const) / h:546: the checked accessors; operator[] h:563 / h:577 forward.
    inline const T& GetItem(u32 luIndex) const;
    inline T& GetItem(u32 luIndex);
    inline const T& operator[](u32 luIndex) const;
    inline T& operator[](u32 luIndex);

    // Begin h:593 / End h:610: the live range.
    inline T* Begin();
    inline T* End();

    // QSort (h:626): in-place sort through the C library qsort.
    inline T* QSort(int (*lpfComparator)(const void* lpA, const void* lpB));

    // ===== Members the tree's callers use that the DWARF does not declare =====
    // Each is a raw count access that ARTIST shows inline without an assert, or a forwarding
    // alias of an original member (it inlines that member's body with its original lines).

    // Drop back to the pre-Construct state. ARTIST shows it as a bare `stw -1` into the count
    // word from an owner's constructor, e.g. CgsGui::AnimData::AnimData @0x824E8FC8 (both
    // embedded counts @+0x90/+0xAC) before any Construct runs.
    void MarkUnconstructed() { miCount = KI_UNCONSTRUCTED; }

    // Fill a fixed index->element table: live count = capacity N. ARTIST
    // GameMainFlowController::Construct @0x823C6440 stores N(7) straight into maStates' count
    // word @+0x4C, then assigns all N entries through the checked accessor.
    void SetFullCount() { miCount = static_cast<s32>(N); }

    // The raw count word (-1 before Construct/Clear), read without GetLength's assert.
    s32 GetCount() const { return miCount; }

    // The capacity under the tree's older name; GetCapacity (h:341) is the original.
    u32 GetSize() const { return N; }

    // The tree's older names for GetItem (h:528 / h:546) and Grow (h:279).
    T&       Ge(u32 luIndex)       { return GetItem(luIndex); }
    const T& Ge(u32 luIndex) const { return GetItem(luIndex); }
    T*       AddNew()              { return Grow(); }




































private:
    T   maElements[N];  // DWARF h:171
    s32 miCount;        // DWARF h:172 muLength (uint32_t); -1 == not yet Constructed/Cleared
};

// Construct / Clear: ARTIST does this to each embedded Array<> in its owner's Construct with a
// single `stw 0` into the count word, e.g. GameModeParams::Construct @0x8231C370
// (maStartLocations +0x250, maCheckpointDataArray +0x520). Construct() and Clear() are the two
// names the assert text attests; the inline element buffer needs no per-element work.





template <typename T, u32 N>
inline void
Array<T, N>::Construct()
{
    miCount = 0;
}























template <typename T, u32 N>
inline void
Array<T, N>::Append(const T& lrElement)
{
    // Push one element (out-of-line copies include Array<int,20>::Append @0x821FBCD0 and
    // Array<float,10>::Append @0x821FDAD0). ARTIST streams the capacity message
    // ("Array container out of space, Length: " << count << ", Capacity: " << N); it is a
    // plain string here. The unsigned compare keeps the -1 sentinel failing.
    //
    //
    //
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    CGS_ASSERT(static_cast<u32>(miCount) < N, "Array container out of space");
    maElements[miCount] = lrElement;
    ++miCount;
}



template <typename T, u32 N>
inline void
Array<T, N>::AppendArray(const Array<T, N>& lrSource)
{
    // Append every live element of lrSource (ARTIST
    // Array<OverheadSignScore,32>::AppendArray<32> @0x822E5348, @0x82374258).
    //
    u32 luIndex;    // DWARF h:240
    // ResourceRegistrar::ClearUnreferancedFiles (0x826E2080) uses it to push the removal
    // candidates back into the map. lrSource's own constructed check is the GetLength inside
    // the capacity assert (336); the loop re-reads GetLength on every pass.
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    CGS_ASSERT((lrSource.GetLength() + static_cast<u32>(miCount)) <= N, "Array container out of space appending an array");
    for (luIndex = 0; luIndex < lrSource.GetLength(); ++luIndex)
    {
        Append(lrSource.GetItem(luIndex));
    }
}



template <typename T, u32 N>
template <u32 M>
inline void
Array<T, N>::AppendSet(const Set<T, M>& lSet)
{
    // Append a set's elements in its encounter order; the destination stays an Array
    // (ARTIST Array<u16,72>::AppendSet<72> @0x8271B5A8, outside the export set).
    //
    u32 luIndex;    // DWARF h:263
    //
    //
    //
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    CGS_ASSERT(lSet.GetLength() + static_cast<u32>(miCount) <= N, "Array container out of space appending a set");
    for (luIndex = 0; luIndex < lSet.GetLength(); ++luIndex)
    {
        Append(lSet.GetItem(luIndex));
    }
}


template <typename T, u32 N>
inline T*
Array<T, N>::Grow()
{
    // Reserve the next free slot and return it unwritten; the caller fills it in place
    // (ARTIST Array<OverheadSignScore,32>::Grow @0x822ABF70 and the
    // Array<ChainableMultiplierInfo,8> copy @0x823177E8). ARTIST streams the capacity
    // message exactly as Append does; it is a plain string here.
    //
    //
    //
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    CGS_ASSERT(static_cast<u32>(miCount) < N, "Array container out of space");
    T* lpNewElement = &maElements[miCount];
    ++miCount;
    return lpNewElement;
}





template <typename T, u32 N>
inline void
Array<T, N>::InsertBefore(u32 luIndex, const T& lrElement)
{
    // Insert so the element sits at luIndex, shifting the tail up one slot (ARTIST
    // Array<int,20>::InsertBefore @0x822111B0, ICEMoviePlaylist::InsertMovieBefore). The
    // index check reads GetLength() (its 336 fires first); ARTIST streams "trying to insert
    // before an invalid index: " << luIndex, a plain string here.
    //
    //
    //
    //
    CGS_ASSERT(luIndex <= GetLength(), "trying to insert before an invalid index");
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    CGS_ASSERT(static_cast<u32>(miCount) < N, "Array container out of space");
    for (u32 luShift = GetLength(); luShift > luIndex; --luShift)
    {
        maElements[luShift] = maElements[luShift - 1];
    }
    ++miCount;
    maElements[luIndex] = lrElement;
}


template <typename T, u32 N>
inline u32
Array<T, N>::GetLength() const
{
    // The checked live count. Inlined into hundreds of ARTIST callers with this assert on
    // line 336 (li r5,0x150), e.g. BrnGui::GuiCache::GetScoringTrafficCount @0x824497C0;
    // out of line in Array<DriveThruInfo,46>::GetLength @0x823AC200.
    //
    //
    //
    //
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    return static_cast<u32>(miCount);
}
template <typename T, u32 N>
inline u32
Array<T, N>::GetCapacity() const
{
    // Attested by the assert text baked into BrnDirector::MomentSelector::AddMoment
    // @0x82209F80: "mMomentDescriptionArray.GetLength() < mMomentDescriptionArray.GetCapacity()".
    return N;
}






template <typename T, u32 N>
inline void
Array<T, N>::Clear()
{
    miCount = 0;
}









template <typename T, u32 N>
inline void
Array<T, N>::Erase(u32 luIndex)
{
    // Remove the element at luIndex, shifting the tail down one slot (order-preserving;
    // ARTIST Array<int,20>::Erase @0x821FBDF0).
    //
    //
    //
    //
    //
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    CGS_ASSERT(luIndex < static_cast<u32>(miCount), "Trying to erase an unused element");
    --miCount;
    for (; luIndex < static_cast<u32>(miCount); ++luIndex)
    {
        maElements[luIndex] = maElements[luIndex + 1];
    }
}





template <typename T, u32 N>
inline void
Array<T, N>::EraseFast(u32 luIndex)
{
    // Unordered erase: the last live element overwrites luIndex, then the count drops (a
    // self-copy when luIndex is already last, as on the console; ARTIST
    // Array<BankingScore,6>::EraseFast @0x82449018).
    //
    //
    //
    //
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    CGS_ASSERT(luIndex < static_cast<u32>(miCount), "Trying to erase an unused element");
    maElements[luIndex] = maElements[miCount - 1];
    --miCount;
}




template <typename T, u32 N>
inline void
Array<T, N>::EraseInstancesOf(const T& lrElement)
{
    // Erase every live element equal to lrElement, compacting the array in order (ARTIST
    // Array<int,20>::EraseInstancesOf @0x821FBEA0): each slot is erased while it still
    // matches -- the match test first, then the index test -- before the index advances.
    //
    //
    //
    //
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    for (u32 luIndex = 0; luIndex < static_cast<u32>(miCount); ++luIndex)
    {
        while ((lrElement == maElements[luIndex]) && (luIndex < static_cast<u32>(miCount)))
        {
            Erase(luIndex);
        }
    }
}






template <typename T, u32 N>
inline u32
Array<T, N>::CountInstancesOf(const T& lrElement) const
{
    // Count the live elements equal to lrElement (ARTIST
    // Array<BehaviourHelperIndex,28>::CountInstancesOf @0x821FCC40).
    //
    //
    //
    //
    //
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    u32 luMatches = 0;
    for (u32 luIndex = 0; luIndex < static_cast<u32>(miCount); ++luIndex)
    {
        if (lrElement == maElements[luIndex])
        {
            ++luMatches;
        }
    }
    return luMatches;
}




template <typename T, u32 N>
inline s32
Array<T, N>::FindFirstInstanceOf(const T& lrElement) const
{
    // The index of the first element equal to lrElement, else -1 (ARTIST
    // Array<int,8>::FindFirstInstanceOf @0x823184F8 returns -1 literally; the DWARF spells
    // the return uint32_t with KU_INVALID 0xFFFFFFFF, the same bits).
    //
    //
    //
    //
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    for (u32 luIndex = 0; luIndex < static_cast<u32>(miCount); ++luIndex)
    {
        if (lrElement == maElements[luIndex])
        {
            return static_cast<s32>(luIndex);
        }
    }
    return KI_UNCONSTRUCTED;
}




template <typename T, u32 N>
inline bool
Array<T, N>::Contains(const T& lrElement) const
{
    // Whether lrElement is present (ARTIST Array<u64,80>::Contains @0x82325470): its own
    // constructed assert, then FindFirstInstanceOf with its own.
    //
    //
    //
    //
    //
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    return (FindFirstInstanceOf(lrElement) != KI_UNCONSTRUCTED);
}

template <typename T, u32 N>
inline bool
Array<T, N>::IsFull() const
{
    // ARTIST Array<u64,80>::IsFull @0x823254F0: GetLength's 336 assert, then the compare.
    return (GetLength() == GetCapacity());
}









template <typename T, u32 N>
inline const T&
Array<T, N>::GetItem(u32 luIndex) const
{
    // The checked accessor (const). ARTIST streams "Array index out of bounds. Index: "
    // << luIndex << ", length: " << count into the assert buffer (a plain string here).
    // Out-of-line const copies: Array<int,20>::GetItem const @0x821FF788, @0x8235B918
    // (sizeof T 48), @0x82200140 (116).
    //
    //
    //
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    CGS_ASSERT(luIndex < static_cast<u32>(miCount), "Array index out of bounds");
    return maElements[luIndex];
}


template <typename T, u32 N>
inline T&
Array<T, N>::GetItem(u32 luIndex)
{
    // The checked accessor: the const overload's checks on its own lines (ARTIST
    // Array<int,20>::GetItem @0x821FF680, @0x823AFEC8 (sizeof T 24), @0x82200248 (116),
    // @0x8241D4F8 (44)).
    //
    //
    //
    //
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    CGS_ASSERT(luIndex < static_cast<u32>(miCount), "Array index out of bounds");
    return maElements[luIndex];
}

template <typename T, u32 N>
inline const T&
Array<T, N>::operator[](u32 luIndex) const
{
    return GetItem(luIndex);
}








template <typename T, u32 N>
inline T&
Array<T, N>::operator[](u32 luIndex)
{
    return GetItem(luIndex);
}










template <typename T, u32 N>
inline T*
Array<T, N>::Begin()
{
    // The first element (ARTIST inlines it as the first std::sort argument in
    // CollisionStateManager::UpdateParams @0x82704420).
    //
    //
    //
    //
    //
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    return &maElements[0];
}


template <typename T, u32 N>
inline T*
Array<T, N>::End()
{
    // One past the last live element (ARTIST inlines it as the second std::sort argument
    // in CollisionStateManager::UpdateParams @0x82704420).
    //
    //
    //
    //
    //
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    return &maElements[miCount];
}

template <typename T, u32 N>
inline T*
Array<T, N>::QSort(int (*lpfComparator)(const void* lpA, const void* lpB))
{
    // In-place sort of the live elements through the C library qsort (ARTIST
    // Array<ChainableMultiplierInfo,8>::QSort @0x82317950). ARTIST streams the constructed
    // message; it is a plain string here. The console returns void; the buffer base returned
    // here is what the explicit instantiation in Array_ChainableMultiplierInfo_8.cpp
    // declares.
    //
    //
    //
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    const u32 luCount = static_cast<u32>(miCount);
    if (luCount > 1)
    {
        std::qsort(maElements, luCount, sizeof(T), lpfComparator);
    }
    return maElements;
}

// The Array<int,20> GetItem overloads have independently emitted X360 bodies with streamed
// bounds-assert messages. Their explicit specializations live in CgsArrayInt20.cpp; the generic
// accessors and operator[] remain unchanged for every other instantiation.
// Both int20 operator[] overloads forward to their corresponding GetItem.
template <> s32&       Array<s32, 20>::GetItem(u32 luIndex);
template <> const s32& Array<s32, 20>::GetItem(u32 luIndex) const;
template <> s32&       Array<s32, 20>::operator[](u32 luIndex);
template <> const s32& Array<s32, 20>::operator[](u32 luIndex) const;

} // namespace CgsContainers
