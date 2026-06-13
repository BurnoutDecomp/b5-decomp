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
    mbRenderEnabled = lbEnabled;

    mAnimField0 = 0x400000000ull;
    mAnimField1 = 0x400000000ull;
    mField352   = 0;
    mField356   = 0;
    mField452   = 0;
    mField456   = 0;
    mField5444  = 0;
    mMode436    = 4;

    return this;
}
}
