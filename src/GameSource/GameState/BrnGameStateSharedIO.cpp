#include "GameSource/GameState/BrnGameStateSharedIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream (AddMessage dynamic assert text)
#include <cstring>  // memcpy, memset, strncpy, strlen

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
            CGS_ASSERT(false, "No room for completion status data\n");
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
        CGS_ASSERT(static_cast<u32>(liIndex) < KI_MAX_LANDMARKS_IN_MODE, "luIndex < NUMBITS");

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
    CGS_ASSERT(liCheckpointIndex >= 0, "liCheckpointIndex >= 0");
    CGS_ASSERT(liCheckpointIndex < KI_MAX_LANDMARKS_IN_MODE, "liCheckpointIndex < KI_MAX_LANDMARKS_IN_MODE");

    CGS_ASSERT(static_cast<u32>(liCheckpointIndex) < KI_MAX_LANDMARKS_IN_MODE, "luIndex < NUMBITS");

    CGS_ASSERT(mCheckpointsRemaining.IsBitSet(static_cast<u32>(liCheckpointIndex)), "Checkpoint either out of range or already hit!");

    CGS_ASSERT(static_cast<u32>(liCheckpointIndex) < KI_MAX_LANDMARKS_IN_MODE, "luIndex < NUMBITS");

    mCheckpointsRemaining.UnSetBit(static_cast<u32>(liCheckpointIndex));
}

