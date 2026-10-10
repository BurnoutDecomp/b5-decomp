// BrnRendererModule_wBT_01.cpp -- BrnRendererModule bodies landed beside the (upstream-hot)
// BrnRendererModule.cpp: the debug screenshot trigger and the graphics-memory report.

#include "GameSource/Graphics/BrnRendererModule.h"
#include "GameSource/Resource/BrnResourceAllocator.h"          // BrnResource::LinearResourceAllocator
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"                                 // rw::Resource / rw::ResourceDescriptor

#include <cstring>

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
    const u64 lu64MainEnd       = static_cast<u64>(reinterpret_cast<uintptr_t>(lpMainStart + luMainCapacity));
    const u64 lu64PhysicalStart = static_cast<u64>(reinterpret_cast<uintptr_t>(lpPhysicalStart));
    const u64 lu64PhysicalEnd   = static_cast<u64>(reinterpret_cast<uintptr_t>(lpPhysicalStart + luPhysicalCapacity));

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
