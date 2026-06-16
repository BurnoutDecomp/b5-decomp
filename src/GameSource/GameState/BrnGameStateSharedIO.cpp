#include "GameSource/GameState/BrnGameStateSharedIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsNetwork::K_INVALID_PLAYER_ID has no committed home; file-local -1 (the FlybyManager precedent).
namespace CgsNetwork
{
static const BrnNetwork::NetworkPlayerID K_INVALID_PLAYER_ID = -1;
}

namespace BrnGameState
{
namespace GameStateModuleIO
{
// X360 0x82356EA0. Zero the score-bearing words of a StuntScoreInfo, leaving the muReservedNN
// gap words intact (the X360 body is explicit per-word stores with gaps -- NOT a memset). The
// trailing `return this` is a register artifact of a void function and is dropped.
void StuntScoreInfo::Clear()
{
    muWord00 = 0;
    muWord01 = 0;
    muWord02 = 0;
    muWord03 = 0;
    muWord04 = 0;
    muWord05 = 0;
    muWord06 = 0;
    // muReserved07 intentionally NOT cleared by the X360 build.
    muWord08 = 0;
    // muReserved09 intentionally NOT cleared.
    muWord10 = 0;
    // muReserved11 intentionally NOT cleared.
    muWord12 = 0;
    // muReserved13 intentionally NOT cleared.
    muWord14 = 0;
    muWord15 = 0;
    muWord16 = 0;
    muWord17 = 0;
    muWord18 = 0;
    // muReserved19 intentionally NOT cleared.
    muWord20 = 0;
    // muReserved21..23 intentionally NOT cleared.
    muWord24 = 0;
}

// X360 0x82354340. Initialise one game-mode event: store the event id, traffic-light trigger id
// and landmark count, then copy the first liNumLandmarks LandmarkIndex values from the source
// array. The bounds assert keeps liNumLandmarks within the fixed 16-slot array.
void SpecificGameModeEventInterface::Event::Construct(
    s32            liEventID,
    u32            luTrafficLightTriggerId,
    LandmarkIndex* lpaLandmarkIndices,
    s32            liNumLandmarks)
{
    CGS_ASSERT(liNumLandmarks <= KI_MAX_LANDMARKS_IN_MODE,
               "liNumLandmarks <= KI_MAX_LANDMARKS_IN_MODE");

    miEventID              = liEventID;
    mTrafficLightTriggerId = luTrafficLightTriggerId;
    miNumLandmarks         = liNumLandmarks;

    for (s32 liIndex = 0; liIndex < miNumLandmarks; ++liIndex)
    {
        maLandmarkIndices[liIndex] = lpaLandmarkIndices[liIndex];
    }
}

// X360 0x82326360. Initialise the freeburn "every player" completion block: zero each of the 7
// per-player slots' completion bits + invalidate its player id, then zero the local-player bits.
void FburnChallengeEveryPlayerStatusData::Construct()
{
    for (s32 liSlot = 0; liSlot < KI_NUM_PLAYER_SLOTS; ++liSlot)
    {
        CompletedChallenges& lSlot = maCompletedChallenges[liSlot];
        for (u32 luWord = 0; luWord < CompletedFburnChallenges::KU_NUM_BIT_WORDS; ++luWord)
        {
            lSlot.mCompletedChallenges.maxBits[luWord] = 0;
        }
        lSlot.mPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
    }

    for (u32 luWord = 0; luWord < CompletedFburnChallenges::KU_NUM_BIT_WORDS; ++luWord)
    {
        mLocalChallengeCompletionData.maxBits[luWord] = 0;
    }
}

// X360 0x823263C8. Record one remote player's completion entry in the first free slot (free ==
// mPlayerID sentinel). All 7 full -> fire the "No room" assert (verbatim file/line) and bail.
// FAITHFUL QUIRK: the X360 body zeroes the slot's bit-array rather than copying lpCompletedChallenges
// (the source param is dead); only the player id is stored. Void; return-result dropped.
void FburnChallengeEveryPlayerStatusData::AddCompletionStatus(
    const CompletedFburnChallenges* /*lpCompletedChallenges*/, BrnNetwork::NetworkPlayerID lPlayerID)
{
    s32 liIndex = 0;
    while (maCompletedChallenges[liIndex].mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID)
    {
        ++liIndex;
        if (liIndex >= KI_NUM_PLAYER_SLOTS)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("No room for completion status data\n",
                                       "..\\..\\..\\GameSource\\GameState/BrnGameStateSharedIO.h", 2063);
            CgsDev::Assert::EndAssert();
            return;
        }
    }

    for (u32 luWord = 0; luWord < CompletedFburnChallenges::KU_NUM_BIT_WORDS; ++luWord)
    {
        maCompletedChallenges[liIndex].mCompletedChallenges.maxBits[luWord] = 0;
    }
    maCompletedChallenges[liIndex].mPlayerID = lPlayerID;
}
}
}
