#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (Insert/Erase/Find/GetItem guards)

// Set<T, N> -- a fixed-capacity unordered set with linear-search membership. Shape
// (member names/types/order, method set + signatures) is authoritative from the DecFIGS
// DWARF (references/DecFIGS/dwarfdump/GameShared/GameClasses/Containers/CgsSet.h):
//
//     T    maElements[N];   // CgsSet.h:147  (+0x00)  inline element buffer
//     u32  muLength;        // CgsSet.h:148  live element count (full when == N)
//
// and the static sentinel:
//
//     static const u32 KU_INVALID = 0xffffffff;   // CgsSet.h:53
//
// The DWARF lists each used instantiation as CgsContainers::Set<T,Nu> -- e.g.
// Set<CgsID,512u>, Set<CgsID,5u>, Set<CgsID,11u>, Set<CgsID,14u>, Set<uint16_t,8u>,
// Set<uint16_t,15u>, Set<uint16_t,72u>, Set<uint16_t,160u>, Set<BrnWorld::PropEntityID,32u>.
// StuntModeScoring embeds one BY VALUE:
//     typedef Set<CgsID,512u> StuntElementSet;                 // BrnStuntModeScoring.h:70
//     StuntModeScoring::StuntElementSet mRecentStuntElementSet;// BrnStuntModeScoring.h:348
//
// Spelled as the unqualified Set<T,N> at global scope to match the committed Array<T,N> /
// FifoQueue<T,N> container convention (the DWARF spells these CgsContainers::Set, but the
// project's committed containers live at global scope, and the embedding typedef itself is
// the unqualified `Set<CgsID,512u>`). The capacity is a u32 non-type parameter to match the
// DWARF spelling `<...,512u>`.
//
// Bodies are the canonical Set<T,N> template (recovered intact from the Feb-2007 reference
// tree, references/Feb-2007/.../Containers/CgsSet.h); the original guarded each public method
// with a streamed `CgsDev::Assert::PrintStringed(...)` + `tw 31,1,1` trap, mapped here to the
// project's CGS_ASSERT with the same message text. muLength == KU_INVALID is the
// "used before Construct/Clear" sentinel; Clear() flips it to 0 (empty-but-usable).
//
// FixUp/FixDown are the big-endian fix-up shims; their bodies fold rw::EndianSwap over the
// live elements + the length word. rw::EndianSwap lands with a later (not-yet-reconstructed)
// TU, so those two are declared-only here -- StuntModeScoring uses only Insert/Contains/Clear
// /Find and iteration (GetLength + operator[]), all of which are fully bodied below.
template <typename T, u32 N>
class Set
{
public:
    // The sentinel returned by Find when an element is absent, and the "unconstructed"
    // marker value for muLength (CgsSet.h:53). Equal across every instantiation.
    static const u32 KU_INVALID = 0xffffffffu;

    // Bring the set to its empty-but-usable state (CgsSet.h:162). Construct just calls Clear.
    void Construct()
    {
        Clear();
    }

    // Insert lElement if not already present (CgsSet.h:177). No-op on a duplicate; asserts the
    // set was Construct/Clear'd and has room before appending.
    void Insert(const T& lElement)
    {
        CGS_ASSERT(muLength != KU_INVALID, "Set used before Construct/Clear was called");

        if (!Contains(lElement))
        {
            CGS_ASSERT(muLength < N, "Set container out of space");

            maElements[muLength] = lElement;
            ++muLength;
        }
    }

    // Remove lElement (CgsSet.h:201). Unordered erase: overwrite the found slot with the last
    // live element, then drop the count. Asserts the element is present.
    void Erase(const T& lElement)
    {
        CGS_ASSERT(muLength != KU_INVALID, "Set used before Construct/Clear was called");

        u32 luIndex = Find(lElement);
        CGS_ASSERT(luIndex != KU_INVALID, "luIndex != KU_INVALID");

        maElements[luIndex] = maElements[muLength - 1];
        --muLength;
    }

