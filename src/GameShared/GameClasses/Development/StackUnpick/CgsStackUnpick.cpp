#include <xtl.h>
#include <xbdm.h>
#include "GameShared/GameClasses/Development/StackUnpick/CgsStackUnpick.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstring>
#include <string.h>

namespace CgsDev
{
    namespace
    {
        void ClearStack(StackUnpickBase& lrStack)
        {
            lrStack.miNumStackAddresses = 0;
            for (s32 liIndex = 0; liIndex < StackUnpickBase::KI_MAX_STACK_ADDRESSES; ++liIndex)
            {
                lrStack.maAdjustedStack[liIndex] = 0;
                lrStack.maStack[liIndex] = 0;
            }
        }
    }

    uintptr_t StackUnpickBase::GetStackAddress(s32 liIndex) const
    {
        if (liIndex < 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("liIndex >= 0",
                "..\\..\\..\\GameShared\\GameClasses\\Development/StackUnpick/CgsStackUnpick.cpp", 74);
            CgsDev::Assert::EndAssert();
        }
        return liIndex < miNumStackAddresses ? maAdjustedStack[liIndex] : 0;
    }

    void StackUnpickBase::RemoveFirstItemInStack()
    {
        if (miNumStackAddresses > 0)
        {
            for (s32 liIndex = 0; liIndex < miNumStackAddresses - 1; ++liIndex)
            {
                maAdjustedStack[liIndex] = maAdjustedStack[liIndex + 1];
                maStack[liIndex] = maStack[liIndex + 1];
            }
            --miNumStackAddresses;
            maAdjustedStack[miNumStackAddresses] = 0;
            maStack[miNumStackAddresses] = 0;
        }
    }

    // ARTIST82819118: actual XDK capture, count, rebasing and first-frame removal.
    // The original returns true even when capture/rebasing clears the stack.
    bool StackUnpick::Prepare()
    {
        ClearStack(*this);
        const HRESULT lResult = DmCaptureStackBackTrace(KI_MAX_STACK_ADDRESSES,
            reinterpret_cast<PVOID*>(maStack));
        miNumStackAddresses = 0;
        if (FAILED(lResult))
        {
            ClearStack(*this);
            return true;
        }
        while (miNumStackAddresses < KI_MAX_STACK_ADDRESSES && maStack[miNumStackAddresses])
            ++miNumStackAddresses;
        // ARTIST data82F31FEC is00400000; the calibrated reference scan finds
        // only this reader, while explicit map-file rebasing supplies its own base.
        if (!ComputeAdjustedStack(0x00400000u))
        {
            ClearStack(*this);
            return true;
        }
        RemoveFirstItemInStack();
        return true;
    }

    // ARTIST828173D8: use the native executable path and module walk. Do not
    // replace the walk with a host module handle or synthesize stack addresses.
    bool StackUnpick::ComputeAdjustedStack(uintptr_t luBaseAddress)
    {
        std::memcpy(maAdjustedStack, maStack, sizeof(maStack));
        DM_XBE lXbe;
        std::memset(&lXbe, 0, sizeof(lXbe));
        if (FAILED(DmGetXbeInfo(0, &lXbe)))
        {
            ClearStack(*this);
            return true;
        }
        const char* lpcName = std::strrchr(lXbe.LaunchPath, '\\');
        if (!lpcName)
            lpcName = lXbe.LaunchPath;
        if (*lpcName == '\\')
            ++lpcName;
        PDM_WALK_MODULES lpWalk = 0;
        DMN_MODLOAD lModule;
        uintptr_t luModuleBase = 0x82000000u;
        HRESULT lResult = DmWalkLoadedModules(&lpWalk, &lModule);
        while (lResult == XBDM_NOERR)
        {
            if (_stricmp(lModule.Name, lpcName) == 0)
                luModuleBase = reinterpret_cast<uintptr_t>(lModule.BaseAddress);
            lResult = DmWalkLoadedModules(&lpWalk, &lModule);
        }
        if (lResult != XBDM_ENDOFLIST)
            return false;
        DmCloseLoadedModules(lpWalk);
        for (s32 liIndex = 0; liIndex < miNumStackAddresses; ++liIndex)
        {
            const uintptr_t luAddress = maAdjustedStack[liIndex];
            if (luAddress && luAddress >= luModuleBase)
                maAdjustedStack[liIndex] = luAddress - luModuleBase + luBaseAddress;
        }
        return true;
    }
}
