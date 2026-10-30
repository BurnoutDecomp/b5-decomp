// BrnRendererModule_wBT_01.cpp -- BrnRendererModule bodies landed beside the (upstream-hot)
// BrnRendererModule.cpp: the debug screenshot trigger.

#include "GameSource/Graphics/BrnRendererModule.h"
#include "GameSource/Resource/BrnResourceAllocator.h"          // BrnResource::LinearResourceAllocator
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"                                 // rw::Resource / rw::ResourceDescriptor

#include <cstring>
#include <cstdio>
#include "GameShared/GameClasses/Core/CgsStringUtils.h"

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

// MemoryUsage is defined in BrnRendererModule.cpp (ARTIST823FBDE0).
