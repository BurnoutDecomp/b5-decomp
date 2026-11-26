// ===========================================================================
// EATech Apt -- AptCIHNone out-of-line body. Reconstructed STRICTLY from the
// X360 ARTIST.XEX (AptCIHNone::AptCIHNone @0x82B00DC8); no LegacySrc source / no
// DecFIGS DWARF exists for this X360-only singleton.
//
// X360 @0x82B00DC8 (store-for-store):
//     AptCIH::AptCIH(this, 0, 0);              // base node ctor: null char + null parent
//     this->mnValueData = this->mnValueData & 0xFFFFFF80 | 0x25;
//                                              // insrwi r10,0x25,7,25: meValueType = 37
//     *this = off_82145FF0;                    // (automatic) AptCIHNone vtable store
//     AptValue::setIsDefined(this, 1);         // the "none" handle IS a defined value
//     AptValue::setRefCount(this, 0xFFF);      // pin MAX_REFCOUNT -- the shared singleton
//
// The 0x25 write is meValueType = AptVFT_CIHNone (37): X360 allocates bit-fields from the
// MSB, so the last-declared 7-bit type tag is the word's low 7 bits (getVtblIndex is
// clrlwi 25; the tree's old reading of these as the x64 low flag bits was the LSB-first
// layout). The base AptCIH ctor's AptVFT_CharacterInstHandle tag is replaced.
// ===========================================================================

// XDK size optimization matches ARTIST at 0x82B00E30.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "SDKs/EATech/include/Apt/AptCIHNone.h"

// @0x82B00DC8
AptCIHNone::AptCIHNone()
    : AptCIH(/*pCharacter*/ 0, /*pParent*/ 0)
{
    // ARTIST 0x82B00DF4 `rlwimi r10,0x25,0,25,31`: the low 7 bits of the word are
    // meValueType (X360 bit-fields are allocated from the MSB; getVtblIndex is clrlwi 25),
    // so the derived ctor retags the CIH as AptVFT_CIHNone (37), written directly as the
    // base AptValue constructors do (no GC-flag lock, no other bit).
    mValueBitfield.meValueType = AptVFT_CIHNone;

    setIsDefined(true);              // the AS "none" handle is a defined value
    setRefCount(MAX_REFCOUNT);       // pinned: the shared singleton is never freed
}
