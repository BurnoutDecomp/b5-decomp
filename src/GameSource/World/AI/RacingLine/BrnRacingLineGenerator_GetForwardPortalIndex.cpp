// =================================================================================================
// RacingLine/BrnRacingLineGenerator_GetForwardPortalIndex.cpp  (aiwave A5, 2026-09-03)
//
//   BrnAI::RacingLineGenerator::GetForwardPortalIndex @0x827817F8  (186 insns, VMX128)
//   DWARF BrnRacingLineGenerator.h:79:
//     static uint8_t GetForwardPortalIndex(const AISectionsData*, const AISection*, Vector2, Vector2)
//
// Partfile of RacingLine/BrnRacingLineGenerator.cpp (which bodies SetupSectionExit and
// DropHardNoGoLinesIntoMap only). Split out because the committed BrnRacingLineGenerator.h
// declares neither this member nor the three Extrapolate* siblings, and that header is not this
// lane's file -- see the header_request below.
//
// ⚠️ [FLAG header_request] The class is declared TU-LOCALLY here with ONLY this static member,
// exactly the idiom BrnRouteRequestManager.cpp already uses for the same function (its sole
// caller: ComputeSectionBehind @0x827892C0). The real declaration belongs in
// RacingLine/BrnRacingLineGenerator.h:
//     static u8 GetForwardPortalIndex(const AISectionsData* lpAISectionData,
//                                     const AISection* lpAISection,
//                                     Vector2 lCarPosition, Vector2 lCarDirection);
// DELETE-WHEN that lands: replace the local declaration with `#include
// "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"` here and in the manager TU.
//
// Register map (X360): r3 = lpAISectionData (never read), r28 = r4 = lpAISection,
// v122 = v1 = lCarPosition, v126 = v2 = lCarDirection (the DWARF order; ComputeSectionBehind
// @0x82789318..0x82789334 builds v1 = Flatten(GetPosition()), v2 = Flatten(-GetDirection())).
//
//   0x8278182C..0x827818F8  assert(!RwMath::IsZero(lCarDirection))  BrnRacingLineGenerator.cpp:2640
//                           -- the `vandc` (abs) / `vcmpgtfp` vs flt_820C3B70 lane 0 (FLT_EPSILON,
//                           read from the image) pair per lane: zero <=> |x| <= eps && |y| <= eps
//   0x827818FC..0x82781958  v123 = lCarDirection * rsqrt(x*x + y*y)   (vrsqrtefp + two
//                           Newton-Raphson refinements == an exact normalise here)
//   0x82781854              f31 = flt_820C4358 == -2.0f   (best-dot seed)
//   0x82781904/0x82781ABC   for (i = 0; i < lpAISection->mu8NumPortals (+0x14); ++i)
//   0x82781984                portal = lpAISection->GetPortal(i)
//   0x827819B8..0x827819C4    v126 = (portal.x, portal.z)   (vrlimi lane packs of the Vector3)
//   0x827819D0                d = v126 - lCarPosition
//   0x827819D4..0x82781A44    if (IsZero(d)) continue      (same eps idiom, both lanes)
//   0x82781A48..0x82781A94    dN = d * rsqrt(d.x*d.x + d.y*d.y) ; dot = dN . v123
//   0x82781AA8..0x82781AB8    if (dot > best) { best = dot ; result = i }
//   0x82781ACC              return result   (0 when no portal beat the seed)
// =================================================================================================

#include <cmath>

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // Vector2 (rw vpu; .x/.y)
#include "SharedClasses/AI/AISectionsResourceType.h"        // BrnAI::AISection / AISectionsData
#include "GameSource/World/AI/BrnAIPortal.h"                // BrnAI::Portal (GetPositionX/Z)
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT

namespace BrnAI
{
// [FLAG header_request] -- see the banner.
class RacingLineGenerator
{
public:
    static u8 GetForwardPortalIndex(const AISectionsData* lpAISectionData,
                                    const AISection* lpAISection,
                                    Vector2 lCarPosition, Vector2 lCarDirection);
};

namespace
{
    // flt_820C3B70 lane 0 == 0x34000000 == FLT_EPSILON (image bytes, 2026-09-03). The
    // RwMath::IsZero epsilon every VMX user of that address splats (`lvlx ; vspltw ..,0`).
    const f32 KF_ZERO_EPSILON = 1.1920929e-07f;

    // flt_820C4358 == -2.0f: the best-dot seed (below any unit dot product).
    const f32 KF_BEST_DOT_SEED = -2.0f;

    inline bool IsZero2D(f32 lfX, f32 lfY)
    {
        return !(fabsf(lfX) > KF_ZERO_EPSILON) && !(fabsf(lfY) > KF_ZERO_EPSILON);
    }
}

u8 RacingLineGenerator::GetForwardPortalIndex(const AISectionsData* lpAISectionData,
                                              const AISection* lpAISection,
                                              Vector2 lCarPosition, Vector2 lCarDirection)
{
    (void)lpAISectionData;   // r3: passed by every caller, read by nothing in the body

    CGS_ASSERT(!IsZero2D(lCarDirection.x, lCarDirection.y),
               "!RwMath::IsZero(lCarDirection)");   // BrnRacingLineGenerator.cpp:2640

    // The console normalises unconditionally after the (non-gating) assert; a zero direction
    // yields NaN lanes, every dot compare then fails and portal 0 is returned. Same here.
    const f32 lfDirectionInvLength =
        1.0f / sqrtf((lCarDirection.x * lCarDirection.x) + (lCarDirection.y * lCarDirection.y));
    const f32 lfDirectionX = lCarDirection.x * lfDirectionInvLength;
    const f32 lfDirectionY = lCarDirection.y * lfDirectionInvLength;

    u8  luBestPortalIndex = 0;
    f32 lfBestDot         = KF_BEST_DOT_SEED;

    const u8 luNumPortals = lpAISection->mu8NumPortals;
    for (u8 luPortalIndex = 0; luPortalIndex < luNumPortals; ++luPortalIndex)
    {
        const Portal* lpPortal = lpAISection->GetPortal(luPortalIndex);

        // Flattened portal position (x, z) minus the car's flattened position.
        const f32 lfDeltaX = lpPortal->GetPositionX() - lCarPosition.x;
        const f32 lfDeltaY = lpPortal->GetPositionZ() - lCarPosition.y;

        if (IsZero2D(lfDeltaX, lfDeltaY))
        {
            continue;
        }

        const f32 lfDeltaInvLength = 1.0f / sqrtf((lfDeltaX * lfDeltaX) + (lfDeltaY * lfDeltaY));
        const f32 lfDot = (lfDeltaX * lfDeltaInvLength * lfDirectionX)
                        + (lfDeltaY * lfDeltaInvLength * lfDirectionY);

        if (lfDot > lfBestDot)
        {
            lfBestDot         = lfDot;
            luBestPortalIndex = luPortalIndex;
        }
    }

    return luBestPortalIndex;
}
}
