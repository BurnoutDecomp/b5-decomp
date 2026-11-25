// Attrib::Gen::songlist -- generated AttribSys class, out-of-line function bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::songlist::Songs     @ 0x82683998
//   Attrib::Gen::songlist::songlist  @ 0x82697978  (ctor)
// See songlist.h for the class shape. Both are real out-of-line X360 functions; they
// live here (not inline in the header) so this TU has a translation unit and mirrors
// the committed generated-class convention (a per-class .cpp for the out-of-line bodies).

#include "GameSource/AttribSys/Generated/classes/songlist.h"
#include <cstring>

namespace Attrib
{
namespace Gen
{

// ARTIST 0x82697988 invokes the RefSpec base overload before the class and layout checks.
songlist::songlist(const RefSpec& lrRefSpec, void* lpOwner)
    : Instance(lrRefSpec, lpOwner)
{
    static const u64 KU_SONGLIST_CLASS = 0x8E28B14A7C94BB46ULL; // ARTIST 0x826979A4
    if (GetClass() != KU_SONGLIST_CLASS && GetClass() != 0)
        AssertOnClassCheck(GetClass(), KU_SONGLIST_CLASS, GetCollection());
    SetDefaultLayout(0x970u);
}

// Songs @ 0x82683998 -- read mpAttributeData (Instance+4 -> r30), get the array length via
// Attrib::Private::GetLength(mpAttributeData); if luIndex (r4, unsigned) >= length (unsigned
// compare `cmplw`), fall back to the shared zero-initialised default block sized for one Song
// record (0x18 bytes). Otherwise index into the array: element stride 0x18 (24) bytes
// (slwi r11,r31,1 -> idx*2; add -> idx*3; slwi r11,r11,3 -> idx*24) and the array starts at +8
// within the attribute-data block (add r11,r11,r30 -> +mpAttributeData; addi r3,r11,8 -> +8).
const RefSpec& songlist::Songs(u32 luIndex) const
{
    const u8* lpData = static_cast<const u8*>(GetLayoutPointer());
    if (luIndex < reinterpret_cast<const Private*>(lpData)->GetLength())
    {
        return reinterpret_cast<const RefSpec*>(lpData + 8)[luIndex];
    }
    return *static_cast<const RefSpec*>(DefaultDataArea(sizeof(RefSpec)));
}

// Num_Songs -- X360 MusicEffect::UpdateParams @0x826FEBBC / @0x826FEC60:
//     lwz r11, var_14C(r1)   ; the songlist Instance's mpAttributeData
//     lwz r4,  0x968(r11)    ; -> EaTraxData::SelectSong's aiNumSongs
s32 songlist::Num_Songs() const
{
    const u8* lpData = static_cast<const u8*>(GetLayoutPointer());
    if (!lpData)
        return 0;
    s32 liCount = 0;
    std::memcpy(&liCount, lpData + 0x968, sizeof(liCount));
    return liCount;
}

}
}

