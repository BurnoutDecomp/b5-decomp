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

// X360 0x8231BDF8. Clear the bit set, then set the remaining-bit for each checkpoint in
// [0, liNumCheckpoints). The X360 inlines BitArray<16>::UnSetAll (the leading *this = 0 of
// the single field) and BitArray<16>::SetBit per iteration; SetBit's inlined bounds check
// fires the dynamic CgsBitArray.h:222 assert ("Index: N, Number of bits: 16"), reduced here
// to the static expression matching the committed BrnGameActions.cpp convention. (The X360
// returns `this`; that is a calling-convention artifact, dropped for this void method.)
void CarCheckpointData::SetupCheckpoints(s32 liNumCheckpoints)
{
    mCheckpointsRemaining.UnSetAll();

    for (s32 liIndex = 0; liIndex < liNumCheckpoints; ++liIndex)
    {
        if (!(static_cast<u32>(liIndex) < KI_MAX_LANDMARKS_IN_MODE))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "luIndex < NUMBITS",
                "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsBitArray.h",
                222);
            CgsDev::Assert::EndAssert();
        }

        mCheckpointsRemaining.SetBit(static_cast<u32>(liIndex));
    }
}

// X360 0x8231BF90. Record that checkpoint liCheckpointIndex has been reached by clearing its
// remaining-bit. Guards: index in [0, KI_MAX_LANDMARKS_IN_MODE) (BrnGameStateSharedIO.h:1210/
// :1211), the bit must currently be set i.e. not already hit and in range
// (BrnGameStateSharedIO.h:1214 + the inlined IsBitSet bounds check CgsBitArray.h:203), then
// UnSetBit clears it (its inlined bounds check is CgsBitArray.h:241). The dynamic
// "invalid index : N < 16" StrStream messages are reduced to static expressions, matching the
// committed convention. (X360 returns `this`; void here.)
void CarCheckpointData::MarkCheckpointAsHit(s32 liCheckpointIndex)
{
    if (liCheckpointIndex < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liCheckpointIndex >= 0",
            "..\\..\\..\\GameSource\\GameState/BrnGameStateSharedIO.h",
            1210);
        CgsDev::Assert::EndAssert();
    }
    if (liCheckpointIndex >= KI_MAX_LANDMARKS_IN_MODE)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liCheckpointIndex < KI_MAX_LANDMARKS_IN_MODE",
            "..\\..\\..\\GameSource\\GameState/BrnGameStateSharedIO.h",
            1211);
        CgsDev::Assert::EndAssert();
    }

    if (!(static_cast<u32>(liCheckpointIndex) < KI_MAX_LANDMARKS_IN_MODE))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "luIndex < NUMBITS",
            "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsBitArray.h",
            203);
        CgsDev::Assert::EndAssert();
    }

    if (!mCheckpointsRemaining.IsBitSet(static_cast<u32>(liCheckpointIndex)))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "Checkpoint either out of range or already hit!",
            "..\\..\\..\\GameSource\\GameState/BrnGameStateSharedIO.h",
            1214);
        CgsDev::Assert::EndAssert();
    }

    if (!(static_cast<u32>(liCheckpointIndex) < KI_MAX_LANDMARKS_IN_MODE))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "luIndex < NUMBITS",
            "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsBitArray.h",
            241);
        CgsDev::Assert::EndAssert();
    }

    mCheckpointsRemaining.UnSetBit(static_cast<u32>(liCheckpointIndex));
}

// X360 0x823261D0. Return the lowest-indexed remaining checkpoint, or -1 if none remain.
// The X360 prologue is a SWAR population count of the single u64 field; if it is zero the
// "No checkpoints set" assert fires (BrnGameStateSharedIO.h:1256). It then scans for the
// lowest set bit (BitArray<16>::GetFirstNonZeroBit -> the (fieldBits - clz64(field) + 63)
// idiom), clamping an out-of-range result to -1.
s32 CarCheckpointData::GetNextCheckpointIndex() const
{
    if (mCheckpointsRemaining.IsZero())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "No checkpoints set",
            "..\\..\\..\\GameSource\\GameState/BrnGameStateSharedIO.h",
            1256);
        CgsDev::Assert::EndAssert();
    }

    const s32 liNextCheckpoint = mCheckpointsRemaining.GetFirstNonZeroBit();
    if (liNextCheckpoint >= KI_MAX_LANDMARKS_IN_MODE)
    {
        return -1;
    }
    return liNextCheckpoint;
}

