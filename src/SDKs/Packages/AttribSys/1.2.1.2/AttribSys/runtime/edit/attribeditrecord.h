#pragma once

#include "types.hpp"

// ===========================================================================
// Attrib::EditRecord -- one live-link / editor attribute edit.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2). An EditRecord
// is one pending attribute edit that the AttribSys editor/live-link machinery
// threads into an Attrib::EditTable (the sorted EditSpecifier red-black tree in
// the sibling attribedittable TU). The record owns a heap buffer -- allocated
// from the AttribSys package allocator -- holding the edit's payload; kind-7
// (Replace) edits additionally keep an "undo" pointer so the original attribute
// value can be restored before the record is torn down.
//
// Only the destructor is boot-traceable to this TU (reached from
// Attrib::DecodeLiveLinkMessage):
//   Attrib::EditRecord::~EditRecord @ 0x8280DE58
// The remaining members (RevertReplace and the edit/apply surface) live in their
// own editor-only TUs and are declared here just far enough for the destructor to
// compile and link.
//
// Member SHAPE is homed from the X360 asm of the destructor (the only attested
// accessor of this class):
//   +0x00  mnKind        u16   (lhz; compared == 7 to gate RevertReplace)
//   +0x18  mnBufferSize  u32   (lwz; byte count freed + accounted to miFreeTotal)
//   +0x1C  mpBuffer      void* (lwz; the owned edit buffer, freed when non-NULL)
//   +0x20  mpUndoState   void* (lwz; when non-NULL AND kind==7 -> RevertReplace)
// The +0x02..+0x17 gap and any members past +0x20 are not touched by the
// destructor and are left as unrecovered padding (no fabricated fields).
//
// `Attrib` is a vendor (AttribSys) library boundary, so its identifiers are
// preserved per the naming convention.
// ===========================================================================

namespace Attrib
{

class EditRecord
{
public:
    // Edit-kind tag stored at +0x00 (u16). The destructor only distinguishes the
    // Replace kind: when mnKind == KU_EDITKIND_REPLACE and an undo state is live,
    // the destructor reverts the replace before releasing the record. The other
    // kind values are not attested by this TU.
    // (Inferred name: kind 7 is the value RevertReplace handles.)
    static const u16 KU_EDITKIND_REPLACE = 7;

    // X360 0x8280DE58 -- reached from Attrib::DecodeLiveLinkMessage. If this is a
    // live Replace edit (mpUndoState != NULL && mnKind == Replace) restore the
    // original value first, then release the owned edit buffer (mpBuffer, of
    // mnBufferSize bytes) back to the AttribSys package allocator.
    ~EditRecord();

    // Restore the attribute value this Replace edit overwrote. Its own (editor-only)
    // ledger TU; declared-not-defined here (resolved at link, like the sibling
    // EditTable's EditRecordAllocNode / eastl::RBTreeInsert primitives).
    // X360 body lives in the class:Attrib::EditRecord::RevertReplace TU.
    int RevertReplace();

private:
    // Never emitted; pins the X360-attested, pointer-invariant member offsets on
    // the LLP64 gate. Offsets at/after the first pointer (mpUndoState) shift when
    // pointers widen to 8 bytes, so only the pre-pointer facts are asserted.
    static void _AssertLayout();

    u16   mnKind;         // +0x00  edit-kind tag
    u8    maPad02[0x16];  // +0x02..+0x17  (unrecovered)
    u32   mnBufferSize;   // +0x18  owned-buffer byte count
    void* mpBuffer;       // +0x1C  owned edit buffer (freed on destruct)
    void* mpUndoState;    // +0x20  Replace-undo state (non-NULL -> revertable)
    // (further members, if any, unrecovered -- untouched by the destructor)
};

} // namespace Attrib
