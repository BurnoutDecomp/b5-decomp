#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include <cstring>

// ARTIST CgsPerfMonGpuX360.cpp. The ledger retains the PS3 pathname.
namespace CgsDev
{
    namespace
    {
        const s32 KI_NUMBUFFERS = 5;
        const s32 KI_NUMTIMERS = KI_PERFMONGPUMAXMONITORS + 1;
        const char KAC_ASSERTFILE[] = "..\\..\\..\\GameShared\\GameClasses\\Development/PerfMon/Gpu/X360/CgsPerfMonGpuX360.cpp";

        // ARTIST828259B8 uses33 costs followed by33 start times in each0x210-byte
        // record. These X360 records differ from the DecFIGS RSX query-ID form.
        struct PerfMonGpuX360_Values
        {
            u64 mau64Cost[KI_NUMTIMERS];
            u64 mau64StartTime[KI_NUMTIMERS];
        };

        struct PerfMonGpuX360_Buffer
        {
            PerfMonGpuX360_Values maValues[KI_NUMBUFFERS];
            bool mabUsedThisFrame[KI_PERFMONGPUMAXMONITORS];
            s32 miCurrentCPUReadBuffer;
            s32 miCurrentGPUWriteBuffer;
        };

        // Each producer stores two big-endian halfwords; D3DCALLBACK receives
        // the corresponding DWORD. Frame marks leave the unused ID unwritten.
        union GpuTimeMark
        {
            struct
            {
                u16 mu16Type;
                u16 mu16MonitorID;
            } mFields;
            DWORD muContext;
        };

        CGS_STATIC_ASSERT((sizeof(PerfMonGpuX360_Values) == 0x210), "ARTIST GPU timer record");
        CGS_STATIC_ASSERT((offsetof(PerfMonGpuX360_Values, mau64StartTime) == 0x108), "ARTIST GPU start timers");
        CGS_STATIC_ASSERT((sizeof(PerfMonGpuX360_Buffer) == 0xA78), "ARTIST GPU timer bank");
        CGS_STATIC_ASSERT((offsetof(PerfMonGpuX360_Buffer, mabUsedThisFrame) == 0xA50), "ARTIST GPU monitor use flags");
        CGS_STATIC_ASSERT((offsetof(PerfMonGpuX360_Buffer, miCurrentCPUReadBuffer) == 0xA70), "ARTIST GPU CPU read index");
        CGS_STATIC_ASSERT((offsetof(PerfMonGpuX360_Buffer, miCurrentGPUWriteBuffer) == 0xA74), "ARTIST GPU write index");
        CGS_STATIC_ASSERT((sizeof(GpuTimeMark) == 4), "ARTIST GPU callback context");
        CGS_STATIC_ASSERT((offsetof(GpuTimeMark, mFields.mu16Type) == 0), "ARTIST GPU callback mark half");
        CGS_STATIC_ASSERT((offsetof(GpuTimeMark, mFields.mu16MonitorID) == 2), "ARTIST GPU callback ID half");

        // ARTIST83018270..83018CE7 and83018CE8. Callback/CPU accesses retain the
        // original repeated loads; DecFIGS corroborates the volatile bank.
        volatile PerfMonGpuX360_Buffer gValues;
        f32 gfCyclesToPercent;

