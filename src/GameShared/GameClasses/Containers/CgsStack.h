#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#line 6 "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsStack.h"
// CgsContainers::Stack<Type, Length> -- a fixed-capacity LIFO. Member set, names and lines
// from the DecFIGS DWARF (references/DecFIGS/dwarfdump/GameShared/GameClasses/Containers/
// CgsStack.h: namespace h:24, class h:33, maData h:81, miLength h:82); the header's shape (the
// class declaring its members, the inline definitions after it) and the bodies from the
// LegacySrc copy; ARTIST confirms every body and line. miLength holds 0x7FFFFFFF until
// Construct/Clear ("Stack used before Construct/Clear was called").
//
// SOURCE LINES: ARTIST bakes this header's assert lines into every inlined copy (file string
// @0x82010F14): Push 98/99, Grow 110/111, Pop 121/122, Contains 132, Peek 149/150,
// GetLength 160, IsFull 169, IsEmpty 177, operator[] const 201-203, operator[] 213-215. Push
// and Grow assert !IsFull() and Pop/Peek !IsEmpty(), so IsFull's 169 / IsEmpty's 177 follow
// them inline (e.g. 0x82286280: 98, 99, 169).
//
//
//
//
//
//
namespace CgsContainers
{




const s32 KI_STACK_UNCONSTRUCTED = 0x7FFFFFFF;   // miLength before Construct/Clear

template <class Type, s32 Length>
class Stack
{
public:
    // h:36 (LegacySrc: `Stack() { miLength=0x7FFFFFFF; }`) is not restored: the owners in this
    // tree construct their stacks explicitly and rely on the type staying an aggregate.

    // Construct h:89 / Clear h:191: empty but usable.
    inline void Construct();
    inline void Clear();

    // Push h:96 / Grow h:108 / Pop h:119: the checked stack operations.
    inline void Push(const Type& lrEntry);
    inline Type* Grow();
    inline void Pop();

    // Contains h:130 / Peek h:147.
    inline bool Contains(const Type& lrEntry) const;
    inline const Type& Peek() const;

    // GetLength h:158, IsFull h:167, IsEmpty h:175, GetMaxLength h:184.
    inline s32 GetLength() const;
    inline bool IsFull() const;
    inline bool IsEmpty() const;
    inline s32 GetMaxLength() const;

    // operator[] h:199 (const) / h:211: the checked accessors.
    inline const Type& operator[](s32 liIndex) const;
    inline Type& operator[](s32 liIndex);

    // (DWARF: private from here; the tree's owners read the members by name.)


















    Type maData[Length];    // DWARF h:81
    s32  miLength;          // DWARF h:82
};



template <class Type, s32 Length>
inline void
Stack<Type, Length>::Construct()
{
    Clear();
}

template <class Type, s32 Length>
inline void
Stack<Type, Length>::Push(const Type& lrEntry)
{
    CGS_ASSERT(miLength != KI_STACK_UNCONSTRUCTED, "Stack used before Construct/Clear was called");
    CGS_ASSERT(!IsFull(), "!IsFull()");

    maData[miLength] = lrEntry;
    ++miLength;
}


template <class Type, s32 Length>
inline Type*
Stack<Type, Length>::Grow()
{
    CGS_ASSERT(miLength != KI_STACK_UNCONSTRUCTED, "Stack used before Construct/Clear was called");
    CGS_ASSERT(!IsFull(), "!IsFull()");

    Type* lpEntry = &maData[miLength];
    ++miLength;
    return lpEntry;
}
template <class Type, s32 Length>
inline void
Stack<Type, Length>::Pop()
{
    CGS_ASSERT(miLength != KI_STACK_UNCONSTRUCTED, "Stack used before Construct/Clear was called");
    CGS_ASSERT(!IsEmpty(), "!IsEmpty()");

    --miLength;
}


template <class Type, s32 Length>
inline bool
Stack<Type, Length>::Contains(const Type& lrEntry) const
{
    CGS_ASSERT(miLength != KI_STACK_UNCONSTRUCTED, "Stack used before Construct/Clear was called");
    // ARTIST Stack<u16,400>::Contains @0x8270A830: a linear scan of the live entries.
    for (s32 liIndex = 0; liIndex < miLength; ++liIndex)
    {
        if (maData[liIndex] == lrEntry)
        {
            return true;
        }
    }
    return false;
}


template <class Type, s32 Length>
inline const Type&
Stack<Type, Length>::Peek() const
{
    CGS_ASSERT(miLength != KI_STACK_UNCONSTRUCTED, "Stack used before Construct/Clear was called");
    CGS_ASSERT(!IsEmpty(), "!IsEmpty()");

    return maData[miLength - 1];
}


template <class Type, s32 Length>
inline s32
Stack<Type, Length>::GetLength() const
{
    CGS_ASSERT(miLength != KI_STACK_UNCONSTRUCTED, "Stack used before Construct/Clear was called");
    return miLength;
}


template <class Type, s32 Length>
inline bool
Stack<Type, Length>::IsFull() const
{
    CGS_ASSERT(miLength != KI_STACK_UNCONSTRUCTED, "Stack used before Construct/Clear was called");
    return (miLength == Length);
}

template <class Type, s32 Length>
inline bool
Stack<Type, Length>::IsEmpty() const
{
    CGS_ASSERT(miLength != KI_STACK_UNCONSTRUCTED, "Stack used before Construct/Clear was called");
    return (miLength == 0);
}


template <class Type, s32 Length>
inline s32
Stack<Type, Length>::GetMaxLength() const
{
    return Length;
}

template <class Type, s32 Length>
inline void
Stack<Type, Length>::Clear()
{
    miLength = 0;
}


template <class Type, s32 Length>
inline const Type&
Stack<Type, Length>::operator[](s32 liIndex) const
{
    CGS_ASSERT(miLength != KI_STACK_UNCONSTRUCTED, "Stack used before Construct/Clear was called");
    CGS_ASSERT(liIndex >= 0, "Stack access overflow");
    CGS_ASSERT(liIndex < miLength, "Stack access overflow");

    return maData[liIndex];
}


template <class Type, s32 Length>
inline Type&
Stack<Type, Length>::operator[](s32 liIndex)
{
    CGS_ASSERT(miLength != KI_STACK_UNCONSTRUCTED, "Stack used before Construct/Clear was called");
    CGS_ASSERT(liIndex >= 0, "Stack access overflow");
    CGS_ASSERT(liIndex < miLength, "Stack access overflow");

    return maData[liIndex];
}

} // namespace CgsContainers
