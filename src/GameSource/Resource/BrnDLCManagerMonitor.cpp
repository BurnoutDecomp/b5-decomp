#include "GameSource/Resource/BrnDLCManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <eathread/eathread_futex.h>                         // AutoFutex for the selected vendor Futex
#include "SDKs/Realmc/RealmcXenonUtil.h"                     // XCONTENT_DATA / XCONTENT_DEVICE_DATA
#include <cstring>   // memcpy

// Recovered DLC monitor boundary; deliberately unmounted from the shipping TU list.
// These original methods require genuine PC XContent discovery/device/mount providers.
// The selected DLCManager::Construct remains in BrnDLCManager.cpp and starts no worker.
// Select this TU together with the original DLC Prepare path only when its backend and
// worker/owner lifetime are established; this split supplies no successful-empty provider.

extern "C"
{
    DWORD XContentCreateEnumerator(DWORD dwUserIndex, DWORD dwDeviceID, DWORD dwContentType,
                                   DWORD dwContentFlags, DWORD cItems, DWORD* pcbBuffer,
                                   HANDLE* phEnum);
    DWORD XContentGetDeviceData(DWORD dwDeviceID, void* pDeviceData);
    DWORD XContentCreate(DWORD dwUserIndex, const char* szRootName, void* pContentData,
                         DWORD dwContentFlags, DWORD* pdwDisposition, DWORD* pdwLicenseMask,
                         void* pOverlapped);
    u32 XEnumerate(void* lhEnum, void* lpvBuffer, u32 lcbBuffer,
                   u32* lpcItemsReturned, void* lpOverlapped);
}

namespace BrnResource
{
    // ARTIST 0x82669DB8. The actual critical-section member is initialized by
    // the GameDataModule C++ member-construction path (0x827E3230), not here in
    // this separate two-phase manager Construct.
    void DLCManager::StartDLCMonitor()
    {
        if (!mbDLCMonitorThreadStarted)
        {
            EA::Thread::ThreadParameters lParameters;
            lParameters.mnProcessor = 0;
            lParameters.mnPriority = 100;

            mDLCMonitorThread.Begin(
                &DLCManager::X360DLCEnumerationThreadProc,
                this,
                &lParameters,
                EA::Thread::Thread::GetGlobalRunnableFunctionUserWrapper());
            mbDLCMonitorThreadStarted = true;
        }
    }

    // ARTIST 0x82665A58. This is a one-shot engine worker, kept separate from
    // Prepare: the XContent enumerator/device/create calls remain external. This
    // source slice adds no PC discovery/mount backend; Prepare remains unactivated,
    // so the current path does not start this worker.
    intptr_t DLCManager::X360DLCEnumerationThreadProc(void* lpContext)
    {
        DLCManager* const lpManager = static_cast<DLCManager*>(lpContext);
        alignas(RealmcIface::XCONTENT_DATA) u8 lacTempBuffer[0x2000];
        u32 luPriorPackageCount;

        {
            EA::Thread::AutoFutex lLock(lpManager->mMonitorLock);
            luPriorPackageCount = static_cast<u32>(lpManager->muMonitorPackageCount);
        }

        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Creating DLC enumerator\n";

        DWORD luRequiredBufferSize = 0;
        HANDLE lhEnumerator;
        const DWORD luEnumeratorError = XContentCreateEnumerator(
            0, 0, 2, 0, 1, &luRequiredBufferSize, &lhEnumerator);

        if (luEnumeratorError == 0)
        {
            CGS_ASSERT(luRequiredBufferSize <= static_cast<DWORD>(sizeof(lacTempBuffer)),
                       "luRequiredBufferSize <= sizeof(lacTempBuffer)");

            u32 luQualifiedPackageCount = 0;
            do
            {
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint << "Enumerating DLC\n";

                u32 luItemsReturned;
                XEnumerate(lhEnumerator, lacTempBuffer,
                           static_cast<u32>(sizeof(lacTempBuffer)),
                           &luItemsReturned, 0);
                if (luItemsReturned == 0)
                    break;

                RealmcIface::XCONTENT_DATA* const lpContentRecords =
                    reinterpret_cast<RealmcIface::XCONTENT_DATA*>(lacTempBuffer);
                for (u32 liRecord = 0; liRecord < luItemsReturned; ++liRecord)
                {
                    RealmcIface::XCONTENT_DATA& lContent = lpContentRecords[liRecord];
                    char lacFileName[43];
                    std::memcpy(lacFileName, lContent.szFileName, sizeof(lContent.szFileName));
                    lacFileName[42] = 0;

                    if (lContent.dwContentType == 2)
                    {
                        RealmcIface::XCONTENT_DEVICE_DATA lDeviceData;
                        XContentGetDeviceData(lContent.DeviceID, &lDeviceData);
                        if (lDeviceData.dwDeviceType == 1)
                        {
                            if (CgsDev::Message::gxMessageFilterFlags & 1)
                                *CgsDev::Log::gpDebugPrint
                                    << "Marketplace content found on hard disk at "
                                    << lacFileName << "\n";

                            if (luQualifiedPackageCount >= luPriorPackageCount)
                            {
                                char lacContainerName[16];
                                CgsCore::SPrintf(lacContainerName,
                                                 static_cast<u32>(sizeof(lacContainerName)),
                                                 "dlc%u", luQualifiedPackageCount);
                                if (XContentCreate(0, lacContainerName,
                                                   &lContent, 3, 0, 0, 0) != 0)
                                {
                                    if (CgsDev::Message::gxMessageFilterFlags & 1)
                                        *CgsDev::Log::gpDebugPrint << "XContentCreate failed\n";
                                }
                            }
                            ++luQualifiedPackageCount;
                        }
                    }
                }
            }
            while (true);

            CloseHandle(lhEnumerator);
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "Enumeration complete - num items="
                                            << luQualifiedPackageCount << "\n";

            EA::Thread::AutoFutex lLock(lpManager->mMonitorLock);
            lpManager->muMonitorPackageCount = static_cast<s32>(luQualifiedPackageCount);
        }
        else if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            *CgsDev::Log::gpDebugPrint << "Failed to create enumerator with error: "
                                        << static_cast<s32>(luEnumeratorError) << "\n";
        }

        return 0;
    }

} // namespace BrnResource