        // ARTIST828259B8; exact native SDK D3DCALLBACK signature is void(DWORD).
        void GpuTimerCallback(DWORD luContext)
        {
            GpuTimeMark lMark;
            lMark.muContext = luContext;
            LARGE_INTEGER lCounter;
            switch (lMark.mFields.mu16Type)
            {
            case 0:
                gValues.miCurrentGPUWriteBuffer = (gValues.miCurrentGPUWriteBuffer + 1) % KI_NUMBUFFERS;
                for (s32 liIndex = 0; liIndex < KI_NUMTIMERS; ++liIndex)
                    gValues.maValues[gValues.miCurrentGPUWriteBuffer].mau64Cost[liIndex] = 0;
                QueryPerformanceCounter(&lCounter);
                gValues.maValues[gValues.miCurrentGPUWriteBuffer].mau64StartTime[KI_PERFMONGPUMAXMONITORS] = static_cast<u64>(lCounter.QuadPart);
                break;
            case 1:
                QueryPerformanceCounter(&lCounter);
                gValues.maValues[gValues.miCurrentGPUWriteBuffer].mau64Cost[KI_PERFMONGPUMAXMONITORS] =
                    static_cast<u64>(lCounter.QuadPart) - gValues.maValues[gValues.miCurrentGPUWriteBuffer].mau64StartTime[KI_PERFMONGPUMAXMONITORS];
                break;
            case 2:
                QueryPerformanceCounter(&lCounter);
                gValues.maValues[gValues.miCurrentGPUWriteBuffer].mau64StartTime[lMark.mFields.mu16MonitorID] = static_cast<u64>(lCounter.QuadPart);
                break;
            case 3:
                QueryPerformanceCounter(&lCounter);
                gValues.maValues[gValues.miCurrentGPUWriteBuffer].mau64Cost[lMark.mFields.mu16MonitorID] +=
                    static_cast<u64>(lCounter.QuadPart) - gValues.maValues[gValues.miCurrentGPUWriteBuffer].mau64StartTime[lMark.mFields.mu16MonitorID];
                break;
            default:
                Assert::BeginAssert();
                Assert::FireAssert("invalid mark type", KAC_ASSERTFILE, 0xAA);
                Assert::EndAssert();
                break;
            }
        }
    }

    bool PerfMonGpu::Construct()
    {
        miMaxMonitor = 0;
        meGameFrequency = E_PMF_60HZ;
        mbActiveMonitor = false;
        mbProfilingRunning = false;
        miActiveMonitorID = 0;
        std::memset(maMonitors, 0, sizeof(maMonitors));
        std::memset(const_cast<PerfMonGpuX360_Buffer*>(&gValues), 0, sizeof(gValues));
        gValues.miCurrentCPUReadBuffer = 2;
        gValues.miCurrentGPUWriteBuffer = 0;

        LARGE_INTEGER lFrequency;
        if (!QueryPerformanceFrequency(&lFrequency))
        {
            Assert::BeginAssert();
            Assert::FireAssert("QueryPerformanceFrequency(&lFrequency)", KAC_ASSERTFILE, 0xD1);
            Assert::EndAssert();
        }
        gfCyclesToPercent = 100.0f / static_cast<f32>(static_cast<u64>(lFrequency.QuadPart));
        return true;
    }

    void PerfMonGpu::Swap()
    {
        if (mbProfilingRunning)
        {
            Assert::BeginAssert();
            Assert::FireAssert("mbProfilingRunning == false", KAC_ASSERTFILE, 0xF8);
            Assert::EndAssert();
        }
        gValues.miCurrentCPUReadBuffer = (gValues.miCurrentCPUReadBuffer + 1) % KI_NUMBUFFERS;
    }

    void PerfMonGpu::StartProfiling()
    {
        D3DDevice* const lpDevice = gpD3DDevice;
        GpuTimeMark lMark;
        lMark.mFields.mu16Type = 0;
        for (s32 liIndex = 0; liIndex < KI_PERFMONGPUMAXMONITORS; ++liIndex)
            gValues.mabUsedThisFrame[liIndex] = false;
        lpDevice->SetPredication(0x3FFFFFFF);
        lpDevice->InsertCallback(D3DCALLBACK_IDLE, GpuTimerCallback, lMark.muContext);
        lpDevice->SetPredication(0);
        mbProfilingRunning = true;
    }

    void PerfMonGpu::StopProfiling()
    {
        D3DDevice* const lpDevice = gpD3DDevice;
        GpuTimeMark lMark;
        lMark.mFields.mu16Type = 1;
        lpDevice->SetPredication(0x3FFFFFFF);
        lpDevice->InsertCallback(D3DCALLBACK_IDLE, GpuTimerCallback, lMark.muContext);
        lpDevice->SetPredication(0);
        mbProfilingRunning = false;
    }