// X360 0x823C4D50. Walk every set bit in ascending order, writing its index into
// lpaiCheckpointIndexes, and return the count written. The X360 inlines
// BitArray<16>::GetFirstNonZeroBit for the seed and GetNextNonZeroBit for each step (the
// inner IsBitSet bounds check is the CgsBitArray.h:203 'invalid index' assert); both stop at
// -1, which ends the walk.
s32 CarCheckpointData::GetAllRemainingCheckpointIndexes(s32* lpaiCheckpointIndexes) const
{
    s32 liCount = 0;

    for (s32 liIndex = mCheckpointsRemaining.GetFirstNonZeroBit();
         liIndex >= 0;
         liIndex = mCheckpointsRemaining.GetNextNonZeroBit(liIndex))
    {
        lpaiCheckpointIndexes[liCount] = liIndex;
        ++liCount;
    }

    return liCount;
}

// ===== Forked slices from the GameStateModuleIO TU =====
// TODO(conductor-review): reuse committed home / DWARF member names (FlybyData -> mRivalsToShow /
// GetCarFlybyData; OnlineGameResults -> BrnGameActions.h GameAction<E_ACTION_ONLINE_GAME_RESULT>).

// X360 0x821F2A90. Indexed accessor for one of the (KI_MAX_CARS_IN_FLYBY == 3) per-rival records.
// The X360 body bounds-checks liRivalIndex with two verbatim asserts (BrnGameStateSharedIO.h
// :1110/:1111) then returns &maFlybyRivalData[liRivalIndex] (this + 4 + 196 * liRivalIndex).
FlybyRivalData* FlybyData::GetFlybyRivalData(s32 liRivalIndex)
{
    if (liRivalIndex < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liRivalIndex >= 0",
            "..\\..\\..\\GameSource\\GameState/BrnGameStateSharedIO.h",
            1110);
        CgsDev::Assert::EndAssert();
    }
    if (liRivalIndex >= FlybyRivalData::KI_MAX_CARS_IN_FLYBY)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liRivalIndex < FlybyRivalData::KI_MAX_CARS_IN_FLYBY",
            "..\\..\\..\\GameSource\\GameState/BrnGameStateSharedIO.h",
            1111);
        CgsDev::Assert::EndAssert();
    }
    return &maFlybyRivalData[liRivalIndex];
}

// X360 0x821F2B08. Free predicate over EGameModeType. Returns true for exactly two mode values --
// E_MODE_ONLINE_FREE_BURN_LOBBY (15) and E_MODE_ONLINE_SHOWTIME (16). FAITHFUL QUIRK: despite the
// name, the online-showtime mode is also treated as a free-burn lobby (the X360 build groups them).
bool IsOnlineFreeBurnLobby(EGameModeType leGameMode)
{
    return (leGameMode == E_MODE_ONLINE_FREE_BURN_LOBBY)
        || (leGameMode == E_MODE_ONLINE_SHOWTIME);
}

// X360 0x82311C30. Copy-assign an OnlineGameResults (260-byte payload == 65 u32 words). The X360
// body copies word 0, SKIPS the word at offset 4 (index 1), then copies words 2..64 contiguously.
// FAITHFUL QUIRK: the field at +0x04 is intentionally NOT copied by the X360 build, so the gap is
// preserved byte-for-byte.
OnlineGameResults& OnlineGameResults::operator=(const OnlineGameResults& lOther)
{
    mauWords[0] = lOther.mauWords[0];
    // mauWords[1] (offset 0x04) deliberately NOT copied by the X360 build.
    for (u32 luWord = 2; luWord < KU_NUM_WORDS; ++luWord)
    {
        mauWords[luWord] = lOther.mauWords[luWord];
    }
    return *this;
}
}
}
