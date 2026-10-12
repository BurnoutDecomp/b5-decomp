#include "GameShared/GameClasses/System/Resource/CgsDebugPoolHistogram.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"

namespace CgsResource
{
    // ARTIST828D9ED0. The original Prepare parameters occupy f1/f2 and
    // argument slots r4/r5; the pool-name pointer and flags are in r6/r7.
    void DebugPoolHistogram::Reset(const Pool* lpPool)
    {
        CgsDev::DebugUI::Window::Prepare(200.0f, 250.0f, lpPool->GetName(), KX_FLAGNORMAL);
        mpPool = lpPool;
    }
}
