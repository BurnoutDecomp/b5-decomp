/////////////////////////////////////////////////////////////////////////////
// Copyright (c) Electronic Arts Inc. All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
// Out-of-line definition home for EA::Allocator::ICoreAllocator (declared in
// coreallocator/icoreallocator_interface.h).
//
//   EA::Allocator::ICoreAllocator::~ICoreAllocator  -- base deleting destructor
//       reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82276820
//       (the `scalar deleting destructor' thunk).
//
// The X360 thunk @ 0x82276820 stores the ICoreAllocator vtable (off_8200F5B4)
// into *this (stw r11, 0(r31)), then -- for the deleting variant -- conditionally
// calls operator delete(this) when the low bit of the flags argument is set
// (clrlwi r10, r4, 31; cmplwi; beq skip; bl operator_delete), and returns this in
// r3. ICoreAllocator is an abstract base with no members and no base subobject, so
// the destructor body is empty: defining ~ICoreAllocator() out-of-line emits
// exactly the vtable store + the conditional operator-delete tail.
//
// Cited by X360 address only -- no leaked-source provenance.

#include "coreallocator/icoreallocator_interface.h"

namespace EA
{
namespace Allocator
{
    // 0x82276820. Polymorphic base destructor (vtable store + deleting tail).
    ICoreAllocator::~ICoreAllocator()
    {
    }
}  // namespace Allocator
}  // namespace EA
