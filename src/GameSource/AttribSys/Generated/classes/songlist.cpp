// Attrib::Gen::songlist -- generated AttribSys class, out-of-line function bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::songlist::Songs     @ 0x82683998
//   Attrib::Gen::songlist::songlist  @ 0x82697978  (ctor)
// See songlist.h for the class shape. Both are real out-of-line X360 functions; they
// live here (not inline in the header) so this TU has a translation unit and mirrors
// the committed generated-class convention (a per-class .cpp for the out-of-line bodies).

#include "GameSource/AttribSys/Generated/classes/songlist.h"

namespace Attrib
{
namespace Gen
{

// ctor @ 0x82697978 -- chain the Attrib::Instance base ctor (X360 sub_8280A248 @0x82697988,
// r3/r4/r5 = this/lpCollection/lpOwner), assert the collection's class is ClassName::songlist
// (0x7C94BB46 == 2090122054; skip the assert when the class is unset/0), then give the instance
// a default data area (0x970 bytes) if it has none. Mirrors the committed song::song.
songlist::songlist(Collection* lpCollection, void* lpOwner)
    : Instance(lpCollection, lpOwner)
{
    static const int KI_SONGLIST_CLASS = 2090122054; // Attrib::ClassName::songlist (0x7C94BB46)
    if (GetClass() != KI_SONGLIST_CLASS && GetClass() != 0)
        AssertOnClassCheck(GetClass(), KI_SONGLIST_CLASS, GetCollection());
    if (!mpAttributeData)
        mpAttributeData = DefaultDataArea(0x970u);
}

// Songs @ 0x82683998 -- read mpAttributeData (Instance+4 -> r30), get the array length via
// Attrib::Private::GetLength(mpAttributeData); if luIndex (r4, unsigned) >= length (unsigned
// compare `cmplw`), fall back to the shared zero-initialised default block sized for one Song
// record (0x18 bytes). Otherwise index into the array: element stride 0x18 (24) bytes
// (slwi r11,r31,1 -> idx*2; add -> idx*3; slwi r11,r11,3 -> idx*24) and the array starts at +8
// within the attribute-data block (add r11,r11,r30 -> +mpAttributeData; addi r3,r11,8 -> +8).
void* songlist::Songs(unsigned int luIndex) const
{
    u8* lpData = static_cast<u8*>(GetLayoutPointer()); // Instance+4 == mpAttributeData
    if (luIndex >= reinterpret_cast<const Private*>(lpData)->GetLength())
        return DefaultDataArea(0x18u);
    return lpData + 8 + 24u * luIndex;
}

}
}
