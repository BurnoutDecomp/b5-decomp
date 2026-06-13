#ifndef BRN_POST_FX_SHADER_H
#define BRN_POST_FX_SHADER_H

#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82401538.
// The constructor clears five head elements (stride-6 in the guest, partially
// unrolled by the compiler) and a trailing run of stride-5 elements.
class BrnPostFxShader
{
public:
    BrnPostFxShader();

private:
    struct HeadElem
    {
        u32 mVal[5];
        u32 mPad;
    };

    struct LoopElem
    {
        u32 mVal[5];
    };

    u8       mPad0[720]; // unrecovered members preceding the head elements (guest index 180)
    HeadElem mHeadElems[5];
    LoopElem mLoopElems[2];
};

#endif
