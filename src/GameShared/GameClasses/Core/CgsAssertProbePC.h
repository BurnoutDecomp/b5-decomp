#pragma once
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/CgsHarnessSlot.h"

namespace CgsDev { namespace Assert {
// FLAG PC-platform leaf: an opt-in, slot-local regression trigger. These events
// exercise the real assertion entry points at update and mid-render call sites.
inline void PollFrameProbePC(unsigned luPoint)
{
    static const char* spcEnabled = std::getenv("BRN_ASSERT_FRAME_PROBE");
    static const bool sbEnabled = spcEnabled && spcEnabled[0] == '1';
    if (!sbEnabled || luPoint >= 2) return;
    static HANDLE sahEvents[2] = {};
    if (!sahEvents[luPoint])
    {
        char lacBase[80], lacName[96];
        std::snprintf(lacBase,sizeof(lacBase),"Local\\BurnoutPC_AssertProbe_%s",luPoint ? "Dispatch" : "Update");
        sahEvents[luPoint] = OpenEventA(SYNCHRONIZE,FALSE,
            CgsSystem::HarnessSlot::Name(lacName,sizeof(lacName),lacBase));
    }
    if (sahEvents[luPoint] && WaitForSingleObject(sahEvents[luPoint],0)==WAIT_OBJECT_0)
    {
        if (luPoint) CGS_ASSERT(false,"native dispatch assertion probe");
        else CGS_ASSERT(false,"native update assertion probe");
    }
}
} }
