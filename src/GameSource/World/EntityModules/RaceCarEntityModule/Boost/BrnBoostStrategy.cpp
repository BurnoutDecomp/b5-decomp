// ============================================================================
// BrnWorld::BoostStrategy -- the mode-start boost seed.
//
//   OnModeStart   X360 0x822A5FC8
//
// The base class's own bodies are otherwise all pure/derived (BoostBurnout2/3/5 carry the
// per-strategy TUs), so this is the first BrnBoostStrategy.cpp. OnModeStart is NOT virtual
// -- HandlePrepareForModeAction @0x823092F0 calls it directly on the module's current
// strategy (`*(a1 + 97504)`) -- but it DISPATCHES through slot 49 (+0xC4) AddBoost, which
// is what makes the quarter/full seeds respect each strategy's own earning rules.
//
// SOURCE: BURNOUT_X360_ARTIST.XEX raw asm 0x822A5FC8..0x822A608C (a 14-entry jump table at
// jpt_822A5FE4). The three float constants are read out of the decrypted image, not guessed:
//   flt_82003F40 = 0x3E800000 = 0.25f
//   flt_82005450 = 0x3F666666 = 0.9f   (0.89999998 as the pseudocode prints it)
//   flt_820147FC = 0x3F000000 = 0.5f
// ============================================================================
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostStrategy.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // EGameModeType
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleIOQueues.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/math/vpu/vector3_operation.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace BrnWorld
{
    const f32 KF_BOOST_EFFECT_LOSS_MAX       = 2.0f;
    const f32 KF_BOOST_EFFECT_LOSS_MIN       = 1.0f;
    const f32 KF_MIN_BOOST_BEFORE_USE        = 0.15f;
    const f32 KF_MIN_BOOST_BEFORE_BOUNCE     = 0.01f;
    const f32 KF_MIN_SPIN_ANGLE              = 45.0f;
    const f32 KF_BOUNCE_BOOST_FLASH_BAR_TIME = 0.6f;
    const f32 KF_MIN_BOOST_TIME              = 1.25f;

    const f32 BoostStrategy::KF_ON_SHORTCUT = 30.0f;
    const f32 BoostStrategy::KF_ONCOMING_MINSPEED = 0.0f;

    namespace
    {
        // The three seeds, read out of the decrypted ARTIST image at the addresses the asm
        // names (file_off = 0x3000 + vaddr - 0x82000000, big-endian) -- NOT inferred from the
        // pseudocode's printed decimals.
        const f32 KF_MODE_START_QUARTER_BAR    = 0.25f;   // flt_82003F40 (0x3E800000)
        const f32 KF_MODE_START_ONLINE_HIGH_BAR = 0.9f;   // flt_82005450 (0x3F666666)
        const f32 KF_MODE_START_ONLINE_HALF_BAR = 0.5f;   // flt_820147FC (0x3F000000)

        template <typename Payload>
        void AddBoostEvent(RaceCarEntityModuleIO::GameEventQueue* lpQueue,
                           const Payload& lrPayload, s32 liEventId)
        {
            lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lrPayload),
                              liEventId, static_cast<s32>(sizeof(Payload)));
        }

        struct ScalarBoostEvent
        {
            f32 mfValue;
        };

        struct TailgatingBoostEvent
        {
            f32                 mfDistance;
            EActiveRaceCarIndex meCarIndex;
        };

        static_assert(sizeof(ScalarBoostEvent) == 4, "Breaker scalar boost events are 4 bytes");
        static_assert(sizeof(TailgatingBoostEvent) == 8, "Breaker tailgating event is 8 bytes");
    }

