#include "vendor/renderware/collision/Aggregate.hpp"

// ===========================================================================
// rw::collision::Aggregate -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   Aggregate::Fixup  @ 0x82BBBBD8
//
// The X360 body is a single tail-branch (`b <sibling>; .long 0`) with no
// prologue: the function was identical-COMDAT-folded by the linker onto a
// code-identical trivial sibling, so its own machine code is gone and IDA shows
// a branch to the surviving fold target's symbol. The recoverable fact is that
// Aggregate::Fixup is a trivial leaf returning a constant; the substantive
// fix-up work belongs to the derived overrides (ClusteredMesh::Fixup,
// TriangleKDTreeProcedural::Fixup) which call into this base. It is therefore
// reconstructed as the trivial base method. See Aggregate.hpp.
// ===========================================================================

namespace rw
{
namespace collision
{

// Aggregate::Fixup @ 0x82BBBBD8
//
// The X360 body was identical-COMDAT-folded onto a code-identical trivial leaf, so
// its own machine code (and the exact return constant) is GONE from the export --
// IDA shows only a tail-branch to the surviving fold target. The recoverable
// contract is the two call sites: rw::collision::ClusteredMesh::Fixup @0x82BB31B8
// and rw::collision::TriangleKDTreeProcedural::Fixup @0x82BB1168 each do
//   if (Aggregate::Fixup()) { <rebase pointers>; KDTree::Fixup(); ...; return 1; }
//   return <result>;
// i.e. the base MUST return a NONZERO/truthy value for the derived collision
// fix-ups to run at all (a 0 would make them permanent no-ops).
// FLAGGED: the exact constant is NOT asm-recoverable (folded); a truthy placeholder
// is used to honor the call-site contract. This TU is BLOCKED pending identification
// of the fold survivor -- do not treat the value below as an X360-attested fact.
int Aggregate::Fixup()
{
    return 1;   // truthy placeholder: honors the if (Fixup()) call-site contract
}

} // namespace collision
} // namespace rw