// X360 0x823261D0. Return the lowest-indexed remaining checkpoint, or -1 if none remain.
// The X360 prologue is a SWAR population count of the single u64 field; if it is zero the
// "No checkpoints set" assert fires (BrnGameStateSharedIO.h:1256). It then scans for the
// lowest set bit (BitArray<16>::GetFirstNonZeroBit -> the (fieldBits - clz64(field) + 63)
// idiom), clamping an out-of-range result to -1.
s32 CarCheckpointData::GetNextCheckpointIndex() const
{
    CGS_ASSERT(!mCheckpointsRemaining.IsZero(), "No checkpoints set");

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

// X360 0x82357470. Reset the per-car race-distance interface: zero the 8 distance-to-finish
// floats and the active-car count, and set the total race distance to 0.0f. (The X360 zeroes the
// 8 words in a loop, then stores miNumActiveRaceCars=0 and mfTotalRaceDistance=0.0f from a baked
// 0.0f literal. The trailing `return this` is a void-function calling-convention artifact, dropped.)
void RaceCarRaceDistanceInterface::Clear()
{
    for (s32 liRaceCarIndex = 0; liRaceCarIndex < 8; ++liRaceCarIndex)
    {
        mafRaceCarDistanceToFinish[liRaceCarIndex] = 0.0f;
    }

    miNumActiveRaceCars = 0;
    mfTotalRaceDistance = 0.0f;
}

// X360 cpp:267 -- per-rival reset, inlined by the X360 build into FlybyData::Prepare
// (no standalone symbol). Invalidate the race-car slot, clear both message-ID buffers and both
// parameter buffers, zero the per-message parameter counts, default the style to NEUTRAL and the
// message count to 0. NOTE: mPlayerName is intentionally NOT reset (the X360 leaves it untouched).
void FlybyRivalData::Prepare()
{
    meRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;

    std::memset(maacMessageIDs, 0, sizeof(maacMessageIDs));
    std::memset(maacMessageParameter, 0, sizeof(maacMessageParameter));

    maiNumberOfParameters[0] = 0;
    maiNumberOfParameters[1] = 0;

    meMessageStyle     = E_MESSAGE_STYLE_NEUTRAL;
    miNumberOfMessages = 0;
}

// X360 0x82363A08. Reset the whole flyby set: clear the car count, then Prepare() each of the
// (KI_MAX_CARS_IN_FLYBY == 3) rival slots. Returns true (the X360 returns 1). The per-slot reset
// is FlybyRivalData::Prepare, which the X360 build inlined here.
bool FlybyData::Prepare()
{
    miNumberOfCars = 0;

    for (s32 liFlybyCarIndex = 0;
         liFlybyCarIndex < FlybyRivalData::KI_MAX_CARS_IN_FLYBY;
         ++liFlybyCarIndex)
    {
        mRivalsToShow[liFlybyCarIndex].Prepare();
    }

    return true;
}

// X360 0x82356EF0. Append a rival to the flyby set: store its active-race-car index and copy its
// player name into the next free slot, then bump the count. Guards (verbatim baked file/line):
// count in [0, KI_MAX_CARS_IN_FLYBY), non-null name with a non-empty, < KI_USERNAME_LENGTH string.
// The X360 inlines lpPlayerName->GetPlayerName() to the name buffer; the 16-byte memcpy is the
// PlayerName struct copy. The trailing memcpy `return` is a void-function artifact, dropped.
void FlybyData::AddCar(EActiveRaceCarIndex leRaceCarIndex, const CgsNetwork::PlayerName* lpPlayerName)
{
    CGS_ASSERT(miNumberOfCars >= 0, "miNumberOfCars >= 0");
    CGS_ASSERT(miNumberOfCars < FlybyRivalData::KI_MAX_CARS_IN_FLYBY, "miNumberOfCars < FlybyRivalData::KI_MAX_CARS_IN_FLYBY");
    CGS_ASSERT(lpPlayerName, "lpPlayerName");
    CGS_ASSERT(lpPlayerName, "lpPlayerName->GetPlayerName()");
    CGS_ASSERT(std::strlen(lpPlayerName->GetPlayerName()) != 0, "strlen( lpPlayerName->GetPlayerName() ) > 0");
    CGS_ASSERT(std::strlen(lpPlayerName->GetPlayerName()) < static_cast<u32>(CgsNetwork::PlayerName::KI_USERNAME_LENGTH), "strlen( lpPlayerName->GetPlayerName() ) < static_cast<uint32_t>(CgsNetwork::KI_USERNAME_LENGTH )");

    mRivalsToShow[miNumberOfCars].meRaceCarIndex = leRaceCarIndex;
    std::memcpy(&mRivalsToShow[miNumberOfCars].mPlayerName, lpPlayerName,
                CgsNetwork::PlayerName::KI_USERNAME_LENGTH);
    ++miNumberOfCars;
}

// X360 0x82357070. Attach a localised string-id message (+ optional parameter) to the most recently
// added rival (index miNumberOfCars-1). Guards (verbatim baked file/line): at least one car added,
// not over capacity, the message id fits KI_MAX_MESSAGE_ID_BUFFER, the per-rival message slot is
// free (< KI_MAX_MESSAGES), and -- when a parameter is supplied -- it fits KI_USERNAME_LENGTH and
// the per-message parameter count stays <= KI_MAX_PARAMETERS. The over-length asserts build their
// text through the committed CgsDev::StrStream (the X360 streams into the global assert buffer; a
// local buffer is used here since that global has no committed home). The two CgsStringUtils.h:55
// asserts are the inlined StrnCpy bounds check. Returns void (the X360 `return result` is a
// calling-convention artifact).
void FlybyData::AddMessage(const char* lpcMessage, const char* lpcParameter)
{
    CGS_ASSERT(miNumberOfCars > 0, "miNumberOfCars > 0");
    CGS_ASSERT(miNumberOfCars <= FlybyRivalData::KI_MAX_CARS_IN_FLYBY, "miNumberOfCars <= FlybyRivalData::KI_MAX_CARS_IN_FLYBY");
    if (std::strlen(lpcMessage) >= static_cast<u32>(FlybyRivalData::KI_MAX_MESSAGE_ID_BUFFER))
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "String ID " << (lpcMessage ? lpcMessage : "<NULLSTRING>") << " too long\n";
        CGS_ASSERT(false, lacMessageBuffer);
    }

    const s32 liFlybyCarIndex = miNumberOfCars - 1;
    FlybyRivalData& lRival = mRivalsToShow[liFlybyCarIndex];
    const s32 liCurrentMessageIndex = lRival.miNumberOfMessages;

    CGS_ASSERT(liCurrentMessageIndex < FlybyRivalData::KI_MAX_MESSAGES, "liCurrentMessageIndex < GameStateModuleIO::FlybyRivalData::KI_MAX_MESSAGES");

    // Inlined StrnCpy<KI_MAX_MESSAGE_ID_BUFFER>(maacMessageIDs[idx], lpcMessage) bounds check.
    if (std::strlen(lpcMessage) >= static_cast<u32>(FlybyRivalData::KI_MAX_MESSAGE_ID_BUFFER))
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "String too long: " << (lpcMessage ? lpcMessage : "<NULLSTRING>");
        CGS_ASSERT(false, lacMessageBuffer);
    }
    std::strncpy(lRival.maacMessageIDs[liCurrentMessageIndex], lpcMessage,
                 FlybyRivalData::KI_MAX_MESSAGE_ID_BUFFER);

    if (lpcParameter)
    {
        if (std::strlen(lpcParameter) >= static_cast<u32>(FlybyRivalData::KI_MAX_PARAMTER_LENGTH))
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Param " << lpcParameter << " too long \n";
            CGS_ASSERT(false, lacMessageBuffer);
        }
        // Inlined StrnCpy<KI_MAX_PARAMTER_LENGTH>(maacMessageParameter[idx], lpcParameter) bounds check.
        if (std::strlen(lpcParameter) >= static_cast<u32>(FlybyRivalData::KI_MAX_PARAMTER_LENGTH))
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "String too long: " << lpcParameter;
            CGS_ASSERT(false, lacMessageBuffer);
        }
        std::strncpy(lRival.maacMessageParameter[liCurrentMessageIndex], lpcParameter,
                     FlybyRivalData::KI_MAX_PARAMTER_LENGTH);

        ++lRival.maiNumberOfParameters[liCurrentMessageIndex];
        CGS_ASSERT(lRival.maiNumberOfParameters[liCurrentMessageIndex] <= FlybyRivalData::KI_MAX_PARAMETERS, "mRivalsToShow[liFlybyCarIndex].maiNumberOfParameters[liCurrentMessageIndex] <= GameStateModuleIO::FlybyRivalData::KI_MAX_PARAMETERS");
    }

    ++lRival.miNumberOfMessages;
}

