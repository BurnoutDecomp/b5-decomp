// FX-FOLLOWUPS (crash parity 2026-09-25): the rwcollision side of FxFollowupsFineModuleConstruct.cpp, in its own TU
// because VolumeBBoxQuery.hpp / GPInstance.hpp carry the vendor vpu vocabulary, which must not meet the
// CgsSceneManager one the fine module's headers use (the VolumeQueryHostLayout.hpp rule). It holds the link seams
// VolumeQuery.cpp needs (no query runs) and one probe the main TU calls.
#include "types.hpp"
#include <cstddef>
#include <cstring>

#include "vendor/renderware/collision/VolumeQuery.hpp"
#include "vendor/renderware/collision/VolumeBBoxQuery.hpp"
#include "vendor/renderware/collision/CollisionVolume.hpp"
#include "vendor/renderware/collision/VolRef.hpp"
#include "vendor/renderware/collision/GPInstance.hpp"
#include "vendor/renderware/collision/LineSegIntersect.hpp"

using namespace rw::collision;

#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint = nullptr; } }
// VolumeQuery.cpp's line walk announces its two [PC TRAP]s through WriteToLog (FX-FOLLOWUPS stage a); nothing here
// reaches them, so any that fires is printed.
namespace CgsDev { namespace Log { void WriteToLog(const char* lpcText) { std::printf("LOG (collision TU): %s", lpcText); } } }

namespace rw { namespace collision {
    Volume::VTable* gVolumeVTable[E_VOLUMETYPE_NUMINTERNALTYPES] = {};
    // The narrow phase is not reached here (no query runs); these only let VolumeQuery.cpp link.
    s32 GPInstanceBatchIntersect1xN(PrimitivePairIntersectResult*, s32, const GPInstance&, const GPInstance*, s32, f32) { return 0; }
    s32 GPInstanceBatchIntersectNx1(PrimitivePairIntersectResult*, s32, const GPInstance*, s32, const GPInstance&, f32) { return 0; }
#include "fxfu_fine.inc"
} }

static bool Inside(const void* lpFrom, size_t luBytes, const void* lpLow, size_t luRegion)
{
    const u8* lp = static_cast<const u8*>(lpFrom);
    const u8* lpLo = static_cast<const u8*>(lpLow);
    return lp >= lpLo && lp + luBytes <= lpLo + luRegion;
}

// The VolumeVolumeQuery built in place at lpBuffer: its descriptor(100, 100) and every carve it made -- the two
// bbox sub-queries, the result array and the (R + 1) GPInstance scratch -- lie inside [lpBuffer, lpBuffer + size).
bool FxVvqCarveInside(const void* lpQueryHandle, const void* lpBuffer, size_t luBufferSize, u32* lpuDescriptorOut)
{
    u32 lauDesc[12];
    VolumeVolumeQuery::GetResourceDescriptor(lauDesc, 100, 100);
    *lpuDescriptorOut = lauDesc[0];
    const VolumeVolumeQuery* lpVvq = static_cast<const VolumeVolumeQuery*>(lpQueryHandle);
    return lauDesc[0] <= luBufferSize
        && Inside(lpVvq->m_bBoxQueryAtoB, sizeof(VolumeBBoxQuery), lpBuffer, luBufferSize)
        && Inside(lpVvq->m_bBoxQueryBtoA, sizeof(VolumeBBoxQuery), lpBuffer, luBufferSize)
        && Inside(lpVvq->m_intersectionBuffer,
                  static_cast<size_t>(lpVvq->m_intersectionBufferMaxSize) * sizeof(PrimitivePairIntersectResult),
                  lpBuffer, luBufferSize)
        && Inside(lpVvq->m_instancingSPR, 101 * sizeof(GPInstance), lpBuffer, luBufferSize);
}

// The host record strides the VolumeLineQuery carve uses.
void FxVlqStrides(size_t* lpuVolRef, size_t* lpuVolume, size_t* lpuResult)
{
    *lpuVolRef = sizeof(VolRef);
    *lpuVolume = sizeof(Volume);
    *lpuResult = sizeof(VolumeLineSegIntersectResult);
}