// Prepare @0x822A5C48.  The apparent VMX stores in Breaker are the compiler's
// implementation of Vector3::SetZero and scalar zero stores; only
// mStartPosition is a vector-valued member here.
bool BoostStrategy::Prepare()
{
    mfMaxBoost = 100.0f;
    mbCrashing = false;
    mbWrecking = false;
    mbInfiniteBoost = false;
    mStartPosition.SetZero();
    mfSpeed = 0.0f;
    mbBoosting = false;
    mfDriftingDistance = 0.0f;
    mbBoostingLastUpdate = false;
    mfInAirDistance = 0.0f;
    mbBoostRequested = false;
    mfOncomingDistance = 0.0f;
    mbDrifting = false;
    mfTailgatingDistance = 0.0f;
    mbInAir = false;
    mfSpinAngle = 0.0f;
    mbOncoming = false;
    mfCurrentCarBoostLossLevel = 0.0f;
    mbIsTailgating = false;
    mfTotalBoostingTimeFromStart = 0.0f;
    mbBoostEarningEnabled = false;
    mfCurrentBoostingTime = 0.0f;
    mbJustTrafficChecked = false;
    mfMinBoostAllowedAmount = 0.0f;
    mbInChainMode = false;
    mfBoostEventModeModifier = 1.0f;
    mbJustLostBoostChunk = false;
    mfOriginalBoostEarning = 1.0f;
    miCombinedBoostLevel = 0;
    mfTotalDistanceTraveled = 0.0f;
    miCurrentCarBoostLevel = 0;
    mfTimeSpentCheating = 0.0f;
    miBoostLevel = 0;
    mbForceBoost = false;
    mbIsBoostFull = false;
    meTailgatedCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    mePreviousOncomingState = E_ONCOMING_STATE_FALSE;
    return true;
}

// UpdateChainExploits @0x822A5D18.  The vector dot/rsqrt sequence in the PPC
// listing is the RenderWare Magnitude implementation, not scalar data widened
// into a persistent VecFloat member.
void BoostStrategy::UpdateChainExploits(Vector3 lCurrentPosition)
{
    if ((!mbBoostingLastUpdate && mbBoosting) || mbChainNotifyPending)
    {
        mStartPosition = lCurrentPosition;
        mfTotalDistanceTraveled = 0.0f;
    }

    mbBoostingLastUpdate = mbBoosting;
    if (mbBoosting)
        mfTotalDistanceTraveled =
            rw::math::vpu::Magnitude(mStartPosition - lCurrentPosition);
}

void BoostStrategy::SetForceBoost(bool lbForceBoost) // 0x822A5DC8
{
    mbForceBoost = lbForceBoost && mbBoosting;
}

bool BoostStrategy::GetIsChainNotifyPending(u32* lpOutNumChained) // 0x822A5DF0
{
    CGS_ASSERT(lpOutNumChained != nullptr, "lpOutNumChained");
    *lpOutNumChained = 0;
    return false;
}

f32 BoostStrategy::GetCurrentBoostingTime() { return mfCurrentBoostingTime; } // 0x822A5E50
bool BoostStrategy::IsBoosting() const { return mbBoosting; }                  // 0x822A5E58
bool BoostStrategy::IsInAir() const { return mbInAir; }                       // 0x822A5E60
bool BoostStrategy::IsDrifting() const { return mbDrifting; }                 // 0x822A5E68
bool BoostStrategy::IsSpinning() const { return mfSpinAngle > 45.0f; }        // 0x822A5E70

bool BoostStrategy::IsATrafficCheckPending() // 0x822A5E98
{
    const bool lbPending = mbJustTrafficChecked;
    mbJustTrafficChecked = false;
    return lbPending;
}

bool BoostStrategy::HasJustLostBoostChunk() // 0x822A5EC0
{
    const bool lbLost = mbJustLostBoostChunk;
    mbJustLostBoostChunk = false;
    return lbLost;
}

bool BoostStrategy::IsOncoming() const { return mbOncoming; }          // 0x822A5ED8
bool BoostStrategy::IsBoostFull() const { return mbIsBoostFull; }      // 0x822A5EE0
bool BoostStrategy::IsTailgating() const { return mbIsTailgating; }    // 0x822A5EE8
f32 BoostStrategy::GetBoostAmount() const { return mfBoostAmount; }    // 0x822A5EF0
f32 BoostStrategy::GetMaxBoost() const { return mfMaxBoost; }          // 0x822A5EF8

