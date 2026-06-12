#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8246CA80
//   (BrnFlapt::MovieClipRef::SetVisible)
//
// Behaviour-faithful to the X360 pseudocode:
//     v2 = *result;                       // the referenced movie-clip instance
//     if ( a2 ) v3 = *(v2 + 54) | 2;      // set visible bit
//     else      v3 = *(v2 + 54) & 0xFD;   // clear visible bit
//     *(v2 + 54) = v3;
//     return result;
//
// The ref holds the clip-instance pointer in its first slot; the visibility flag
// is bit 1 (0x02) of the flags byte at +0x36 of that instance.

namespace BrnFlapt
{
    struct MovieClipRef
    {
        MovieClipRef* SetVisible(bool lbVisible);
    };

    MovieClipRef* MovieClipRef::SetVisible(bool lbVisible)
    {
        u8* lpClip = *reinterpret_cast<u8**>(this);

        u8 luFlags = lpClip[54];
        lpClip[54] = lbVisible ? (luFlags | 0x02u)
                               : (luFlags & 0xFDu);

        return this;
    }
}
