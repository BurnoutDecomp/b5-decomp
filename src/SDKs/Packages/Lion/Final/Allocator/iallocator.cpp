// ============================================================================
// SDKs/Packages/Lion/Final/Allocator/iallocator.cpp
//
// Out-of-line definition home for the Lion allocator interface
// EA::Allocator::IAllocator (declared in
// SDKs/Packages/Lion/Final/Allocator/include/allocator/iallocator.h).
//
//   EA::Allocator::IAllocator::~IAllocator  -- base deleting destructor
//       reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82277E80
//       (the `vector deleting destructor' thunk).
//
// The X360 thunk @ 0x82277E80 stores the IAllocator vtable (off_8200FDB4) into
// *this (stw r11, 0(r31)), then -- for the deleting variant -- conditionally
// calls operator delete(this) when the low bit of the flags argument is set
// (clrlwi r10, r4, 31; beq skip; bl operator_delete), and returns this in r3.
// IAllocator is an abstract base with no members and no base subobject, so the
// destructor body is empty: defining ~IAllocator() out-of-line emits exactly the
// vtable store + the conditional operator-delete tail the binary performs.
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "SDKs/Packages/Lion/Final/Allocator/include/allocator/iallocator.h"

namespace EA
{
namespace Allocator
{
    // 0x82277E80. Polymorphic base destructor (vtable store + deleting tail).
    IAllocator::~IAllocator()
    {
    }
}  // namespace Allocator
}  // namespace EA