void BoostStrategy::SetCrashing(bool lbCrashing) // 0x822A5F00
{
    if (lbCrashing && !mbCrashing)
        OnCrash();
    mbCrashing = lbCrashing;
}

void BoostStrategy::SetWrecking(bool lbWrecking, bool lbIsInOnlineGameMode)
{
    if (lbWrecking && !mbWrecking)
        OnWrecked(lbIsInOnlineGameMode);
    mbWrecking = lbWrecking;
}

void BoostStrategy::SetInfiniteBoost(bool lbInfiniteBoost) // 0x822A5F60
{
    if (lbInfiniteBoost && !mbInfiniteBoost)
        OnEnterInfiniteBoost();
    mbInfiniteBoost = lbInfiniteBoost;
}

void BoostStrategy::SetSpeed(f32 lfSpeed) // 0x822A5FC0
{
    mfSpeed = lfSpeed;
}

// ----------------------------------------------------------------------------
// OnModeStart @ 0x822A5FC8.
//
// Seed the boost bar for the mode that is starting. Four shapes, straight off the jump
// table -- and note the DEFAULT arm is "do nothing", reached two different ways: the
// `cmplwi cr6,r4,0xD / bgtlr cr6` guard at the top returns immediately for anything above
// E_MODE_ONLINE_BURNING_HOME_RUN (so E_MODE_ONLINE_FREE_BURN(14),
// E_MODE_ONLINE_FREE_BURN_LOBBY(15) and E_MODE_ONLINE_SHOWTIME(16) never touch the bar),
// and six in-range modes route to the empty table slot at 0x822A6088.
//
// ⚠️ THE TWO ARMS ARE NOT THE SAME OPERATION. Modes 0/3/7/8 and 5 go through the VIRTUAL
// AddBoost (slot 49, `lwz r11,0xC4(r10)` / `bctr`), which clamps and honours
// mbBoostEarningEnabled / mbCrashing; modes 10/11/13 STORE mfBoostAmount (+0xA0) directly
// (`stfs f0,0xA0(r3)`), bypassing all of that. Collapsing either into the other would
// change what the bar does on a crashed or earning-disabled car.
//
// lbFlag is the caller's `*(a1 + 96404) == 2` -- BoostManager's current strategy-type
// selector -- and only the online arm reads it.
// ----------------------------------------------------------------------------
void BoostStrategy::OnModeStart(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType,
                                bool lbFlag)
{
    switch (leGameModeType)
    {
    // jumptable 822A5FE4 cases 0,3,7,8 -- asm 0x822A6020.
    case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE:
    case BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE:
    case BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK:
    case BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN:
        AddBoost(mfMaxBoost * KF_MODE_START_QUARTER_BAR);
        break;

    // jumptable 822A5FE4 case 5 -- asm 0x822A6040.
    case BrnGameState::GameStateModuleIO::E_MODE_BURNING_ROUTE:
        AddBoost(mfMaxBoost);
        break;

    // jumptable 822A5FE4 cases 10,11,13 -- asm 0x822A6054.
    case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_RACE:
    case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE:
    case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN:
        mfBoostAmount = lbFlag ? (mfMaxBoost * KF_MODE_START_ONLINE_HIGH_BAR)
                               : (mfMaxBoost * KF_MODE_START_ONLINE_HALF_BAR);
        break;

    // jumptable 822A5FE4 cases 1,2,4,6,9,12 (the empty slot at 0x822A6088), plus every
    // mode > 13 via the `bgtlr` guard: the bar is left exactly as it was.
    default:
        break;
    }
}

void BoostStrategy::SetBoostRequested(bool lbBoostRequested) // 0x822A6090
{
    const bool lbAllowed = AreWeAllowedToBoost();
    mbBoostRequested = lbAllowed && (lbBoostRequested || mbForceBoost);
}

