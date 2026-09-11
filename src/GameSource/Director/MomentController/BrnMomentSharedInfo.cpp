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
// Compilation home for the `BrnDirector::detail::MomentSharedInfo_*` reach shims -- the
// free functions every Moments/*.cpp declares at its head to read the shared record through
// the `const void*` Moment::Update passes it. Each one is a single named member read; the
// record itself is BrnMomentSharedInfo.h.
//
// A shim with a body is a claim about which member it lands on, so every one below was
// checked twice: the displacement each call site recorded against an offsetof probe of the
// record's own sub-structs (GameState, RaceCarState, PlayerCrashInfo -- all three have no
// pointer members past their head, so their host layout is the record's), and the role name
// the call site coined against the member that displacement lands on. NINE names lost that
// second check and are listed in the header banner; the member each one reads is named in
// its comment here.
//
// The shims still WITHOUT a body are the resolved-vehicle lanes the takedown look-back reads
// through VehicleRef::Get. The two the census called uncarved are bodied here: the crash-type
// word is a named VehicleTracker member with its own published accessor, and the profile-data
// flag is read through the same byte blob two mounted arbitrator arms already read it through.
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

    // The takedown look-back's three player-frame lanes, all off the SAME leading snapshot.
    // Its own names for the first two hold up; the third does not (see the header banner).
    const rw::math::vpu::Vector3& MomentSharedInfo_GetForward(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mPlayerInfo.mRaceCarState.mTransform.zAxis;
    }

    const rw::math::vpu::Vector3& MomentSharedInfo_GetPosition(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mPlayerInfo.mRaceCarState.mTransform.wAxis;
    }

    // NOT a second position: the same linear-velocity lane GetPlayerVelocity returns. The
    // look-back's "projection" is therefore -dot(fwd, dPos) / dot(fwd, dVel) -- a time to
    // alignment in seconds, which is what its [0,1] band tests.
    const rw::math::vpu::Vector3& MomentSharedInfo_GetSegmentReference(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mPlayerInfo.mRaceCarState.mLinearVelocity;
    }

    // The hit-traffic moment's trigger byte: the player's crash flag, read off the leading
    // snapshot rather than through mpPlayerCar (which is where GetDynamicsAbort1098 below
    // reads the identical member).
    bool MomentSharedInfo_IsHitTrafficConditionActive(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mPlayerInfo.mRaceCarState.mbCrashing;
    }

    // ---- further reads through mpGameState -------------------------------------------------

    // The takedown look-back's search gate. Same flag as WasTakedown above -- it says a
    // takedown happened, not that a victim slot is filled.
    bool MomentSharedInfo_HasTakedownVictim(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpGameState->mbTakedownActive;
    }

    // Same slot as GetTakedownVictimIndex; the look-back binds its VehicleRef to it.
    s32 MomentSharedInfo_GetVictimRaceCarIndex(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpGameState->meTakedownVictimID;
    }

    // This frame's action-flag word, which the stunt moment reads bitwise.
    u32 MomentSharedInfo_GetStuntFlags(const void* lpSharedInfo)
    {
        return static_cast<u32>(Record(lpSharedInfo).mpGameState->miThisFramesActionFlags);
    }

    // The action-requested camera field the stunt moment gates on (> 0) and whose low word
    // it uses as the staged take number.
    s64 MomentSharedInfo_GetStuntTakeField(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpGameState->miActionRequestedCamera;
    }

    // Crash time REMAINING, not elapsed -- the stationary-crash moment compares the ICE
    // take's length against it.
    f32 MomentSharedInfo_GetTimeCrashing(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpGameState->mfCrashTimeRemaining;
    }

    // The showtime-intro flag inside the opaque DirectorProfileData sub-object, at its +0x05.
    // Two MOUNTED consumers already read this exact byte through the same blob and gate on it
    // the same way -- ArbStateRoaming::Update FORCES the external chase cam while it is up, and
    // the same state's showtime-intro border/blur ramp runs only while it is up. So the stunt
    // moment's use of it is an abort, not a third meaning: the stunt camera stands down while
    // the showtime intro owns the shot. The sub-object stays opaque -- the recorded field layout
    // for it does not line up with the displacements its own users read, as its home header
    // explains -- so this reads the byte blob exactly as those two do rather than inventing one.
    bool MomentSharedInfo_IsShowtimeIntroActive(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpGameState->mDirectorProfileData.maOpaque[0x05] != 0;
    }

    // The slow-motion permission flag, not a camera-active flag.
    bool MomentSharedInfo_IsCrashCameraActive(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpGameState->mbCanUseSlomo;
    }

    // The event type -- crash mode is one of its enumerators, which is why the call site
    // reads it as a "crash mode" word.
    s32 MomentSharedInfo_GetCrashModeWord(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpGameState->meEventType;
    }

    bool MomentSharedInfo_IsNewCarJoining(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpGameState->mbNewCarAdded;
    }

    s32 MomentSharedInfo_GetNewCarJoinTargetIndex(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpGameState->meAddedCarID;
    }

    // ---- the rest of the record's own pointers and scalars ---------------------------------

    CgsNumeric::Random* MomentSharedInfo_GetRandom(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpRandom;
    }

    DebugLog* MomentSharedInfo_GetDebugLog(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpDebugLog;
    }

    // The "world" the look-back's VehicleRef resolves against IS the all-vehicle block.
    const void* MomentSharedInfo_GetWorld(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpAllVehicleData;
    }

    // The two timesteps are distinct members and the family reads both: the frame step and
    // the simulation step.
    f32 MomentSharedInfo_GetFrameTimestep(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mfTimestep;
    }

    f32 MomentSharedInfo_GetTimestep(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mfSimTimestep;
    }

    bool MomentSharedInfo_GetJumpEnableFlag1316(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mbAllowJumpMoment;
    }

    bool MomentSharedInfo_GetStuntEnableFlag1317(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mbAllowStuntMoment;
    }

    bool MomentSharedInfo_GetFlag1318(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mbAllowHardStopMoment;
    }

    bool MomentSharedInfo_GetFlag1319(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mbForceNextWorldCrashToBeFastTopDown;
    }

    const DirectorResourceManager* MomentSharedInfo_GetDirectorResourceManager(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpDirectorResourceManager;
    }

    // The call site takes it non-const (the hard-stop moment drives the selector through it);
    // the record holds the slot const, so the cast is here rather than at every use.
    ShotSelector* MomentSharedInfo_GetShotSelector(const void* lpSharedInfo)
    {
        return const_cast<ShotSelector*>(Record(lpSharedInfo).mpShotSelector);
    }

    const CrashAnalysis* MomentSharedInfo_GetCrashAnalysis(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpCrashAnalysis;
    }

    const EffectInterface* MomentSharedInfo_GetEffectInterface(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpEffectInterface;
    }

    // ---- through mpPlayerTracker -----------------------------------------------------------

    // This crash's energy classification. The call site recorded a displacement into the tracker
    // and the census called it "a word this tree has not carved" -- it is carved, and has been:
    // VehicleTracker names that exact slot meCrashType and already publishes GetCrashType(), so
    // this is one named read like the rest of the family, not a struct job.
    s32 MomentSharedInfo_GetCurrentCrashType(const void* lpSharedInfo)
    {
        return static_cast<s32>(Record(lpSharedInfo).mpPlayerTracker->GetCrashType());
    }

    // ---- through mpPlayerCar ---------------------------------------------------------------
    // The same snapshot type as the leading copy, reached by pointer. The stationary-crash
    // moment dots the first lane against the other two.

    // The car's own forward axis, which is the axis a barrel roll turns about -- so the call
    // site's name holds up even though what it reads is a transform row.
    const rw::math::vpu::Vector3& MomentSharedInfo_GetCrashRotationAxis(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpPlayerCar->mRaceCarState.mTransform.zAxis;
    }

    const rw::math::vpu::Vector3& MomentSharedInfo_GetLinearVelocity(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpPlayerCar->mRaceCarState.mLinearVelocity;
    }

    const rw::math::vpu::Vector3& MomentSharedInfo_GetAngularVelocity(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpPlayerCar->mRaceCarState.mAngularVelocity;
    }

    // TIME in the air, not height -- the stunt moment's "0 == grounded" reading is right,
    // the unit in the name is not.
    f32 MomentSharedInfo_GetAirborneHeight(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpPlayerCar->mRaceCarState.mfTimeInAir;
    }

    // The same crash flag IsHitTrafficConditionActive reads off the leading snapshot.
    bool MomentSharedInfo_GetDynamicsAbort1098(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpPlayerCar->mRaceCarState.mbCrashing;
    }

    // The car's own up axis, second lane -- i.e. how upright the car still is, which is what
    // the hard-stop moment's single-lane compare tests. Nothing to do with speed.
    f32 MomentSharedInfo_GetCrashSpeedDiff(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpPlayerCar->mRaceCarState.mTransform.yAxis.y;
    }

    // The above-ground probe's distance and its valid flag. Neither is about the crash; the
    // two call-site names are the family's worst pair.
    f32 MomentSharedInfo_GetCrashElapsed(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpPlayerCar->mRaceCarState
                   .mAboveGroundTestResult.mfVerticalDistance;
    }

    bool MomentSharedInfo_HasCrashDynamics(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpPlayerCar->mRaceCarState
                   .mAboveGroundTestResult.mbValid;
    }

    // ---- through mpPlayerCrashInfo ---------------------------------------------------------
    // Three consecutive bools of the crash analysis block, numbered by the call sites.

    bool MomentSharedInfo_GetCrashFlag36(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpPlayerCrashInfo->mbHardstopVsWall;
    }

    bool MomentSharedInfo_GetCrashFlag37(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpPlayerCrashInfo->mbHardStopVsAI;
    }

    bool MomentSharedInfo_GetCrashFlag39(const void* lpSharedInfo)
    {
        return Record(lpSharedInfo).mpPlayerCrashInfo->mbHitWater;
    }

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
