#include "GameSource/GameState/BrnGameActions.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cmath>   // std::fabs

namespace
{
// SoundTriggerAction::IsEmpty query-position tolerance. The X360 reads this float from rodata
// (0x82029BA4), which is not in the available exports; this is an UNCONFIRMED stand-in -- the
// |lane| > eps emptiness test is faithful, only the literal value is provisional.
const float KF_QUERY_POS_EPSILON = 1.0e-4f;
}

// CgsNetwork::K_INVALID_PLAYER_ID has no committed home (it lives in the un-reconstructed
// CgsNetworkConstants.h); modelled file-local as -1, exactly as the committed
// BrnGameStateFlybyManager.cpp does. The assert expression strings keep the original spelling.
namespace CgsNetwork
{
static const BrnNetwork::NetworkPlayerID K_INVALID_PLAYER_ID = -1;
}

namespace BrnGameState
{
namespace GameStateModuleIO
{
// X360 0x822A0250. True iff the requested reset position OR direction is non-zero. The X360
// spelled this with the rwmath VPU operator!=(Vector3,Vector3); the unused 4th (w) lane is zero
// in both operands, so this is a faithful 3-lane (V3) compare.
bool ResetPlayerCarAction::HasToChangeLocation() const
{
    if (mPosition.x != 0.0f || mPosition.y != 0.0f || mPosition.z != 0.0f)
    {
        return true;
    }
    return mDirection.x != 0.0f || mDirection.y != 0.0f || mDirection.z != 0.0f;
}

// X360 0x82355178. Empty iff the query position is (within tolerance) zero AND there is no
// entity, no result type, and no active-trigger bits.
bool SoundTriggerAction::IsEmpty()
{
    if (std::fabs(mQueryPos.x) > KF_QUERY_POS_EPSILON
        || std::fabs(mQueryPos.y) > KF_QUERY_POS_EPSILON
        || std::fabs(mQueryPos.z) > KF_QUERY_POS_EPSILON)
    {
        return false;
    }
    if (mEntityId.muValue != 0u)
    {
        return false;
    }
    if (meResultType != E_TYPE_INVALID)
    {
        return false;
    }
    if (muActiveTriggers != 0u)
    {
        return false;
    }
    return true;
}

// X360 0x8230FE98. Range-checked setter for the disconnected player's active-race-car slot.
void RemotePlayerDisconnectedAction::SetActiveRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    if (!((leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) &&
          (leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT)))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "( leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 ) && ( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )",
            "..\\..\\..\\GameSource\\GameState/BrnGameActions.h",
            6360);
        CgsDev::Assert::EndAssert();
    }

    meActiveRaceCarIndex = leActiveRaceCarIndex;
}

// X360 0x8230FF00. Store the disconnected player's network id (must not be the invalid sentinel).
void RemotePlayerDisconnectedAction::SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lPlayerID)
{
    CGS_ASSERT(lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
               "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    mPlayerID = lPlayerID;
}

// X360 0x82355088. Initialise a setup-network-car action payload (6 members). The three asserts
// bound the race-car index and reject the null model id (expression/file/line byte-exact).
void SetupNetworkCarAction::Construct(EPlayerScoringIndex lePlayerScoringIndex,
                                      EActiveRaceCarIndex leActiveRaceCarIndex,
                                      Vector3             lPos,
                                      Vector3             lAt,
                                      CgsID               lModelId,
                                      CgsID               lWheelModelId)
{
    if (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0",
            "..\\..\\..\\GameSource\\GameState/BrnGameActions.h",
            783);
        CgsDev::Assert::EndAssert();
    }
    if (leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_COUNT)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT",
            "..\\..\\..\\GameSource\\GameState/BrnGameActions.h",
            784);
        CgsDev::Assert::EndAssert();
    }
    if (lModelId == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lModelId != kCGSID_NULL",
            "..\\..\\..\\GameSource\\GameState/BrnGameActions.h",
            785);
        CgsDev::Assert::EndAssert();
    }

    mWorldSpacePosition  = lPos;
    mAt                  = lAt;
    mModelId             = lModelId;
    mWheelModelId        = lWheelModelId;
    meActiveRaceCarIndex = leActiveRaceCarIndex;
    mePlayerScoringIndex = lePlayerScoringIndex;
}

// X360 0x823551F0. Bounds-checked setter for the added player's scoring slot.
void OnlinePlayerAddedAction::SetPlayerScoringIndex(EPlayerScoringIndex lePlayerScoringIndex)
{
    CGS_ASSERT(
        (lePlayerScoringIndex >= E_PLAYER_SCORING_INDEX_0) &&
        (lePlayerScoringIndex <  E_PLAYER_SCORING_INDEX_COUNT),
        "(lePlayerScoringIndex >= E_PLAYER_SCORING_INDEX_0) && (lePlayerScoringIndex < E_PLAYER_SCORING_INDEX_COUNT)");

    mePlayerScoringIndex = lePlayerScoringIndex;
}
}
}
