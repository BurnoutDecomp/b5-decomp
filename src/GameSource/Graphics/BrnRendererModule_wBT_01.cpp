// BrnRendererModule_wBT_01.cpp -- BrnRendererModule bodies landed beside the (upstream-hot)
// BrnRendererModule.cpp: the debug screenshot trigger and the graphics-memory report.

#include "GameSource/Graphics/BrnRendererModule.h"
#include "GameSource/Resource/BrnResourceAllocator.h"          // BrnResource::LinearResourceAllocator
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"                                 // rw::Resource / rw::ResourceDescriptor

#include <cstring>
#include <cstdio>
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "pc/gcm/renderengine/device.h"

// ARTIST823F6730. A request is consumed once even when lock/open/write fails.
// The original prefix bytes are 64 3A 5C 00; sanitizing the whole filename changes its
// colon to an underscore. Preserve the resulting relative d_\ path and do not
// create directories or redirect failed writes.
void BrnRendererModule::TakeScreenshot()
{
    if (!mbDispatchThreadTakeScreenshot)
        return;

    renderengine::Device::LockedFrontBuffer lLockedFrontBuffer = {};
    if (renderengine::Device::LockFrontBuffer(lLockedFrontBuffer))
    {
        char lacFileName[256];
        CgsCore::SPrintf(lacFileName, sizeof(lacFileName), "%sscreenshot%d_%s.tga",
                        "d:\\", static_cast<s32>(muScreenshotCounter++), macScreenShotText);
        for (char* lpcCharacter = lacFileName; *lpcCharacter != '\0'; ++lpcCharacter)
        {
            if (*lpcCharacter == ' ' || *lpcCharacter == ':')
                *lpcCharacter = '_';
        }

        FILE* lpFile = std::fopen(lacFileName, "wb");
        if (lpFile)
        {
            // External TGA header bytes: uncompressed24-bit BGR, bottom origin.
            u8 lauHeader[18] = {};
            lauHeader[2] = 2;
            lauHeader[12] = static_cast<u8>(lLockedFrontBuffer.width);
            lauHeader[13] = static_cast<u8>(lLockedFrontBuffer.width >> 8);
            lauHeader[14] = static_cast<u8>(lLockedFrontBuffer.height);
            lauHeader[15] = static_cast<u8>(lLockedFrontBuffer.height >> 8);
            lauHeader[16] = 24;
            bool lbWriteFailed = std::fwrite(lauHeader, sizeof(lauHeader), 1, lpFile) != 1;

            // FLAG PC-platform leaf: native D3D9 readback already supplies
            // linear BGRX/BGRA rows. Xenos untile and its5120-byte scratch-buffer
            // limit do not apply to this native row source.
            const u8* lpuPixels = static_cast<const u8*>(lLockedFrontBuffer.pixelData);
            for (s32 liRow = static_cast<s32>(lLockedFrontBuffer.height) - 1; liRow >= 0; --liRow)
            {
                const u8* lpuRow = lpuPixels + static_cast<usize>(liRow) * lLockedFrontBuffer.pitch;
                for (u32 luColumn = 0; luColumn < lLockedFrontBuffer.width; ++luColumn)
                {
                    const u8* lpuPixel = lpuRow + luColumn * 4u;
                    const u8 lauBgr[3] = {lpuPixel[0], lpuPixel[1], lpuPixel[2]};
                    if (std::fwrite(lauBgr, sizeof(lauBgr), 1, lpFile) != 1)
                        lbWriteFailed = true;
                }
            }
            if (std::fclose(lpFile) != 0)
                lbWriteFailed = true;

            // FLAG PC-platform leaf: observe actual I/O failure without changing
            // original request consumption, counter behavior or remaining writes.
            if (lbWriteFailed)
            {
                char lacMessage[360];
                std::snprintf(lacMessage, sizeof(lacMessage),
                              "[screenshot] write failed: %s\n", lacFileName);
                CgsDev::Log::WriteToLog(lacMessage);
            }
        }
        else
        {
            // FLAG PC-platform leaf: failure observation only; no path fallback.
            char lacMessage[360];
            std::snprintf(lacMessage, sizeof(lacMessage),
                          "[screenshot] open failed: %s\n", lacFileName);
            CgsDev::Log::WriteToLog(lacMessage);
        }
        renderengine::Device::UnlockFrontBuffer(lLockedFrontBuffer);
    }
    mbDispatchThreadTakeScreenshot = false;
}

