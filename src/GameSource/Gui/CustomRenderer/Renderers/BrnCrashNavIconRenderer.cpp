#include "BrnCrashNavIconRenderer.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::CrashNavIconRenderer::CrashNavIconRenderer @ 0x827E0B28
//   BrnGui::CrashNavIconRenderer::SetRenderEnabled     @ 0x827E0CA0
//
// The constructor clears six 5-word state groups (two of them written by a
// stride-5 loop the compiler partially unrolled), sets a state flag to -1, and
// installs the shared icon dispatch tables into all ten icon render states.
// SetRenderEnabled stores the enabled flag and resets the icon animation state.

namespace BrnGui
{
namespace
{
    // Shared icon dispatch tables recovered from the data segment.
    void* const KP_IconVtable = reinterpret_cast<void*>(0x820CEB64);
    void* const KP_IconIface  = reinterpret_cast<void*>(0x820CEB40);
    void* const KP_IconData   = reinterpret_cast<void*>(0x82072F8C);
}

CrashNavIconRenderer::CrashNavIconRenderer()
{
    for (int i = 0; i < 5; ++i)
    {
        mGroupA[i] = 0;
        mGroupB[i] = 0;
        mLoopGroups[0][i] = 0;
        mLoopGroups[1][i] = 0;
        mGroupC[i] = 0;
        mGroupD[i] = 0;
    }

    mState81 = -1;

    for (int i = 0; i < 10; ++i)
    {
        mIcons[i].mpVtable = KP_IconVtable;
        mIcons[i].mpIface  = KP_IconIface;
        mIcons[i].mpData   = KP_IconData;
    }
}

CrashNavIconRenderer* CrashNavIconRenderer::SetRenderEnabled(bool lbEnabled)
{
    // @0x827E0CA0: r11 is loaded with 0 (li r11,0) and is the value stored by
    // every clearing store below -- the two 64-bit `std r11` writes at +0x150/+0x158
    // store 0, NOT 0x400000000 (a Hex-Rays misread of a doubleword zero store). The
    // only non-zero store is `stw r10,0x1B4` with r10=4 (li r10,4) -> mMode436 = 4.
    mbRenderEnabled = lbEnabled;          // stb r4, 4(r3)

    mAnimField0 = 0;                      // std r11(=0), 0x150(r3)
    mAnimField1 = 0;                      // std r11(=0), 0x158(r3)
    mField356   = 0;                      // stw r11(=0), 0x164(r3)
    mField352   = 0;                      // stw r11(=0), 0x160(r3)
    mField456   = 0;                      // stw r11(=0), 0x1C8(r3)
    mField452   = 0;                      // stw r11(=0), 0x1C4(r3)
    mField5444  = 0;                      // stw r11(=0), 0x1544(r3)
    mMode436    = 4;                      // stw r10(=4), 0x1B4(r3)

    return this;
}
}
