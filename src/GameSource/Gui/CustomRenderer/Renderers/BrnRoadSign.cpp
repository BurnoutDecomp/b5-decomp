// ===================================================================================
// BrnGui::RoadSign  -- implementation
//   class:BrnGui::RoadSign
//
// Two 104-byte (0x68) field-by-field copies, recovered store-for-store from the X360
// ARTIST build. Member-by-name; both reproduce a complete copy of the record.
// ===================================================================================
#include "GameSource/Gui/CustomRenderer/Renderers/BrnRoadSign.h"

namespace BrnGui
{
    // @ 0x8244BAD8 -- copy ctor. Copies +0x00..+0x4F as 20 floats (lfs/stfs), then
    // +0x50/+0x54/+0x58/+0x5C/+0x60/+0x64 as words (lwz/stw). (The trailing .long is
    // alignment padding, not a return.)
    RoadSign::RoadSign(const RoadSign& lrOther)
    {
        for (int li = 0; li < 20; ++li)
            mafField00[li] = lrOther.mafField00[li];
        muField50 = lrOther.muField50;
        mfField54 = lrOther.mfField54;
        mfField58 = lrOther.mfField58;
        muField5C = lrOther.muField5C;
        muField60 = lrOther.muField60;
        muField64 = lrOther.muField64;
    }

    // @ 0x8244BBB8 -- copy assignment. Same fields; +0x54/+0x58 move via float loads
    // (lfs/stfs), the rest as words. (Hex-Rays' v2 temp at +0x5C is the compiler
    // scheduling a word load ahead of the +0x54 float store -- no semantic effect.)
    RoadSign& RoadSign::operator=(const RoadSign& lrOther)
    {
        for (int li = 0; li < 20; ++li)
            mafField00[li] = lrOther.mafField00[li];
        muField50 = lrOther.muField50;
        mfField54 = lrOther.mfField54;
        mfField58 = lrOther.mfField58;
        muField5C = lrOther.muField5C;
        muField60 = lrOther.muField60;
        muField64 = lrOther.muField64;
        return *this;
    }
}