void BoostStrategy::SetTailgating(bool lbTailgating,
                                  EActiveRaceCarIndex leTailgatedCarIndex) // 0x822A6110
{
    mbIsTailgating = lbTailgating;
    meTailgatedCarIndex = leTailgatedCarIndex;
}

void BoostStrategy::SetInAir(bool lbInAir) { mbInAir = lbInAir; }             // 0x822A6120
void BoostStrategy::SetDrifting(bool lbDrifting) { mbDrifting = lbDrifting; } // 0x822A6128
void BoostStrategy::SetSpinAngle(f32 lfSpinAngle) { mfSpinAngle = lfSpinAngle; } // 0x822A6130

void BoostStrategy::SetOncomingState(OncomingState leOncomingState) // 0x822A6138
{
    if (mbCrashing)
    {
        mePreviousOncomingState = leOncomingState;
        mbOncoming = false;
        return;
    }

    const bool lbOncomingNow =
        ((leOncomingState == E_ONCOMING_STATE_TRUE) &&
         (mfSpeed > KF_ONCOMING_MINSPEED)) ||
        (mbOncoming && leOncomingState == E_ONCOMING_STATE_PREVIOUS);

    if (lbOncomingNow)
    {
        mePreviousOncomingState = leOncomingState;
        mfOncomingFade = 0.5f;
        mbOncoming = true;
    }
    else
    {
        mbOncoming = mfOncomingFade > 0.0f;
        mePreviousOncomingState = leOncomingState;
    }
}

void BoostStrategy::SetBoostAmount(f32 lfBoostFraction) // 0x822A61E0
{
    mfBoostAmount = std::max(mfMaxBoost * lfBoostFraction, 0.0f);
}

void BoostStrategy::SetBoostEarningEnabled(bool lbIsEnabled) // 0x822A6208
{
    mbBoostEarningEnabled = lbIsEnabled;
}

void BoostStrategy::AddBoost(f32 lfBoostAmount) // 0x822C0E10
{
    if (!mbBoostEarningEnabled || mbCrashing)
        return;

    f32 lfSpeedMultiplier = 1.0f;
    if (mfSpeed >= mfSpeedForMinEarning)
    {
        const f32 lfEarningLerpValue =
            (mfSpeed - mfSpeedForMinEarning) /
            (mfSpeedForMaxEarning - mfSpeedForMinEarning);
        lfSpeedMultiplier = 1.0f + std::min(std::max(lfEarningLerpValue, 0.0f), 1.0f);
    }

    mfBoostAmount = std::min(
        std::max(mfBoostAmount + lfSpeedMultiplier * lfBoostAmount, 0.0f),
        mfMaxBoost);
}

void BoostStrategy::UpdateMaxBoost(bool lbFillToMax) // 0x822C0EB0
{
    mfMaxBoost = static_cast<f32>(miCombinedBoostLevel) * 10.0f;
    if (mfMaxBoost == 0.0f)
        mfMaxBoost = 1.0f;

    mfBoostAmount = std::min(std::max(mfBoostAmount, 0.0f), mfMaxBoost);
    if (lbFillToMax)
        mfBoostAmount = mfMaxBoost;
}

void BoostStrategy::SetBoostSegments(s32 liBoostSegments) // 0x822C0F28
{
    miBoostLevel = liBoostSegments;
    UpdateMaxBoost(false);
}

void BoostStrategy::SetCarStatBoostLevel(s32 liBoostLevel,
                                         s32 liBoostLossLevel) // 0x822D5290
{
    miCombinedBoostLevel = liBoostLevel;
    miBoostLevel = liBoostLevel;
    mfCurrentCarBoostLossLevel =
        std::min(std::max(static_cast<f32>(liBoostLossLevel), 0.0f), 100.0f);
    UpdateMaxBoost(false);
    mfMinBoostAllowedAmount = mfMaxBoost * 0.15f;
    mfBoostAmount = std::min(std::max(mfMaxBoost * 0.5f, 0.0f), mfMaxBoost);
}

