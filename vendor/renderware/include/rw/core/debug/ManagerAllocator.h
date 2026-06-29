#ifndef RW_CORE_DEBUG_MANAGERALLOCATOR_H
#define RW_CORE_DEBUG_MANAGERALLOCATOR_H

// Internal linkage helper shared between the rw::core::debug Manager and IFormatter
// translation units: the process-wide resource allocator the debug subsystem carves its
// objects through.
//
// X360 ground truth: rw::core::debug::Manager::CreateInstance @0x82BBD380 stores the
// incoming rw::IResourceAllocator* into the global off_8327F044; both
// rw::core::debug::IFormatter::operator new @0x82BBCBF0 and the IFormatter scalar deleting
// destructor @0x82BBCB50 read that same global to Alloc / Free their storage. Modelled as
// a single shared accessor rather than a raw extern so neither TU pokes the slot directly.

#include "rw/rwcore_structs.h"  // rw::IResourceAllocator

namespace rw { namespace core { namespace debug { namespace detail {

    // The subsystem allocator slot (X360 off_8327F044). Set by Manager::CreateInstance,
    // read by IFormatter::operator new / operator delete.
    ::rw::IResourceAllocator*& ManagerAllocatorSlot();

} } } }  // namespace rw::core::debug::detail

#endif  // RW_CORE_DEBUG_MANAGERALLOCATOR_H
