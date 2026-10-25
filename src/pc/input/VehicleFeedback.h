#pragma once

#include "pc/input/Gamepad.h"
#include "pc/input/EngineFeedback.h"
#include "pc/input/BoostFeedback.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/AttribSys/Generated/classes/surfacelist.h"
#include "GameSource/AttribSys/Generated/classes/surface.h"
#include "GameSource/AttribSys/Generated/classes/rumblesurface.h"
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"

// FLAG PC-platform leaf: read-only observer of published game-state inputs.
// Called under GameStateModule's existing input/output locks. No device access,
// recovered layout additions, queue consumption or gameplay decisions live here.
namespace CgsInput::AdaptiveTriggers
{
    inline float FeedbackMagnitude(const Vector3& lrVector)
    {
        return Finite(std::hypot(Finite(lrVector.x), Finite(lrVector.y), Finite(lrVector.z)));
    }

    inline void ObserveContacts(Sample& lrSample, EntityId lPlayer,
        const BrnPhysics::ContactSpy::ContactSpyInterface& lrContacts)
    {
        if (!lrContacts.IsValid()) return;
        const auto* lpRun = lrContacts.GetRaceCarContactRunList()->GetRunDataWithEntityID(lPlayer);
        if (!lpRun) return;
        const auto* lpQueue = lrContacts.GetRaceCarContacts();
        for (s32 i = 0; i < lpRun->GetRunLength(); ++i)
        {
            const auto* lpContact = lpQueue->GetBaseContact(lpRun->GetStartIndex() + i);
            const bool lbWorld = (lpContact->mEntityIdB.muValue >> 24) == BrnWorld::E_ENTITYTYPE_WORLD;
            // Same stress units and world attenuation as the recovered rumble
            // reader. Final response/thresholds are PC presentation tuning.
            const float lfNormal = Unit(FeedbackMagnitude(lpContact->mNormalStress) * (lbWorld ? 0.000002f : 0.00002f));
            lrSample.mfImpact = (std::max)(lrSample.mfImpact, lfNormal);
            if (lbWorld) lrSample.mfScrape = (std::max)(lrSample.mfScrape,
                Unit(FeedbackMagnitude(lpContact->mFrictionStress) * 0.00002f));
        }
    }

    inline void ObserveSurface(Sample& lrSample, const BrnPhysics::Vehicle::RaceCarState& lrCar)
    {
        if (!lrSample.muGroundedWheels) return;
        Attrib::Gen::surfacelist lSurfaces;
        lSurfaces.ChangeWithDefault(Attrib::StringToKey("340654"));
        if (!lSurfaces.IsValid()) return;
        unsigned luSeen[4]{};
        unsigned luCount = 0;
        for (unsigned i = 0; i < 4; ++i)
        {
            const auto& lrRoad = lrCar.maWheels[i].mRoadContact;
            if (!lrCar.mabWheelExists[i] || !lrRoad.mbLineTestIsValid || !lrRoad.mbIsOnGround) continue;
            const unsigned luSurface = (lrRoad.mCollisionTag.muValue >> 4) & 0x3f;
            if (std::find(luSeen, luSeen + luCount, luSurface) != luSeen + luCount) continue;
            luSeen[luCount++] = luSurface;
            const auto* lpRef = static_cast<const Attrib::RefSpec*>(lSurfaces.Surfaces(luSurface));
            if (!lpRef || !lpRef->HasResolvedCollection()) continue;
            Attrib::Gen::surface lSurface(*lpRef, nullptr);
            if (!lSurface.RumbleSurface().HasResolvedCollection()) continue;
            Attrib::Gen::rumblesurface lRumble(lSurface.RumbleSurface(), nullptr);
            if (lRumble.RumblePriority() < 0) continue;
            const float lfVolume = Unit((std::fabs(lrSample.mfSpeedMPH) - lRumble.MinSpeedForRumble()) /
                (std::max)(1.0f, lRumble.MaxSpeedForRumble() - lRumble.MinSpeedForRumble()));
            lrSample.mfSurface = (std::max)(lrSample.mfSurface, lfVolume * Unit(
                (std::max)(lRumble.LeftMotorSustainSpeed(), lRumble.RightMotorSustainSpeed())));
        }
    }

