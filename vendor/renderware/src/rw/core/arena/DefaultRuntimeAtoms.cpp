#include "rw/core/arena/DefaultRuntimeAtoms.h"

#include "rw/core/atom/StaticAtomTable.h"  // rw::core::atom::globalAtomTable

// ===========================================================================
// rw::core::arena default runtime atom callbacks, reconstructed from the
// console. Each lookup is a single forward into rw::core::atom::globalAtomTable;
// see DefaultRuntimeAtoms.h for the vtable map.
// ===========================================================================

namespace rw
{
namespace core
{
namespace arena
{

// ---------------------------------------------------------------------------
// DefaultRuntimeAtomFixup::At
//
// A tail call into globalAtomTable.Register(const char*) that keeps the name
// argument in place: intern the name under the next free id (or return the id
// it already has).
// ---------------------------------------------------------------------------
uint16_t DefaultRuntimeAtomFixup::At(const char* lpcName)
{
    return atom::globalAtomTable.Register(lpcName);
}

// ---------------------------------------------------------------------------
// DefaultRuntimeUnfixRefixAtoms::At (id -> name)
//
//   data   = globalAtomTable.mpData
//   return data->mpStringPoolBase + data->mpIdToOffset[id & 0xFFFF]
//
// StaticAtomTable::At(AtomID) inlined; lpBase is ignored.
// ---------------------------------------------------------------------------
const char* DefaultRuntimeUnfixRefixAtoms::At(const void* /*lpBase*/, uint16_t luAtomId) const
{
    return atom::globalAtomTable.At(luAtomId);
}

// ---------------------------------------------------------------------------
// DefaultRuntimeUnfixRefixAtoms::At (name -> id)
//
// A tail call into globalAtomTable.At(const char*): lpBase is dropped and the
// name moves into the key argument.
// ---------------------------------------------------------------------------
uint16_t DefaultRuntimeUnfixRefixAtoms::At(const void* /*lpBase*/, const char* lpcName) const
{
    return atom::globalAtomTable.At(lpcName);
}

}  // namespace arena
}  // namespace core
}  // namespace rw
