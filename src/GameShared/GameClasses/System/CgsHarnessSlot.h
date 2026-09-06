#pragma once

#include "types.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstring>

// FLAG PC-platform leaf: the unattended test harness runs SEVERAL game instances at once
// (tools\tests\slots.ps1 + flow_run.ps1 -Slot <n>), and every host-global name the PC build
// creates or opens is shared by the whole logon session:
//
//   * the single-instance mutex "BurnoutParadiseexe" (CgsHardwareInitPC::IsAlreadyRunning) --
//     WITHOUT a per-slot name the second instance simply refuses to start, so this is not a
//     nicety, it is the thing that makes parallel runs possible at all;
//   * the harness input channels "Local\BurnoutPC_Input_*" (CgsInputPadsPC) and the assert
//     release "Local\BurnoutPC_Assert_Release" (CgsAssertManager) -- one Set() would otherwise
//     press the throttle in EVERY running instance;
//   * the profile container directory "Memcard" (CgsSaveLoadPC) -- two instances writing one
//     Profile.sav is a corrupted save, not a slow one.
//
// The slot id comes from the environment variable BRN_HARNESS_SLOT, which only the harness
// sets. UNSET OR "0" YIELDS THE EMPTY SUFFIX, so slot 0 -- the default path every existing
// script, golden and banked run was measured through -- keeps byte-identical names. That
// property is the whole design constraint: a slot mechanism that changed the default run's
// object names would silently un-compare every run taken before it.
//
// ⚠️ Read ONCE and cached: these names are composed on paths that run per input update, and
// the value cannot change inside a process.
// DELETE-WHEN the harness stops running more than one game instance per box.
namespace CgsSystem
{
namespace HarnessSlot
{
    // "" for slot 0 / unset, "_<n>" otherwise. Digits only -- anything else is ignored rather
    // than pasted into a kernel object name.
    inline const char* Suffix()
    {
        static char sacSuffix[8]  = { 0 };
        static bool sbResolved    = false;
        if (!sbResolved)
        {
            sbResolved = true;
            const char* lpcSlot = std::getenv("BRN_HARNESS_SLOT");
            if (lpcSlot != 0 && lpcSlot[0] != '\0' &&
                !(lpcSlot[0] == '0' && lpcSlot[1] == '\0'))
            {
                u32 luOut = 0;
                sacSuffix[luOut++] = '_';
                for (const char* lpc = lpcSlot; *lpc != '\0' && luOut < (sizeof(sacSuffix) - 1); ++lpc)
                {
                    if (*lpc < '0' || *lpc > '9')
                    {
                        luOut = 0;          // not a slot number at all -- fall back to slot 0
                        break;
                    }
                    sacSuffix[luOut++] = *lpc;
                }
                sacSuffix[luOut] = '\0';
            }
        }
        return sacSuffix;
    }

    // Compose "<base><suffix>" into the caller's buffer and return it. On overflow the base is
    // returned unchanged -- a shared name is a wrong measurement, but a truncated one is a name
    // nothing opens at all, which is worse (it reads exactly like a dead channel).
    inline const char* Name(char* lpacOut, u32 luSize, const char* lpcBase)
    {
        const char* lpcSuffix = Suffix();
        if (lpcSuffix[0] == '\0')
            return lpcBase;
        const int liWritten = std::snprintf(lpacOut, luSize, "%s%s", lpcBase, lpcSuffix);
        if (liWritten <= 0 || static_cast<u32>(liWritten) >= luSize)
            return lpcBase;
        return lpacOut;
    }
}
}
