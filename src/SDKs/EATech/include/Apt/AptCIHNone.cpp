// ===========================================================================
// EATech Apt -- AptCIHNone out-of-line body. Reconstructed STRICTLY from the
// X360 ARTIST.XEX (AptCIHNone::AptCIHNone @0x82B00DC8); no Feb-2007 source / no
// DecFIGS DWARF exists for this X360-only singleton.
//
// X360 @0x82B00DC8 (store-for-store):
//     AptCIH::AptCIH(this, 0, 0);              // base node ctor: null char + null parent
//     this->mnValueData = this->mnValueData & 0xFFFFFF80 | 0x25;
//                                              // insrwi r10,0x25,7,25 -> set low 7 flags
//     *this = off_82145FF0;                    // (automatic) AptCIHNone vtable store
//     AptValue::setIsDefined(this, 1);         // the "none" handle IS a defined value
//     AptValue::setRefCount(this, 0xFFF);      // pin MAX_REFCOUNT -- the shared singleton
//
// The 0x25 write replaces the low 7 bits of the AptValue bitfield word with:
//     bit0 mbIsAllocated         = 1
//     bit1 mbHasRegisterReferenceMark = 0
//     bit2 mbIsInDeferredVector  = 1
//     bit3 mbDestroyedGC         = 0
//     bit4 mbIsDefined           = 0   (immediately re-set to 1 by setIsDefined)
//     bit5 mbAllowsDelayedDeletion = 1
//     bit6 (low bit of mnReferenceCount) = 0  (overwritten by setRefCount)
// It does NOT touch the high meValueType field, so the value-type tag the base
// AptCIH ctor stored (AptVFT_CharacterInstHandle) is retained; the AptCIHNone
// vtable pointer is what distinguishes the type. Reconstructed by NAMED bitfield
// fields (no raw bitfield-word poke); the two named setters cover the bits they
// own, and setIsDefined/setRefCount own mbIsDefined / mnReferenceCount.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCIHNone.h"

// @0x82B00DC8
AptCIHNone::AptCIHNone()
    : AptCIH(/*pCharacter*/ nullptr, /*pParent*/ nullptr)
{
    // Low-7 AptValue flag set (the X360 `bitfield = bitfield & ~0x7F | 0x25`),
    // written by named field. mbIsDefined (bit 4) is cleared here and re-set just
    // below; mnReferenceCount's low bit (bit 6) is owned by setRefCount.
    mValueBitfield.mbIsAllocated            = 1u;
    mValueBitfield.mbHasRegisterReferenceMark = 0u;
    mValueBitfield.mbIsInDeferredVector     = 1u;
    mValueBitfield.mbDestroyedGC            = 0u;
    SetAllowDelayedDeletion(true);   // bit 5 mbAllowsDelayedDeletion = 1

    setIsDefined(true);              // the AS "none" handle is a defined value
    setRefCount(MAX_REFCOUNT);       // pinned: the shared singleton is never freed
}
