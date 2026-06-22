#pragma once

// EA::Allocator::ITaggedAllocator -- the concrete tagged-allocator interface the Lion
// runtime allocates through
// (SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h).
//
// EA vendor code. Surface recovered from the X360 DWARF
// (references/DecFIGS/dwarfdump/SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h):
//   struct ITaggedAllocator : public IAllocator {
//       virtual void* Alloc(size_t, const char*, unsigned int);
//       virtual void* Alloc(size_t, const char*, unsigned int, unsigned int, unsigned int);
//       virtual void* Alloc(size_t, const TagValuePair&);
//       virtual void  Free(void*, size_t);
//       virtual ~ITaggedAllocator();
//   }
//
// The Lion allocators (cLionBlockAlloc::Init, LionSmallAlloc::PageCreate) call the
// Alloc(size_t, const TagValuePair&) form with a chained TagValuePair list and Free(ptr, 0).

#include "SDKs/Packages/Lion/Final/Allocator/include/allocator/iallocator.h"

#include <cstddef>  // size_t

namespace EA
{
namespace Allocator
{

struct ITaggedAllocator : public IAllocator
{
    // Tagged Alloc form actually used by the Lion runtime: a chained TagValuePair list
    // carries the debug name / flags. Inherited from IAllocator; re-declared here to match
    // the DWARF vtable surface.
    virtual void* Alloc(size_t anSize, const TagValuePair& arTags) = 0;

    // Convenience overloads present on the X360 interface.
    virtual void* Alloc(size_t anSize, const char* apName, u32 auFlags) = 0;
    virtual void* Alloc(size_t anSize, const char* apName, u32 auFlags,
                        u32 auAlign, u32 auAlignOffset) = 0;

    virtual void  Free(void* apData, size_t anSize) = 0;

    virtual ~ITaggedAllocator() {}
};

}  // namespace Allocator
}  // namespace EA
