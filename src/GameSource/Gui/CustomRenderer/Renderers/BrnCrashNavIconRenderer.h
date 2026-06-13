#ifndef BRN_CRASH_NAV_ICON_RENDERER_H
#define BRN_CRASH_NAV_ICON_RENDERER_H

#include "types.hpp"

namespace BrnGui
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//   CrashNavIconRenderer  @ 0x827E0B28
//   SetRenderEnabled      @ 0x827E0CA0
// The guest object is large and only partially recovered; members are declared
// by name (no raw offset casts). Exact byte offsets are not reproduced because
// the gate compiles for a 64-bit host (pointers widen from 4 to 8 bytes).
struct IconRenderState
{
    void* mpVtable; // guest +0   off_820CEB64
    void* mpIface;  // guest +144 off_820CEB40 (last write wins over off_820CE0E8)
    void* mpData;   // guest +196 off_82072F8C
};

class CrashNavIconRenderer
{
public:
    CrashNavIconRenderer();
    CrashNavIconRenderer* SetRenderEnabled(bool lbEnabled);

private:
    bool            mbRenderEnabled;    // guest byte +4
    u32             mGroupA[5];         // guest index 39
    u32             mGroupB[5];         // guest index 45
    u32             mLoopGroups[2][5];  // guest index 51, 56 (stride-5 clear loop)
    u32             mGroupC[5];         // guest index 63
    u32             mGroupD[5];         // guest index 69
    s32             mState81;           // guest index 81 (initialised to -1)
    u64             mAnimField0;        // guest +336
    u64             mAnimField1;        // guest +344
    u32             mField352;          // guest +352
    u32             mField356;          // guest +356
    u32             mMode436;           // guest +436
    u32             mField452;          // guest +452
    u32             mField456;          // guest +456
    IconRenderState mIcons[10];         // guest index 116, stride 124
    u32             mField5444;         // guest +5444
};
}

#endif
