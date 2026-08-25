// ============================================================================
// GameSource/Game/GameBridgeWorldToGui.cpp -- the world -> GUI bridge family
// (the DWARF/PS3 home of these bodies; the PS3 unity asserts bake
// "GameSource/Unity/../Game/GameBridgeWorldToGui.cpp").
//
//   BrnGameModule::BridgeWorldToGui            @0x823EDD50  (PS3 0x11E564-region)
//   BrnGameModule::BridgeWorldVehicleDataToGui @0x823E5768  (PS3 named 0x318A18)
//
// PARTIAL SLICE (boost-bar 206 wave, 2026-08-25). The console's per-frame
// vehicle-data bridge posts, in order: the player-crashing state-change event, the
// engine-state change event (374), then -- gated on IsPlayerCarActive() -- the
// player-index pair (373), the BOOST INFO record (206), the race-car-state derived
// speed/heat/scrape family, and further route/traffic/impact legs in the sibling
// sub-bridges. THIS slice reproduces the whole gate + the boost-info post (the HUD
// boost bar's ONLY producer); every other post is FLAG-deferred below at its exact
// console seat, so later waves land them in place.
//
// PS3-vs-X360 event-id note: the PS3 build posts this record as id 204 with the
// PS3's 32-byte GuiEventBoostInfo; the X360 (this target) posts id 206 with the
// 28-byte X360-ordered record -- the same +2 id divergence the overlay events
// carry. The PC GuiEventBoostInfo is the X360 shape, and AddGuiEvent<T> derives
// id/size from the type (GetEventType() == 206, sizeof == 28).
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiModule.h"         // CgsGui::GuiModule::AddGuiEvent
#include "GameShared/GameClasses/Development/CgsStrStream.h" // StrStream (streamed assert messages)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"              // BrnGui::GuiEventBoostInfo (event 206)
#include "GameSource/World/BrnWorldModuleIO.h"               // BrnWorldIO::UpdateOutputBuffer
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // the active-car interface + BoostOutputInfo