    inline void ObserveEvents(Sample& lrSample, EActiveRaceCarIndex lePlayer,
        const BrnGameState::GameStateModuleIO::GameEventQueue& lrQueue)
    {
        namespace Events = BrnGameState::GameStateModuleIO;
        const CgsModule::Event* lpEvent = nullptr;
        s32 liSize = 0;
        s32 liType = lrQueue.GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent)
        {
            switch (liType)
            {
                case Events::E_EVENT_VEHICLE_IMPACT:
                    if (liSize >= static_cast<s32>(sizeof(Events::VehicleImpactEvent)))
                    {
                        const auto& lrEvent = *reinterpret_cast<const Events::VehicleImpactEvent*>(lpEvent);
                        if (lrEvent.meAggressorActiveRaceCarIndex == lePlayer || lrEvent.meVictimActiveRaceCarIndex == lePlayer)
                            lrSample.muEvents |= E_EVENT_IMPACT;
                    }
                    break;
                case Events::E_EVENT_JUST_BOUNCED:
                    if (lrSample.mbShowtime && liSize >= static_cast<s32>(sizeof(Events::JustBouncedEvent)))
                    {
                        const auto& lrEvent = *reinterpret_cast<const Events::JustBouncedEvent*>(lpEvent);
                        lrSample.muEvents |= E_EVENT_BOUNCE;
                        if (lrEvent.mbBoostedBounce) lrSample.muEvents |= E_EVENT_BOOSTED_BOUNCE;
                    }
                    break;
                case Events::E_EVENT_TRAFFIC_CHECKING: lrSample.muEvents |= E_EVENT_TRAFFIC_CHECK; break;
                case Events::E_EVENT_NEAR_MISS_SCORED: lrSample.muEvents |= E_EVENT_NEAR_MISS; break;
                case Events::E_EVENT_COMPLETED_STUNT:
                    if (liSize >= static_cast<s32>(sizeof(Events::CompletedStuntEvent)) &&
                        reinterpret_cast<const Events::CompletedStuntEvent*>(lpEvent)->muStuntActionComplete)
                        lrSample.muEvents |= E_EVENT_STUNT;
                    break;
                default: break;
            }
            liType = lrQueue.GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }

