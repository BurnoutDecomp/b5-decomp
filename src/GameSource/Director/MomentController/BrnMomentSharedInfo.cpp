#include "GameSource/Director/MomentController/BrnMomentSharedInfo.h"

#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"   // GameState (the flags eight of these read)
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"       // AllVehicleData (the real home + class key)
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h"       // VehicleTracker::GetCrashType
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"      // NamedParameters (the gyro block run)
#include "SDKs/Packages/ICE/ICEData.hpp"                               // ICE::ICETakeData (the take reaches)
#include "GameSource/AttribSys/Generated/classes/iceanim.h"            // Attrib::Gen::iceanim (the shot-element guid)

// ============================================================================
// GameSource/Director/MomentController/BrnMomentSharedInfo.cpp
//
// The MomentSharedInfo_* reach shims are gone: Moment::Prepare/Update take the DWARF types again
// (BrnMoment.h:174/180 -- BehaviourManager&, const MomentSharedInfo&), so every moment reads the
// record by member, inline, as ARTIST does. What is left here are the four ICE / shot helpers the
// stationary-crash and stunt moments still call.
// ============================================================================

namespace BrnDirector
{
namespace detail
{
    // ---- the ICE take reaches the stationary-crash and stunt moments make ------------------
    // Not record reads: the moment family declares these beside its record shims because the
    // ICE take fields are private to the ICE data surface. Each is the accessor that surface
    // already publishes.

    f32 ICETakeData_GetDuration(const ICE::ICETakeData* lpTakeData)
    {
        return lpTakeData->GetLength();
    }

    s32 ICETakeData_GetGuid(const ICE::ICETakeData* lpTakeData)
    {
        return lpTakeData->GetGuid();
    }

    // Is this selected shot an iceanim block. The console compares the reference's leading
    // 8 bytes against the generated iceanim class key and, only then, turns the allocated
    // behaviour's collision policy on -- so the tag test is what proves the behaviour the
    // shot-taking NewBehaviour handed back is an ICE-anim one. Both halves of that comparison
    // are already published by name (RefSpec::GetClassKey / iceanim::ClassKey); nothing here
    // reads a raw offset.
    bool ShotReference_IsIceAnimClassKeyTagged(const Attrib::RefSpec* lpShot)
    {
        return lpShot->GetClassKey() == static_cast<u64>(Attrib::Gen::iceanim::ClassKey());
    }

    // A ShotList element is a RefSpec; the take guid is read by building the generated
    // iceanim over it, which is what the behaviour that consumes the same element does.
    s32 IceAnimShotData_GetAnimGuid(const void* lpShotData)
    {
        Attrib::Gen::iceanim lIceAnim(*static_cast<const Attrib::RefSpec*>(lpShotData), 0);
        return lIceAnim.GetAnimGuid();
    }
}
}
