#pragma once

// EA::Allocator::IAllocator and EA::TagValuePair -- the Lion package's tagged
// allocation interface (SDKs/Packages/Lion/Final/Allocator/include/allocator/iallocator.h).
//
// This is the allocator contract the Lion runtime allocates through (cLionBlockAlloc,
// LionSmallAlloc, cLionChunkManager all hold an EA::Allocator::ITaggedAllocator* and call
// Alloc(size, TagValuePair) / Free(ptr, size)). It is EA vendor code: members and methods
// follow EA's CoreAllocator convention (PascalCase, m-prefixed members), not the project
// Brn/Cgs K..._ naming.
//
// Surface recovered from the X360 DWARF
// (references/DecFIGS/dwarfdump/SDKs/Packages/Lion/Final/Allocator/include/allocator/iallocator.h):
//   struct TagValuePair { U32 mTag; union { int/size_t/float/const void* } mValue;
//                         const TagValuePair* mNext; ctors per value type; operator+ chains. }
//   struct IAllocator (abstract): { virtual Alloc(size_t, const TagValuePair&),
//                                   virtual Free(void*, size_t),
//                                   virtual AddRef(), virtual Release(), virtual ~IAllocator() }
//
// The X360 build chains allocation hints by value: each TagValuePair carries a tag id, a
// value, and a pointer to the next pair; operator+ links two pairs into a list which the
// concrete allocator walks for debug-name / flags metadata.

#include "types.hpp"

#include <cstddef>  // size_t

namespace EA
{

// EA::TagValuePair -- a single (tag, value) hint plus a forward link, used to pass a
// variable-length list of allocation hints (debug name, flags, group id, ...) into Alloc.
struct TagValuePair
{
    u32 mTag;

    union
    {
        s32         mInt;
        size_t      mSize;
        f32         mFloat;
        const void* mPointer;
    } mValue;

    const TagValuePair* mNext;

    TagValuePair(u32 auTag, s32 aiValue)
        : mTag(auTag), mNext(nullptr)
    {
        mValue.mInt = aiValue;
    }

    TagValuePair(u32 auTag, size_t auValue)
        : mTag(auTag), mNext(nullptr)
    {
        mValue.mSize = auValue;
    }

    TagValuePair(u32 auTag, f32 afValue)
        : mTag(auTag), mNext(nullptr)
    {
        mValue.mFloat = afValue;
    }

    TagValuePair(u32 auTag, const void* apValue)
        : mTag(auTag), mNext(nullptr)
    {
        mValue.mPointer = apValue;
    }

    // Append arRhs at the END of this chain and return the head (*this). A bare
    // this->mNext=&arRhs drops the middle node on a 3-operand fold: (a+b)+c would
    // re-target a->c, losing b. The X360 builds a 3-node LINE->FILE->NAME chain
    // (head=LINE), so a+b+c must yield a->b->c with a as the head passed to Alloc.
    const TagValuePair& operator+(const TagValuePair& arRhs) const
    {
        TagValuePair* lpTail = const_cast<TagValuePair*>(this);
        while (lpTail->mNext)
            lpTail = const_cast<TagValuePair*>(lpTail->mNext);
        lpTail->mNext = &arRhs;
        return *this;
    }
};

namespace Allocator
{

// EA::Allocator::IAllocator -- abstract tagged allocation interface.
struct IAllocator
{
    virtual void* Alloc(size_t anSize, const TagValuePair& arTags) = 0;
    virtual void  Free(void* apData, size_t anSize) = 0;
    virtual s32   AddRef() = 0;
    virtual s32   Release() = 0;

    // Polymorphic base destructor. Defined OUT-OF-LINE in iallocator.cpp so the
    // interface owns a real .cpp definition home -- the X360 base vector-deleting
    // destructor @ 0x82277E80 (stores the IAllocator vtable, then for the deleting
    // variant conditionally operator-delete's). The body is empty (no members, no
    // base to tear down); the vtable store + delete-tail is what the compiler emits.
    virtual ~IAllocator();
};

}  // namespace Allocator
}  // namespace EA