    // Live element count (CgsSet.h:225). Asserts the set was Construct/Clear'd.
    u32 GetLength() const
    {
        CGS_ASSERT(muLength != KU_INVALID, "Set used before Construct/Clear was called");
        return muLength;
    }

    // Capacity of the inline buffer (CgsSet.h:240).
    u32 GetCapacity() const
    {
        return N;
    }

    // Checked element accessors (CgsSet.h:255 / :272). Assert constructed + index in capacity.
    const T& GetItem(u32 luIndex) const
    {
        CGS_ASSERT(muLength != KU_INVALID, "Set used before Construct/Clear was called");
        CGS_ASSERT(luIndex < N, "Set index out of bounds");
        return maElements[luIndex];
    }

    T& GetItem(u32 luIndex)
    {
        CGS_ASSERT(muLength != KU_INVALID, "Set used before Construct/Clear was called");
        CGS_ASSERT(luIndex < N, "Set index out of bounds");
        return maElements[luIndex];
    }

    // Empty-but-usable reset (CgsSet.h:288): zero live elements (flips muLength off the
    // KU_INVALID sentinel so the guarded accessors stop firing).
    void Clear()
    {
        muLength = 0;
    }

    // Linear search for lElement (CgsSet.h:303). Returns the index, or KU_INVALID when absent.
    u32 Find(const T& lElement) const
    {
        CGS_ASSERT(muLength != KU_INVALID, "Set used before Construct/Clear was called");

        u32 luElement;
        for (luElement = 0; luElement < muLength; ++luElement)
        {
            if (lElement == maElements[luElement])
            {
                return luElement;
            }
        }

        return KU_INVALID;
    }

    // True when lElement is present (CgsSet.h:330). The double constructed-assert
    // (Contains then Find) is faithful to the original.
    bool Contains(const T& lElement) const
    {
        CGS_ASSERT(muLength != KU_INVALID, "Set used before Construct/Clear was called");
        return (Find(lElement) != KU_INVALID);
    }

    // Insert every element of lOther that is not already present (CgsSet.h, Union). The other
    // set may have a different capacity.
    template <u32 Capacity2>
    void Union(const Set<T, Capacity2>& lOther)
    {
        u32 luElement;
        for (luElement = 0; luElement < lOther.GetLength(); ++luElement)
        {
            Insert(lOther[luElement]);
        }
    }

    // Set this = lA \ lB (CgsSet.h, SetDifference): clear, then keep each element of lA that
    // lB does not contain. lA / lB may have different capacities.
    template <u32 Capacity1, u32 Capacity2>
    void SetDifference(const Set<T, Capacity1>& lA, const Set<T, Capacity2>& lB)
    {
        Clear();

        u32 luElement;
        for (luElement = 0; luElement < lA.GetLength(); ++luElement)
        {
            const T& lElement = lA[luElement];

            if (!lB.Contains(lElement))
            {
                Insert(lElement);
            }
        }
    }

    // Indexing operators forward to GetItem (CgsSet.h:399 / :414).
    const T& operator[](u32 luIndex) const
    {
        return GetItem(luIndex);
    }

    T& operator[](u32 luIndex)
    {
        return GetItem(luIndex);
    }

    // True when the set is at capacity (CgsSet.h:474).
    bool IsFull() const
    {
        CGS_ASSERT(muLength != KU_INVALID, "Set used before Construct/Clear was called");
        return muLength == N;
    }

    // Big-endian fix-up shims (CgsSet.h:428 / :451): byte-swap the length word and every live
    // element for opposite-endian reads. Declared-only -- the bodies fold rw::EndianSwap, which
    // lands with a later (not-yet-reconstructed) TU. StuntModeScoring does not call these.
    void FixUp();
    void FixDown();

private:
    T   maElements[N];   // CgsSet.h:147  (+0x00)  inline element buffer
    u32 muLength;        // CgsSet.h:148  live element count; KU_INVALID until Construct/Clear
};
