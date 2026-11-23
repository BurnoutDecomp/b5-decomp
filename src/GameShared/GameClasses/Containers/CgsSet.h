#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (Insert/Erase/Find/GetItem guards)

// CgsContainers::Set<T, N> -- a fixed-capacity unordered set with linear-search membership.
// Namespace, member set, names and lines from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameShared/GameClasses/Containers/CgsSet.h: namespace h:28,
// class h:50, KU_INVALID h:53, maElements h:147, muLength h:148); the shape of the header (the
// class declaring its members, the inline definitions after it) and the bodies from the
// LegacySrc copy of the same header; ARTIST confirms every body and line.
//
// SOURCE LINES: ARTIST bakes this header's line numbers into every inlined assert (li r5,
// <line> before CgsDev::Assert::FireAssert, file string
// "d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\containers\CgsSet.h" @0x820185C8).
// Each definition's declarator sits on its DWARF line and its asserts follow as in the
// original: Insert h:177 (179/183), Erase h:201 (203/206), GetLength h:225 (227),
// GetItem const h:255 (257/258), GetItem h:272 (274/275), Find h:303 (305), Contains h:330
// (332), e.g. Set<u16,15>::Insert @0x822CA500, Set<u16,15>::Erase @0x822CA5A8,
// Set<PropEntityID,32>::IsFull @0x822CA320 (GetLength's 227), Set<u16,15>::SetDifference<15,15>
// @0x822E5440 (227, 257/258, 332).
//
// FixUp/FixDown (h:428 / h:451, asserts 430 / 453 in BrnTraffic::Pvs::FixUp @0x827623E8) byte-
// swap the live elements and the length word with rw::EndianSwap, which lands with a later
// TU, so they are declared only. Union is not in the DWARF (no instantiation uses it); it
// follows the DWARF members so that they keep their lines.
//
namespace CgsContainers
{



















template <typename T, u32 N>
class Set
{
public:
    static const u32 KU_INVALID = 0xffffffffu;

    // Construct h:162: empty but usable (Clear).
    inline void Construct();

    // Insert h:177: add lElement unless it is already present.
    inline void Insert(const T& lElement);

    // Erase h:201: unordered erase of lElement, which must be present.
    inline void Erase(const T& lElement);

    // GetLength h:225 / GetCapacity h:240: the checked live count and the capacity.
    inline u32 GetLength() const;
    inline u32 GetCapacity() const;

    // GetItem h:255 (const) / h:272: the checked accessors.
    inline const T& GetItem(u32 luIndex) const;
    inline T& GetItem(u32 luIndex);

    // Clear h:288.
    inline void Clear();

    // Find h:303: the element's index, else KU_INVALID. Contains h:330.
    inline u32 Find(const T& lElement) const;
    inline bool Contains(const T& lElement) const;

    // Union (not in the DWARF): insert every element of lOther.
    template <u32 Capacity2>
    inline void Union(const Set<T, Capacity2>& lOther);

    // SetDifference h:372: this = lA \ lB.
    template <u32 Capacity1, u32 Capacity2>
    inline void SetDifference(const Set<T, Capacity1>& lA, const Set<T, Capacity2>& lB);

    // operator[] h:399 (const) / h:414: forward to GetItem.
    inline const T& operator[](u32 luIndex) const;
    inline T& operator[](u32 luIndex);

    // FixUp h:428 / FixDown h:451: the big-endian fix-up shims (declared only, see above).
    void FixUp();
    void FixDown();

    // IsFull h:474: GetLength() == GetCapacity().
    inline bool IsFull() const;

















































private:
    T   maElements[N];   // DWARF h:147  (+0x00)  inline element buffer
    u32 muLength;        // DWARF h:148  live element count; KU_INVALID until Construct/Clear
};










template <typename T, u32 N>
inline void
Set<T, N>::Construct()
{
    Clear();
}









template <typename T, u32 N>
inline void
Set<T, N>::Insert(const T& lElement)
{
    CGS_ASSERT(muLength != KU_INVALID, "Set used before Construct/Clear was called");

    if (!Contains(lElement))
    {
        CGS_ASSERT(muLength < N, "Set container out of space");

        maElements[muLength] = lElement;
        ++muLength;
    }
}










template <typename T, u32 N>
inline void
Set<T, N>::Erase(const T& lElement)
{
    CGS_ASSERT(muLength != KU_INVALID, "Set used before Construct/Clear was called");

    u32 luIndex = Find(lElement);
    CGS_ASSERT(luIndex != KU_INVALID, "luIndex != KU_INVALID");

    // Unordered: the last live element fills the erased slot.
    maElements[luIndex] = maElements[muLength - 1];

    --muLength;
}










template <typename T, u32 N>
inline u32
Set<T, N>::GetLength() const
{
    CGS_ASSERT(muLength != KU_INVALID, "Set used before Construct/Clear was called");
    return muLength;
}








template <typename T, u32 N>
inline u32
Set<T, N>::GetCapacity() const
{
    return N;
}









template <typename T, u32 N>
inline const T&
Set<T, N>::GetItem(u32 luIndex) const
{
    CGS_ASSERT(muLength != KU_INVALID, "Set used before Construct/Clear was called");
    CGS_ASSERT(luIndex < N, "Set index out of bounds");
    return maElements[luIndex];
}









template <typename T, u32 N>
inline T&
Set<T, N>::GetItem(u32 luIndex)
{
    CGS_ASSERT(muLength != KU_INVALID, "Set used before Construct/Clear was called");
    CGS_ASSERT(luIndex < N, "Set index out of bounds");
    return maElements[luIndex];
}








template <typename T, u32 N>
inline void
Set<T, N>::Clear()
{
    muLength = 0;
}









template <typename T, u32 N>
inline u32
Set<T, N>::Find(const T& lElement) const
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










template <typename T, u32 N>
inline bool
Set<T, N>::Contains(const T& lElement) const
{
    CGS_ASSERT(muLength != KU_INVALID, "Set used before Construct/Clear was called");
    return (Find(lElement) != KU_INVALID);
}


































template <typename T, u32 N>
template <u32 Capacity1, u32 Capacity2>
inline void
Set<T, N>::SetDifference(const Set<T, Capacity1>& lA, const Set<T, Capacity2>& lB)
{

    Clear();

    u32 luElement;    // DWARF h:377
    for (luElement = 0; luElement < lA.GetLength(); ++luElement)
    {
        const T& lElement = lA[luElement];    // DWARF h:380

        if (!lB.Contains(lElement))
        {
            Insert(lElement);
        }
    }
}









template <typename T, u32 N>
inline const T&
Set<T, N>::operator[](u32 luIndex) const
{
    return GetItem(luIndex);
}









template <typename T, u32 N>
inline T&
Set<T, N>::operator[](u32 luIndex)
{
    return GetItem(luIndex);
}






















































template <typename T, u32 N>
inline bool
Set<T, N>::IsFull() const
{
    // ARTIST Set<PropEntityID,32>::IsFull @0x822CA320: GetLength's 227 assert, then the compare.
    return (GetLength() == GetCapacity());
}

template <typename T, u32 N>
template <u32 Capacity2>
inline void
Set<T, N>::Union(const Set<T, Capacity2>& lOther)
{
    u32 luElement;
    for (luElement = 0; luElement < lOther.GetLength(); ++luElement)
    {
        Insert(lOther[luElement]);
    }
}

} // namespace CgsContainers