void BoostStrategy::RemoveBoost(f32 lfBoostAmount)
{
    mfBoostAmount = std::max(mfBoostAmount - lfBoostAmount, 0.0f);
}

void BoostStrategy::TurnOffBoosting()
{
    mbBoosting = false;
}

void BoostStrategy::OnStartCrashPlay() {}
void BoostStrategy::OnEndCrashPlay() {}
void BoostStrategy::OnPropHit() {}
bool BoostStrategy::IsBlueMode() { return false; }

void BoostStrategy::UpdateStuntBoost(
    const BrnGameState::GameStateModuleIO::CompletedStuntAction*)
{
}

// Update @0x822F8130.  Event ids and payload widths below are taken from the
// raw r5/r6 call setup at each VariableEventQueue::AddEvent call.
void BoostStrategy::Update(RaceCarEntityModuleIO::GameEventQueue* lpEventQueue,
                           f32 lfTimeStep, f32 lfBoostModifier)
{
    mfBoostModifier = lfBoostModifier;
    mfOncomingFade -= lfTimeStep;
    ApplyUpdate(lfTimeStep);

    if (mbBoosting)
    {
        mfTotalBoostingTimeFromStart += lfTimeStep;
        mfCurrentBoostingTime += lfTimeStep;
        mfCurrentNotBoostingTime = 0.0f;
    }
    else if (!mbChainNotifyPending)
    {
        if (mfCurrentBoostingTime != 0.0f)
        {
            const ScalarBoostEvent lEvent = { mfCurrentBoostingTime };
            CGS_ASSERT(lpEventQueue != nullptr, "lpEventQueue");
            AddBoostEvent(lpEventQueue, lEvent, 55);
        }
        mfCurrentBoostingTime = 0.0f;
    }

    if (!mbBoosting)
    {
        mfCurrentNotBoostingTime += lfTimeStep;
        if (mfCurrentNotBoostingTime >= 30.0f &&
            AreWeAllowedToBoost() &&
            mfBoostAmount > mfMaxBoost * 0.5f)
        {
            const s32 liTrainingType = 50;
            AddBoostEvent(lpEventQueue, liTrainingType, 113);
        }
    }

    const f32 lfDistance = std::fabs(mfSpeed * lfTimeStep);

    if (mbDrifting)
    {
        mfDriftingDistance += lfDistance;
        const ScalarBoostEvent lEvent = { mfDriftingDistance };
        AddBoostEvent(lpEventQueue, lEvent, 67);
    }
    else
    {
        mfDriftingDistance = 0.0f;
    }

    if (mfSpinAngle > FLT_EPSILON || mfSpinAngle < -FLT_EPSILON)
    {
        const ScalarBoostEvent lEvent = { mfSpinAngle };
        AddBoostEvent(lpEventQueue, lEvent, 68);
    }

    if (mbCrashing)
        mbForceBoost = false;

    if (mbInAir)
        mfInAirDistance += lfDistance;
    else
        mfInAirDistance = 0.0f;

    if (mbOncoming)
    {
        mfOncomingDistance += lfDistance;
        const ScalarBoostEvent lEvent = { mfOncomingDistance };
        AddBoostEvent(lpEventQueue, lEvent, 70);
    }
    else
    {
        if (mfOncomingDistance > 0.0f)
        {
            const ScalarBoostEvent lEvent = { mfOncomingDistance };
            AddBoostEvent(lpEventQueue, lEvent, 71);
        }
        mfOncomingDistance = 0.0f;
    }

    if (mbIsTailgating)
    {
        mfTailgatingDistance += lfDistance;
        const TailgatingBoostEvent lEvent =
        {
            mfTailgatingDistance,
            meTailgatedCarIndex
        };
        AddBoostEvent(lpEventQueue, lEvent, 72);
    }
    else
    {
        mfTailgatingDistance = 0.0f;
    }
}

} // namespace BrnWorld
