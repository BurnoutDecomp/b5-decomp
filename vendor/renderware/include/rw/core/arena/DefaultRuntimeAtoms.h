#ifndef RW_CORE_ARENA_DEFAULTRUNTIMEATOMS_H
#define RW_CORE_ARENA_DEFAULTRUNTIMEATOMS_H

#include <cstdint>

#include "rw/core/atom/FixupAtoms.h"
#include "rw/core/atom/UnfixRefixAtoms.h"

// ===========================================================================
// The arena's default runtime atom callbacks: the FixupAtoms / UnfixRefixAtoms
// implementations that resolve atoms against the process-wide
// rw::core::atom::globalAtomTable. Both live in the arena TU (arena.cpp).
//
//   DefaultRuntimeAtomFixup        : FixupAtoms
//     slot 0  deleting destructor (restores the FixupAtoms vtable, frees)
//     slot 1  At(name)              -> globalAtomTable.Register(name)
//
//   DefaultRuntimeUnfixRefixAtoms  : UnfixRefixAtoms
//     slot 0  deleting destructor (restores the UnfixRefixAtoms vtable, frees)
//     slot 1  At(base, name) const  -> globalAtomTable.At(name)
//     slot 2  At(base, id) const    -> globalAtomTable.At(id)
//
// Neither class has data members: each is a bare vtable pointer. The `base`
// argument of the unfix/refix lookups is not read.
// ===========================================================================

namespace rw
{
namespace core
{
namespace arena
{

class DefaultRuntimeAtomFixup : public atom::FixupAtoms
{
public:
    // Intern lpcName in the global atom table, returning its (possibly new) id.
    uint16_t At(const char* lpcName) override;
};

class DefaultRuntimeUnfixRefixAtoms : public atom::UnfixRefixAtoms
{
protected:
    // The global atom table's string for luAtomId.
    const char* At(const void* lpBase, uint16_t luAtomId) const override;

    // The global atom table's id for lpcName (0xFFFF when it is not interned).
    uint16_t At(const void* lpBase, const char* lpcName) const override;
};

}  // namespace arena
}  // namespace core
}  // namespace rw

#endif  // RW_CORE_ARENA_DEFAULTRUNTIMEATOMS_H
