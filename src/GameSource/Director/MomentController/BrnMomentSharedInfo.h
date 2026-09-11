#ifndef GAMESOURCE_DIRECTOR_MOMENTCONTROLLER_BRN_MOMENT_SHARED_INFO_H
#define GAMESOURCE_DIRECTOR_MOMENTCONTROLLER_BRN_MOMENT_SHARED_INFO_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                           // Matrix44Affine
#include "GameSource/BurnoutConstants.h"                              // EActiveRaceCarIndex
#include "GameShared/GameClasses/Containers/CgsBitArray.h"            // BitArray<tuNumBits>
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"        // Camera::VehicleInfo

// ============================================================================
// GameSource/Director/MomentController/BrnMomentSharedInfo.h
//
// BrnDirector::MomentSharedInfo -- the per-frame context the director hands to every moment's
// Update. It is one copy of the player's vehicle snapshot followed by the pointers and gates a
// moment needs to decide whether its shot is on: which cars are live, what the game state is
// doing, the camera parameter banks, and the crash/takedown analysis.
//
// The whole moment family reaches this record through free-function shims declared at the head
// of each Moments/*.cpp (`detail::MomentSharedInfo_*`), because Moment::Update's third argument
// is type-erased to `const void*` in this tree. That erasure is the reason the record had no
// home for so long -- nothing forced it to exist. The shims are bodied against THIS definition
// in BrnMomentSharedInfo.cpp; when the family's Update signature is un-erased they collapse
// into ordinary member reads and this header is what they read.
//
// Member names, types and ORDER are the record's own. Offsets are NOT reproduced and must not
// be: the console build has 4-byte pointers and this host has 8, so every member past the
// leading vehicle snapshot diverges by construction. Parity is BY NAMED MEMBER -- the same rule
// the arbitrator state container and the camera parameter bank state for their sub-objects.
//
// ⚠ NOTHING BUILDS ONE YET. The producer is MainDirector's moment tick, which has no body in
// this tree (MomentController::UpdateAllMoments and MainDirector::UpdateMoments are both
// unbodied and the call is gated out of MainDirector::Update), so no moment's Update runs and
// no shim below is reachable at runtime. This header closes the READ end only.
//
// ⓘ TWO SHIM NAMES DO NOT MATCH THE MEMBER THEY LAND ON, and the shim names are the ones that
// are wrong -- they were coined from each call site's role before the record existed:
//   MomentSharedInfo_GetCrashVehicleIndex  reads mePlayerActiveRaceCarIndex, which is the
//       PLAYER's active race-car index, not the crashing car's. Three moment TUs read it under
//       the crash name; on the paths that reach it the player IS the crashing car, so the uses
//       are right even though the name is not.
//   MomentSharedInfo_IsCrashCameraBlocked  reads GameState::mbRoadRageTotalled. That flag does
//       block the bystander/tumbling crash shot, but for one specific reason: a totalled
//       road-rage car is the spiralling deathcam's, which ArbStateCrashing::Prepare selects off
//       the same flag.
// Renaming them is a separate pass across nine TUs and is deliberately not done here.
// ============================================================================

namespace CgsNumeric { class Random; }

namespace BrnDirector
{
    struct GameState;
    struct AllVehicleData;
    class  DebugLog;
    class  DebugPrinter;
    class  VehicleTracker;
    class  DirectorResourceManager;
    class  ShotSelector;
    class  CrashAnalysis;
    class  EffectInterface;
    struct NamedParameters;

    namespace Camera
    {
        class BehaviourParameterBank;
    }

    struct MomentSharedInfo
    {
        // The record's own nested interface name for the contact spy the moments poll.
        class ContactSpyInterface;

        // The player's vehicle snapshot, BY VALUE and first -- which is why the moment family's
        // "direct" reaches (the tumbling moment's linear/angular velocity lanes) land inside
        // Camera::VehicleInfo::mRaceCarState rather than on a record member of their own.
        Camera::VehicleInfo         mPlayerInfo;

        // Which active race-car slots are live this frame.
        CgsContainers::BitArray<8>  mUsedRaceCars;

        CgsNumeric::Random*         mpRandom;

        DebugLog*                   mpDebugLog;
        DebugPrinter*               mpDebugPrinter;

        GameState*                  mpGameState;

        const AllVehicleData*       mpAllVehicleData;

        const Camera::VehicleInfo*  mpPlayerCar;
        const Camera::VehicleInfo*  mpRaceCars;
        const Matrix44Affine*       mpPlayerCarTransform;
        EActiveRaceCarIndex         mePlayerActiveRaceCarIndex;

        f32                         mfTimestep;
        f32                         mfSimTimestep;

        // The three per-moment-family eligibility gates and the two force flags.
        bool                        mbAllowJumpMoment;
        bool                        mbAllowStuntMoment;
        bool                        mbAllowHardStopMoment;
        bool                        mbForceNextWorldCrashToBeFastTopDown;
        bool                        mbForceCollisionPolicysToStart;

        const Camera::BehaviourParameterBank* mpBehaviourParameterBank;
        const NamedParameters*                mpNamedBehaviourParams;

        const VehicleTracker*       mpPlayerTracker;
        const ContactSpyInterface*  mpContacts;

        const DirectorResourceManager* mpDirectorResourceManager;
        const ShotSelector*            mpShotSelector;
        const CrashAnalysis*           mpCrashAnalysis;
        const EffectInterface*         mpEffectInterface;

        const Camera::PlayerCrashInfo* mpPlayerCrashInfo;
    };
}

#endif // GAMESOURCE_DIRECTOR_MOMENTCONTROLLER_BRN_MOMENT_SHARED_INFO_H