// X360 0x821F2A90. Indexed accessor for one of the (KI_MAX_CARS_IN_FLYBY == 3) per-rival records.
// The X360 body bounds-checks liRivalIndex with two verbatim asserts (BrnGameStateSharedIO.h
// :1110/:1111) then returns &mRivalsToShow[liRivalIndex] (this + 4 + 196 * liRivalIndex).
FlybyRivalData* FlybyData::GetFlybyRivalData(s32 liRivalIndex)
{
    CGS_ASSERT(liRivalIndex >= 0, "liRivalIndex >= 0");
    CGS_ASSERT(liRivalIndex < FlybyRivalData::KI_MAX_CARS_IN_FLYBY, "liRivalIndex < FlybyRivalData::KI_MAX_CARS_IN_FLYBY");
    return &mRivalsToShow[liRivalIndex];
}

// X360 0x821F2B08. Free predicate over EGameModeType. Returns true for exactly two mode values --
// E_MODE_ONLINE_FREE_BURN_LOBBY (15) and E_MODE_ONLINE_SHOWTIME (16). FAITHFUL QUIRK: despite the
// name, the online-showtime mode is also treated as a free-burn lobby (the X360 build groups them).
bool IsOnlineFreeBurnLobby(EGameModeType leGameMode)
{
    return (leGameMode == E_MODE_ONLINE_FREE_BURN_LOBBY)
        || (leGameMode == E_MODE_ONLINE_SHOWTIME);
}

// OnlineGameResults::operator= (X360 0x82311C30) moved to its DWARF home BrnGameActions.cpp
// (re-homed onto typed members). It is no longer defined here.

// ===== CarScoreData =====

// X360 0x822A45A8. The X360 ctor zeroes a fixed subset of fields; we zero-init the whole 296-byte
// record (a safe superset -- callers reset via ClearData before use). (return-result artifact dropped.)
CarScoreData::CarScoreData()
{
    memset(this, 0, sizeof(*this));
}

// X360 0x821F2B28. Reset to event-start defaults: zero the record, then seed mTotalTime = Time(0),
// the count fields (8/9/9) and the two slot-index sentinels (-1). All other fields are 0 (memset).
// (The +0x5C sentinel is now the named miOnlineStandingsPosition, carved out for the online scorers;
// ClearData still seeds it to -1, matching the X360 stw r11,0x5C with r11 == -1.)
void CarScoreData::ClearData()
{
    memset(this, 0, sizeof(*this));
    mTotalTime               = CgsSystem::Time(0.0f);
    miField08                = 8;
    miField0C                = 9;
    miField14                = 9;
    miOnlineStandingsPosition = -1;
    miInvalidIndex60         = -1;
}

// X360 0x821F4740. The X360 emits explicit per-word/per-byte stores through +292; a full memcpy of
// the trivially-copyable 296-byte record is equivalent.
CarScoreData& CarScoreData::operator=(const CarScoreData& lOther)
{
    memcpy(this, &lOther, sizeof(*this));
    return *this;
}

// X360 0x8231C170. While a chain is active and the current multiplier is below the supplied ceiling,
// reserve a ChainableMultiplierInfo and fill it from the chainable fields + the caller's value.
void CarScoreData::GetChainableStuntMultipliers(s32 liMaxMultiplier, s32 liSuppliedValue,
                                                Array<ChainableMultiplierInfo, 8>* lpaMultiplierInfo) const
{
    if (mbChainActive && miCurrentChainableMultiplier < liMaxMultiplier)
    {
        ChainableMultiplierInfo* lpInfo = lpaMultiplierInfo->AddNew();
        CGS_ASSERT(lpInfo != nullptr, "lpChainableMultiplierInfo");
        lpInfo->miChainableScore = miChainableScore;
        lpInfo->miChainableField = miChainableField;
        lpInfo->miSuppliedValue  = liSuppliedValue;
        lpInfo->miMultiplier     = miCurrentChainableMultiplier;
    }
}
}
}