// -------------------------------------------------------------------------------------------------
// BrnRendererModule::DEBUGTriggerScreenShot
// Called by BrnGame::BrnGameModule::OnEndOfUpdateFrame. Requests a screenshot from the update
// thread (EndOfFrame hands the request on to the dispatch thread), asks for the 2D overlays to be
// captured too, and records the caption. The caption copy is the inlined CgsStringUtils StrnCpy:
// a "String too long: " assert on the source length, then a bounded strncpy.
// -------------------------------------------------------------------------------------------------
void BrnRendererModule::DEBUGTriggerScreenShot(const char* lpcScreenShotText)
{
    mbUpdateThreadTakeScreenshot  = true;
    mbCaptureOverlaysInScreenshot = true;

    CGS_ASSERT(std::strlen(lpcScreenShotText) < KU_SCREENSHOT_TEXT_LENGTH, "String too long: ");
    std::strncpy(macScreenShotText, lpcScreenShotText, KU_SCREENSHOT_TEXT_LENGTH);
}

// -------------------------------------------------------------------------------------------------
// BrnRendererModule::MemoryUsage
// Called by Construct and Prepare. Prints the graphics linear allocator's two pools to the debug
// stream: lane 0 is main memory, lane 2 is physical memory. Each pool reports its start and end
// address (end = start + capacity) and its usage against capacity. Every line is gated on the
// global message-filter bit.
//
// x64: the pool bases are host pointers, so the two addresses go through the 64-bit overload; the
// sizes stay u32 (the console streams all six through the u32 "%u" overload).
// -------------------------------------------------------------------------------------------------
void BrnRendererModule::MemoryUsage()
{
    const rw::Resource           lHeapBase = mpGraphicsAllocator->GetLinearHeapBase();
    const rw::ResourceDescriptor lUsage    = mpGraphicsAllocator->GetCurrentUsage();
    const rw::ResourceDescriptor lCapacity = mpGraphicsAllocator->GetCapacity();

    const u8* const lpMainStart     = static_cast<const u8*>(lHeapBase.m_baseResources[0]);
    const u8* const lpPhysicalStart = static_cast<const u8*>(lHeapBase.m_baseResources[2]);
    const u32 luMainCapacity        = lCapacity.m_baseResourceDescriptors[0].m_size;
    const u32 luPhysicalCapacity    = lCapacity.m_baseResourceDescriptors[2].m_size;
    const u32 luMainUsage           = lUsage.m_baseResourceDescriptors[0].m_size;
    const u32 luPhysicalUsage       = lUsage.m_baseResourceDescriptors[2].m_size;

    const u64 lu64MainStart     = static_cast<u64>(reinterpret_cast<uintptr_t>(lpMainStart));
    const u64 lu64PhysicalStart = static_cast<u64>(reinterpret_cast<uintptr_t>(lpPhysicalStart));
    // FLAG PC-platform leaf: a retained native arena can have an empty physical
    // domain. Sum printed addresses as native integers, without null-pointer arithmetic.
    const u64 lu64MainEnd       = lu64MainStart + luMainCapacity;
    const u64 lu64PhysicalEnd   = lu64PhysicalStart + luPhysicalCapacity;

    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        *CgsDev::Log::gpDebugPrint << "===================================================\n";
    }
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        *CgsDev::Log::gpDebugPrint << "BrnRendererModule: Memory Usage ";
    }
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        *CgsDev::Log::gpDebugPrint << "for XBox360\n";
    }
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        *CgsDev::Log::gpDebugPrint << "Main Memory start address " << lu64MainStart
                                   << " end address: " << lu64MainEnd << "\n";
    }
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        *CgsDev::Log::gpDebugPrint << "Main Memory total usage " << luMainUsage
                                   << "b out of " << luMainCapacity
                                   << "b so " << (luMainCapacity - luMainUsage) << "b free\n";
    }
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        *CgsDev::Log::gpDebugPrint << "Physical Memory start address " << lu64PhysicalStart
                                   << " end address: " << lu64PhysicalEnd << "\n";
    }
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        *CgsDev::Log::gpDebugPrint << "Physical Memory total usage " << luPhysicalUsage
                                   << "b out of " << luPhysicalCapacity
                                   << "b so " << (luPhysicalCapacity - luPhysicalUsage) << "b free\n";
    }
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        *CgsDev::Log::gpDebugPrint << "======================================================\n";
    }
}