    void PerfMonGpu::StartMonitor(s32 liMonitorId)
    {
        if (liMonitorId < 0)
        {
            Assert::BeginAssert();
            Assert::FireAssert("liMonitorID >= 0", KAC_ASSERTFILE, 0x151);
            Assert::EndAssert();
        }
        if (liMonitorId >= KI_PERFMONGPUMAXMONITORS)
        {
            Assert::BeginAssert();
            Assert::FireAssert("liMonitorID < KI_PERFMONGPUMAXMONITORS", KAC_ASSERTFILE, 0x152);
            Assert::EndAssert();
        }
        if (mbActiveMonitor)
        {
            Assert::BeginAssert();
            StrStream lStrStream(Assert::gpcMessageBuffer, Assert::giMessageBufferSize);
            lStrStream << "Monitor is already started: ";
            const char* lpcName = maMonitors[miActiveMonitorID].macName;
            lStrStream << (lpcName ? lpcName : "<NULLSTRING>");
            lStrStream << "\n";
            Assert::FireAssert(Assert::gpcMessageBuffer, KAC_ASSERTFILE, 0x153);
            Assert::EndAssert();
        }
        if (gValues.mabUsedThisFrame[liMonitorId])
        {
            Assert::BeginAssert();
            Assert::FireAssert("gValues.mabUsedThisFrame[liMonitorID] == false", KAC_ASSERTFILE, 0x154);
            Assert::EndAssert();
        }
        if (mbProfilingRunning != true)
        {
            Assert::BeginAssert();
            Assert::FireAssert("mbProfilingRunning == true", KAC_ASSERTFILE, 0x155);
            Assert::EndAssert();
        }

        GpuTimeMark lMark;
        lMark.mFields.mu16MonitorID = static_cast<u16>(liMonitorId);
        miActiveMonitorID = liMonitorId;
        lMark.mFields.mu16Type = 2;
        D3DDevice* const lpDevice = gpD3DDevice;
        mbActiveMonitor = true;
        gValues.mabUsedThisFrame[liMonitorId] = true;
        lpDevice->SetPredication(0x3FFFFFFF);
        lpDevice->InsertCallback(D3DCALLBACK_IDLE, GpuTimerCallback, lMark.muContext);
        lpDevice->SetPredication(0);
    }

    void PerfMonGpu::StopMonitor(s32 liMonitorId)
    {
        if (liMonitorId < 0)
        {
            Assert::BeginAssert();
            Assert::FireAssert("liMonitorID >= 0", KAC_ASSERTFILE, 0x17E);
            Assert::EndAssert();
        }
        if (liMonitorId >= KI_PERFMONGPUMAXMONITORS)
        {
            Assert::BeginAssert();
            Assert::FireAssert("liMonitorID < KI_PERFMONGPUMAXMONITORS", KAC_ASSERTFILE, 0x17F);
            Assert::EndAssert();
        }
        if (mbActiveMonitor != true)
        {
            Assert::BeginAssert();
            StrStream lStrStream(Assert::gpcMessageBuffer, Assert::giMessageBufferSize);
            lStrStream << "Monitor is not started: ";
            const char* lpcName = maMonitors[liMonitorId].macName;
            lStrStream << (lpcName ? lpcName : "<NULLSTRING>");
            lStrStream << "\n";
            Assert::FireAssert(Assert::gpcMessageBuffer, KAC_ASSERTFILE, 0x180);
            Assert::EndAssert();
        }
        if (mbProfilingRunning != true)
        {
            Assert::BeginAssert();
            Assert::FireAssert("mbProfilingRunning == true", KAC_ASSERTFILE, 0x181);
            Assert::EndAssert();
        }

        GpuTimeMark lMark;
        lMark.mFields.mu16MonitorID = static_cast<u16>(liMonitorId);
        mbActiveMonitor = false;
        D3DDevice* const lpDevice = gpD3DDevice;
        lMark.mFields.mu16Type = 3;
        lpDevice->SetPredication(0x3FFFFFFF);
        lpDevice->InsertCallback(D3DCALLBACK_IDLE, GpuTimerCallback, lMark.muContext);
        lpDevice->SetPredication(0);
    }

    f32 PerfMonGpu::ComputeValueFromTimer(s32 liMonitorId)
    {
        f32 lfRefreshRate;
        switch (meGameFrequency)
        {
        case E_PMF_25HZ: lfRefreshRate = 25.0f; break;
        case E_PMF_30HZ: lfRefreshRate = 29.97f; break;
        case E_PMF_50HZ: lfRefreshRate = 50.0f; break;
        default:
            Assert::BeginAssert();
            Assert::FireAssert("invalid FPS, assuming 60hz", KAC_ASSERTFILE, 0x1C8);
            Assert::EndAssert();
        case E_PMF_60HZ: lfRefreshRate = 59.94f; break;
        }
        const f32 lfCost = static_cast<f32>(gValues.maValues[gValues.miCurrentCPUReadBuffer].mau64Cost[liMonitorId]);
        return lfCost * gfCyclesToPercent * lfRefreshRate;
    }
}
