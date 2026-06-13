#include "BrnPostFxShader.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82401538
//   BrnPostFxShader::BrnPostFxShader
//
// Clears five head elements (the compiler partially unrolled the clear) and a
// trailing run of stride-5 elements cleared by a loop. The re-rolled form is
// shown here.

BrnPostFxShader::BrnPostFxShader()
{
    for (int i = 0; i < 5; ++i)
    {
        mHeadElems[i].mVal[0] = 0;
        mHeadElems[i].mVal[1] = 0;
        mHeadElems[i].mVal[2] = 0;
        mHeadElems[i].mVal[3] = 0;
        mHeadElems[i].mVal[4] = 0;
    }

    for (int i = 0; i < 2; ++i)
    {
        mLoopElems[i].mVal[0] = 0;
        mLoopElems[i].mVal[1] = 0;
        mLoopElems[i].mVal[2] = 0;
        mLoopElems[i].mVal[3] = 0;
        mLoopElems[i].mVal[4] = 0;
    }
}