namespace BrnGame
{
namespace
{
    // The PS3 unity build's baked assert path for this TU.
    const char* const KPC_ASSERT_FILE = "..\\..\\..\\GameSource\\Game/GameBridgeWorldToGui.cpp";
}

// ============================================================================
// BridgeWorldVehicleDataToGui @0x823E5768 -- the per-frame player-vehicle publish.
// ============================================================================
void BrnGameModule::BridgeWorldVehicleDataToGui(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer,
        const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer)
{
    using BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface;


    const RCEntityActiveRaceCarOutputInterface* lpActiveInterface =
        lpWorldOutputBuffer->GetActiveRaceCarOutputInterface();
    if (lpActiveInterface == 0)
    {
        CgsDev::Assert::BeginAssert();
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        lacMessage[0] = '\0';
        CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStream << "Invalid active vehicle interface in BrnGameModule::BridgeWorldVehicleDataToGui";
        CgsDev::Assert::FireAssert(lacMessage, KPC_ASSERT_FILE, 231);
        CgsDev::Assert::EndAssert();
        return;   // the console's null interface would crash on the next read; honest early-out
    }

    // FLAG deferred (console order, before the player gate): the player-crashing
    // state-change event (a function-local lbWasPlayerCarCrashing edge latch posting
    // GuiPlayerCrashingStateChangeEvent, with the showtime-mode suppression) and the
    // engine-state change event (374, off a lseLastEngineState edge latch). Their
    // consumers are not on this build's reconstructed path yet.

    if (!lpActiveInterface->IsPlayerCarActive())
        return;

    const EActiveRaceCarIndex lePlayerIndex =
        lpWorldOutputBuffer->GetPlayerActiveRaceCarIndex();
    CGS_ASSERT(lePlayerIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");   // :304

    // FLAG deferred (console seat: between the index assert and the boost post): the
    // player-index pair event (373 -- {active, global}; the global index read carries the
    // "Player car index hasn't been set" / "< E_GLOBAL_RACE_CAR_INDEX_COUNT" asserts).

    // ---- THE BOOST INFO POST (GUI event 206 -> BrnGui::BoostBarRenderer) -------------
    const BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo* lpBoostInfo =
        lpActiveInterface->GetBoostOutputInfoN(lePlayerIndex);
    if (lpBoostInfo == 0)
    {
        CgsDev::Assert::BeginAssert();
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        lacMessage[0] = '\0';
        CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStream << "Invalid Boost Info struct in BrnGameModule::BridgeWorldVehicleDataToGui";
        CgsDev::Assert::FireAssert(lacMessage, KPC_ASSERT_FILE, 316);
        CgsDev::Assert::EndAssert();
        return;
    }

    BrnGui::GuiEventBoostInfo lBoostEvent;
    lBoostEvent.muNumChained            = lpBoostInfo->muNumChained;
    lBoostEvent.mfBoostAmount           = lpBoostInfo->mfBoostAmount;
    lBoostEvent.mfMaxBoost              = lpBoostInfo->mfMaxBoost;
    lBoostEvent.meBoostType             = lpBoostInfo->meBoostType;
    lBoostEvent.mbBoostIsFull           = lpBoostInfo->mbBoostIsFull;
    lBoostEvent.mbIsBoosting            = lpBoostInfo->mbIsBoosting;
    lBoostEvent.mbIsInAir               = lpBoostInfo->mbIsInAir;
    lBoostEvent.mbIsOncoming            = lpBoostInfo->mbIsOncoming;
    lBoostEvent.mbIsDrifting            = lpBoostInfo->mbIsDrifting;
    lBoostEvent.mbNearMiss              = lpBoostInfo->mbNearMiss;
    lBoostEvent.mbIsChainedMode         = lpBoostInfo->mbIsBlueMode;
    lBoostEvent.mbWasChainJustCompleted = lpBoostInfo->mbWasChainJustCompleted;
    lBoostEvent.mbAllowedToBoost        = lpBoostInfo->mbAllowedToBoost;
    lBoostEvent.mbIsTailgating          = lpBoostInfo->mbIsTailgating;


    // FLAG deferred (console seat: between the field fill and the post): the SHOWTIME
    // override -- in either showtime mode the console forces mbBoostIsFull = true and
    // gates mbAllowedToBoost on !CrashModeScoring::HasCrashModeEnded(). Neither the game
    // module's mode word nor CrashModeScoring is homed on this build, and the PC has no
    // showtime mode to enter; the override lands with them.

    // ⚠️ NOT through the static CgsGui::GuiModule::AddGuiEvent(T&,...) helper: that overload
    // strips a 12-byte GuiEvent<N> header, and GuiEventBoostInfo is a BARE 28-byte payload
    // whose GetEventType() carries the id (the same trap the GameMain TimeInfo post
    // documents). The console's AddGuiEvent<GuiEventBoostInfo> @0x823DA510 pushes the WHOLE
    // record: AddEvent(&event, 206, 28) -- reproduced against the queue directly.
    CGS_ASSERT(lpGuiInputBuffer != 0, "Input hasn't been locked for write");   // CgsGuiModule.h:286
    lpGuiInputBuffer->GetGuiEvents()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lBoostEvent),
        lBoostEvent.GetEventType(),
        static_cast<s32>(sizeof(lBoostEvent)));

    // FLAG deferred (console order, after the boost post): the race-car-state derived
    // family -- the speed/heat record off GetRaceCarState (the "Invalid race car state in
    // BrnGameModule::BridgeWorldToGui" :347 read), the stunt-info post (377) and the rest
    // of the per-frame vehicle telemetry. Each lands with its consumer.
}

// ============================================================================
// BridgeWorldToGui @0x823EDD50 -- the world -> GUI umbrella: the four sub-bridges
// + the collision-world request event.
// ============================================================================
void BrnGameModule::BridgeWorldToGui(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer,
        const BrnWorldIO::UpdateOutputBuffer* lpWorldOutputBuffer)
{
    BridgeWorldVehicleDataToGui(lpGuiInputBuffer, lpWorldOutputBuffer);

    // FLAG deferred (console order): BridgeWorldRouteInformationToGui,
    // BridgeWorldTrafficAndPropDataToGui, BridgeWorldImpactInformationToGui, and the
    // world-entity-state -> GuiEventRequestCollisionWorldEvent tail. Each is its own
    // X360 body; they land with their consumers.
}

} // namespace BrnGame
