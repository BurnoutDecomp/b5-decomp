#include "GameShared/GameClasses/Graphics/CgsPerformanceTracker.h"

// The four lifecycle hooks are empty in this build: ARTIST calls each of them at 0x8284CB38, the
// lone blr shared by every empty function in the image, and the DecFIGS PS3 bodies are a lone blr
// as well (CgsPerformanceTracker.cpp:23, :71, :107, :112). See the header for the call sites.

void CgsGraphics::PerformanceTracker::Startup()
{
}

void CgsGraphics::PerformanceTracker::Shutdown()
{
}

void CgsGraphics::PerformanceTracker::BeginFrame()
{
}

void CgsGraphics::PerformanceTracker::EndFrame()
{
}
