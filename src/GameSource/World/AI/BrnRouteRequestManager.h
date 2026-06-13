#ifndef BRN_ROUTE_REQUEST_MANAGER_H
#define BRN_ROUTE_REQUEST_MANAGER_H

#include "types.hpp"

namespace BrnAI
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8278A3B0.
// Construct() seeds a file-scope weight table and clears the head word of each
// request slot. Slot stride is 9 words (36 bytes) as recovered from the loop.
class RouteRequestManager
{
public:
    struct RequestSlot
    {
        u32 mWords[9];
    };

    RouteRequestManager* Construct();

private:
    u8          mPad0[32];        // members preceding the request array (unrecovered)
    RequestSlot mRequestSlots[16]; // guest offset 32, stride 36
};
}

#endif