    inline Sample ProjectVehicle(const BrnPhysics::Vehicle::RaceCarState& lrCar,
        const BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo& lrBoost, bool lbShowtime, unsigned luDrivenMask)
    {
        Sample lSample;
        lSample.mbShowtime = lbShowtime;
        lSample.mbActive = !lrCar.mbIsHidden;
        lSample.mbDriving = lrCar.meDriverType == BrnPhysics::Vehicle::E_DRIVER_TYPE_PLAYER || lSample.mbShowtime;
        if (!lSample.mbActive) return lSample;
        lSample.muVehicle = lrCar.mCarAssetAttribKey ^ (static_cast<std::uint64_t>(lrCar.mEntityId.muValue) << 32);
        lSample.mbReset = lrCar.mbResetCarTransform || lrCar.mbForceReset;
        lSample.mbCrashing = lrCar.mbCrashing;
        lSample.miGear = lrCar.mi8Gear;
        lSample.mbReverse = lrCar.mi8Gear == BrnPhysics::Vehicle::KU8_REVERSE_GEAR;
        // VehiclePhysics::UpdateEngine explicitly swaps gas/brake in reverse.
        // The published driver controls remain in physical-pedal order.
        lSample.mfGas = lSample.mbReverse ? lrCar.mfBrake : lrCar.mfGas;
        lSample.mfBrake = lSample.mbReverse ? lrCar.mfGas : lrCar.mfBrake;
        lSample.mfHandBrake = lrCar.mfHandBrake;
        lSample.mfSpeedMPH = Finite(lrCar.mfSpeedMPH);
        lSample.mfRPM = lrCar.mfRPM;
        lSample.mfUpShiftRPM = lrCar.mfUpShiftRPM;
        lSample.mfAirTime = lrCar.mfTimeInAir;
        lSample.mfVerticalSpeed = lrCar.mLinearVelocity.y;
        lSample.mbBoosting = lrBoost.mbIsBoosting;
        lSample.mbDrifting = lrBoost.mbIsDrifting || lrCar.mfTimeDrifting > 0;
        lSample.mfDrift = lrCar.mfAbsDriftScale;
        lSample.meCharacter = lrBoost.meBoostType == BrnWorld::E_BOOST_TYPE_AGGRESSION ? Character::Power :
            lrBoost.meBoostType == BrnWorld::E_BOOST_TYPE_STUNT ? Character::Stunt : Character::Speed;
        // Size/speed thresholds select presentation tuning, not a physics mass.
        if (lrCar.mHalfExtent.y > 0.85f && lrCar.mfMaxSpeedMPH < 145.0f) lSample.meCharacter = Character::Heavy;
        for (unsigned i = 0; i < 4; ++i)
        {
            const auto& lrWheel = lrCar.maWheels[i];
            if (!lrCar.mabWheelExists[i] || !lrWheel.mRoadContact.mbLineTestIsValid || !lrWheel.mRoadContact.mbIsOnGround) continue;
            ++lSample.muGroundedWheels;
            lSample.mfBrakeSkid = (std::max)(lSample.mfBrakeSkid, Unit(lrWheel.mfSkidFactor));
            if ((luDrivenMask & (1u << i)) && lrWheel.mbHasTraction)
            {
                ++lSample.muGroundedDrivenWheels;
                const float lfRoadSpeed = std::fabs(Finite(lrWheel.mfRoadLongSpeed));
                const float lfSlip = std::fabs(Finite(lrWheel.mfWheelLongSpeed) - Finite(lrWheel.mfRoadLongSpeed)) /
                    (std::max)(5.0f, lfRoadSpeed);
                lSample.mfDrivenSlip = (std::max)(lSample.mfDrivenSlip, Unit(lfSlip));
            }
        }
        lSample.muChain = lrBoost.muNumChained;
        if (lrBoost.mbWasChainJustCompleted) lSample.muEvents |= E_EVENT_CHAIN;
        return lSample;
    }

    inline Sample ObserveVehicle(
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface& lrCars,
        EActiveRaceCarIndex lePlayer, const BrnGameState::GameStateModuleIO::GameEventQueue& lrEvents,
        const BrnPhysics::ContactSpy::ContactSpyInterface& lrContacts, bool lbBlocked, bool lbDrivingAllowed)
    {
        if (lbBlocked || lePlayer < E_ACTIVE_RACE_CAR_INDEX_0 || lePlayer >= E_ACTIVE_RACE_CAR_INDEX_COUNT ||
            !lrCars.IsRaceCarActive(lePlayer) || !lrCars.IsRaceCarLoaded(lePlayer)) return {};
        const auto& lrCar = *lrCars.GetRaceCarState(lePlayer);
        const auto& lrBoost = *lrCars.GetBoostOutputInfoN(lePlayer);
        Sample lSample = ProjectVehicle(lrCar, lrBoost, lrCars.IsCarInShowtime(lePlayer),
            EngineFeedback::GetDrivenMask(lrCar.mEntityId.muValue));
        lSample.mbDriving &= lbDrivingAllowed;
        if (!lSample.mbActive) return lSample;
        ObserveSurface(lSample, lrCar);
        ObserveContacts(lSample, lrCar.mEntityId, lrContacts);
        if (!lrCar.mbContactingWall) lSample.mfScrape = 0;
        ObserveEvents(lSample, lePlayer, lrEvents);
        lSample.mbLimiter = EngineFeedback::IsLimited(lrCar.mEntityId.muValue);
        lSample.muBoostAward = BoostFeedback::GetSerial();
        return lSample;
    }
}
