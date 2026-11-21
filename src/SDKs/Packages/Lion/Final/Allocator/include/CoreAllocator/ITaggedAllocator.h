#pragma once

#include "coreallocator/icoreallocator_interface.h"
#include "SDKs/Packages/Lion/Final/Allocator/include/allocator/iallocator.h"

namespace EA
{
namespace Allocator
{

// ARTIST's Lion allocator calls tag-based Alloc through the secondary IAllocator
// at this+4 (0x8290A91C). Free uses slot +0xC of the primary ICoreAllocator
// (0x829082D4).
struct ITaggedAllocator : public ICoreAllocator, public IAllocator
{
    using ICoreAllocator::Alloc;
    using ICoreAllocator::Free;
    using IAllocator::Alloc;

    virtual ~ITaggedAllocator() {}
};

}  // namespace Allocator
}  // namespace EA
