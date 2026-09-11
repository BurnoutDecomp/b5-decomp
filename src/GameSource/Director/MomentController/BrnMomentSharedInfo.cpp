#include "GameSource/Director/MomentController/BrnMomentSharedInfo.h"

#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"   // GameState (the flags eight of these read)
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"       // AllVehicleData (the real home + class key)
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"      // NamedParameters (the gyro block run)

// ============================================================================
// GameSource/Director/MomentController/BrnMomentSharedInfo.cpp
//
// Compilation home for the `BrnDirector::detail::MomentSharedInfo_*` reach shims -- the
// free functions every Moments/*.cpp declares at its head to read the shared record through
// the `const void*` Moment::Update passes it. Each one is a single named member read; the
// record itself is BrnMomentSharedInfo.h.
//
// Only the shims the MOUNTED moment TUs need are bodied here. The rest of the family's
// declarations stay unresolved on purpose: a shim with a body is a claim about which member
// it lands on, and the remaining ones reach members whose role names have not been checked
// against the record (see the two corrections in the header banner for why that check is
// worth doing one at a time rather than in bulk).
//
// ⛔ DO NOT give these a quiet fallback for a null record. They are reached only from a
// moment's Update, which the director calls with its own record by reference; a null here
// would mean the tick is wired wrong, and a silent 0/false would hide it.
// ============================================================================

namespace BrnDirector
{
namespace detail
{
    namespace
    {
        // The one cast in the file. Every shim below goes through it, so the type erasure
        // Moment::Update imposes is undone in exactly one place.
        inline const MomentSharedInfo& Record(const void* lpSharedInfo)
        {
            return *static_cast<const MomentSharedInfo*>(lpSharedInfo);
        }
    }

    // ---- through mpGameState -------------------------------------------------------------

    // The player is mid-crash.
    bool MomentSharedInfo_IsPlayerCrashing(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpGameState->mbCrashActive;
    }

    // A takedown landed (the crash / tumbling / bystander moments' second trigger).
    bool MomentSharedInfo_WasTakedown(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpGameState->mbTakedownActive;
    }

    // See the header banner: this is the road-rage-totalled flag, which routes the crash to
    // the spiralling deathcam and so takes the bystander / tumbling shots off the table.
    bool MomentSharedInfo_IsCrashCameraBlocked(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpGameState->mbRoadRageTotalled;
    }

    // The "this crash goes straight to crash mode after the intro" flag -- while it is up the
    // crash replay the moment would be framing does not happen.
    bool MomentSharedInfo_IsCrashReplayDisabled(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpGameState->mbGoToCrashModeAfterIntro;
    }

    // The car the takedown was scored against -- the bystander and tumbling moments frame it.
    s32 MomentSharedInfo_GetTakedownVictimIndex(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpGameState->meTakedownVictimID;
    }

    // ---- direct record members -----------------------------------------------------------

    // See the header banner: the PLAYER's active race-car index, read under a crash name.
    s32 MomentSharedInfo_GetCrashVehicleIndex(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mePlayerActiveRaceCarIndex;
    }

    // The director's "start the collision policies now" override, which forces the bystander
    // and tumbling trigger conditions regardless of the crash/takedown tests.
    bool MomentSharedInfo_GetForceFlag1320(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mbForceCollisionPolicysToStart;
    }

    const AllVehicleData* MomentSharedInfo_GetAllVehicleData(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpAllVehicleData;
    }

    // The camera parameter record the tumbling moment picks its gyro block out of.
    const NamedParameters* MomentSharedInfo_GetNamedBehaviourParams(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpNamedBehaviourParams;
    }

    // ---- through the leading vehicle snapshot ----------------------------------------------
    // Both lanes live inside mPlayerInfo.mRaceCarState, which is why the moment family reads
    // them at a small fixed displacement off the record base rather than through a pointer.

    const rw::math::vpu::Vector3& MomentSharedInfo_GetPlayerVelocity(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mPlayerInfo.mRaceCarState.mLinearVelocity;
    }

    const rw::math::vpu::Vector3& MomentSharedInfo_GetPlayerAngularVelocity(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mPlayerInfo.mRaceCarState.mAngularVelocity;
    }
}
}
