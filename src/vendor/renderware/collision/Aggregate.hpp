#pragma once

#include "types.hpp"

// ===========================================================================
// rw::collision::Aggregate -- the RenderWare collision-aggregate base. Concrete
// collision procedurals (ClusteredMesh, TriangleKDTreeProcedural, ...) derive
// from Aggregate and override its virtuals; Aggregate supplies the trivial base
// behaviour the derived classes fall back to.
//
// OWNING HOME for the single Aggregate function the X360 binary defines:
//     rw::collision::Aggregate::Fixup  @ 0x82BBBBD8
//         (called by rw::collision::TriangleKDTreeProcedural::Fixup and
//          rw::collision::ClusteredMesh::Fixup -- the derived fixups chain into
//          the base Fixup.)
//
// The X360 @ 0x82BBBBD8 is a single tail-branch with no prologue:
//     b      <trivial-sibling>
//     .long 0
// i.e. the function's machine code was IDENTICAL-COMDAT-FOLDED by the linker
// onto a code-identical trivial sibling (IDA renders the surviving symbol name
// at the fold target). The only recoverable fact is that Aggregate::Fixup is a
// trivial leaf that returns a constant -- it carries no aggregate-specific work
// (all real fix-up lives in the derived ClusteredMesh / KDTree overrides). It is
// reconstructed as that trivial base method; the fold target's unrelated symbol
// name is NOT attributed here (doing so would fabricate semantics). FLAGGED:
// the literal return value is inferred from the trivial-fold shape (success/0).
// ===========================================================================

namespace rw
{
namespace collision
{

class Aggregate
{
public:
    // @ 0x82BBBBD8 -- base aggregate fix-up. Trivial in the base; derived
    // procedurals (ClusteredMesh, TriangleKDTreeProcedural) override it.
    int Fixup();
};

} // namespace collision
} // namespace rw
