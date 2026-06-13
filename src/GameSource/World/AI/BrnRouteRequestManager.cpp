#include "BrnRouteRequestManager.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8278A3B0
//   BrnAI::RouteRequestManager::Construct
//
// Seeds the module-global weight table (recovered as IEEE-754 single bit
// patterns from the data segment at 0x8300D570) and zeroes the head word of
// each request slot. The guest writes the slots in a stride-9 loop, then
// clears one extra word in the final slot.

namespace BrnAI
{
// Module-global weight/bound table written during construction (guest 0x8300D570..).
float gRouteRequestWeights[8] = {
    1.0f,         // 0x3F800000
    1.78315496f,  // 0x3FDC4FAC
    1.19288969f,  // 0x3F98B2DC
    1.70421982f,  // 0x3FDA2520
    1.76656675f,  // 0x3FE21A5C
    1.73375201f,  // 0x3FDDEAD6
    1.22346330f,  // 0x3F9C9FB2
    1.14250064f,  // 0x3F924336
};
float gRouteRequestBoundLo = 9.0190702e-11f; // 0x2EC3A75A
float gRouteRequestBoundHi = 0.0f;
float gRouteRequestPad     = 0.0f;

RouteRequestManager* RouteRequestManager::Construct()
{
    gRouteRequestWeights[0] = 1.0f;
    gRouteRequestWeights[1] = 1.78315496f;
    gRouteRequestWeights[2] = 1.19288969f;
    gRouteRequestWeights[3] = 1.70421982f;
    gRouteRequestWeights[4] = 1.76656675f;
    gRouteRequestWeights[5] = 1.73375201f;
    gRouteRequestWeights[6] = 1.22346330f;
    gRouteRequestWeights[7] = 1.14250064f;
    gRouteRequestBoundLo    = 9.0190702e-11f;
    gRouteRequestBoundHi    = 0.0f;
    gRouteRequestPad        = 0.0f;

    for (int i = 0; i < 16; ++i)
    {
        mRequestSlots[i].mWords[0] = 0;
    }
    mRequestSlots[15].mWords[1] = 0;

    return this;
}
}
