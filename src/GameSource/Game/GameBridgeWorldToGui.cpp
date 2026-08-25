// ============================================================================
// GameSource/Game/GameBridgeWorldToGui.cpp -- the world -> GUI bridge family
// (the DWARF/PS3 home of these bodies; the PS3 unity asserts bake
// "GameSource/Unity/../Game/GameBridgeWorldToGui.cpp").
//
//   BrnGameModule::BridgeWorldToGui            @0x823EDD50  (PS3 0x11E564-region)
//   BrnGameModule::BridgeWorldVehicleDataToGui @0x823E5768  (PS3 named 0x318A18)
//
// PARTIAL SLICE (boost-bar 206 wave, 2026-08-25). The console's per-frame
// vehicle-data bridge posts, in order: the player-crashing state-change event (377), the
// engine-state change event (379), then -- gated on IsPlayerCarActive() -- the
// player-index pair (376), the BOOST INFO record (206), the race-car-state derived
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
#include "GameSource/Gui/BrnGuiRaceCarInfoEvent.h"           // GuiRaceCarInfoEvent (207, the mRaceCarInfo SoA feed)
// (GuiPlayerEngineEvent, 379 -- the ignition latch -- comes in with BrnGuiEventTypeDefs.h,
//  pulled in above via BrnGuiRaceCarInfoEvent.h/BrnGuiEventTypeDefs.h. ⛔ Do NOT include
//  GameSource/Gui/BrnGuiDemangledEventTypes.h here: it collides with
//  BrnNetworkPlayerImageRenderer.h over a PRE-EXISTING duplicate definition of
//  BrnGui::GuiEventNetworkPlayerImage -- see the note in the wave log.)
#include <cmath>                                             // sqrtf/acosf (the icon heading derivation)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG] the satnav-diag one-shots

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

    // ---- THE PLAYER-CRASHING STATE-CHANGE POST (GUI event 377) -----------------------
    // LANDED 2026-08-25 (crash wave S2). Console order: BEFORE the player gate, because the
    // console's own crashing read is the interface's -1-guarded accessor and is safe while the
    // player car is inactive. Decoded instruction-for-instruction from @0x823E583C..0x823E58CC
    // (the function's single AddGuiEvent<GuiPlayerCrashingStateChangeEvent> call site):
    //   lwz  r11,0x2858(iface) ; cmpwi -1 ; mulli 0x460 ; lbz r10,0x77A(r11)
    //        -> crashing = (playerIndex == -1) ? false : maRaceCarStates[playerIndex].mbCrashing
    //        -- stride 0x460 == 1120 and +0x77A == 1914 are EXACTLY the two constants
    //           RCEntityActiveRaceCarOutputInterface::IsPlayerCarCrashing() already encodes,
    //           so that accessor is the de-inlined console read, not an approximation.
    //   lwz  r11,0(modeType) ; cmpwi 2 / cmpwi 0x10 ; cntlzw ; extrwi 1,26 ; and r31,r11,r10
    //        -> state = !(mode == E_MODE_OFFLINE_SHOWTIME || mode == E_MODE_ONLINE_SHOWTIME)
    //                   && crashing
    //   lbz  r8,byte_82FAEB91 ; cmplw ; beq  -> POST ONLY ON A CHANGE (the edge latch; the
    //        console's "function-local" latch is a file-scope static byte, hence static here)
    //   cntlzw ; extrwi 1,26 ; stw  -> the payload is the NEGATED state:
    //        crashing -> 0 == E_CRASHBARSTATE_START_CRASHED
    //        cleared  -> 1 == E_CRASHBARSTATE_LEAVE_CRASHED
    //   stb  r31,byte_82FAEB91  -> latch := state, AFTER the post
    //
    // ⚠ FLAG (the one term not modelled): the showtime suppression. The console reads the
    // current game-mode type inline off the module blob; BridgeWorldVehicleDataToGui is a
    // BrnGameModule method and this tree gives it no GameStateModule reach (it receives only the
    // GUI input buffer and the world output buffer). The term is pinned to "not in showtime"
    // -- which is the && identity AND is provably this build's actual state: nothing here can
    // set meCurrentGameModeType to 2 or 16, because SetPlayerCarToShowtimeMode's only console
    // caller (PhysicsModule::HandleGameActions @0x825A72F0) is still a boot gate in
    // BrnPhysicsConductorGates.cpp:153. DELETE-WHEN that gate opens: replace the constant with
    // GameStateModule::IsShowtimeGameMode() (@0x823567A8, already bodied, same 2||16 test).
    //
    // ⚠ AND NOTE, DO NOT "FIX": the console posts LEAVE_CRASHED (1) on the falling edge and this
    // reproduction does too -- but BrnFBurnMainHudState::ProcessGameEvents maps only payload
    // 0|2 -> SendStateEvent("START_CRASH") and has NO END_CRASH arm anywhere in the tree. The
    // producer is complete; the missing half is the CONSUMER, and inventing it here would be
    // fabricating a console behaviour rather than reconstructing one.
    {
        const bool lbPlayerCarCrashing = lpActiveInterface->IsPlayerCarCrashing();
        const bool lbInShowtimeMode    = false;   // FLAG: see above -- the && identity, and true here
        const bool lbCrashState        = !lbInShowtimeMode && lbPlayerCarCrashing;

        static bool lbWasPlayerCarCrashing = false;   // console byte_82FAEB91
        if (lbCrashState != lbWasPlayerCarCrashing)
        {
            BrnGui::GuiPlayerCrashingStateChangeEvent lCrashEvent;
            lCrashEvent.meCurrentState =
                lbCrashState ? BrnGui::GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_START_CRASHED
                             : BrnGui::GuiPlayerCrashingStateChangeEvent::E_CRASHBARSTATE_LEAVE_CRASHED;

            CGS_ASSERT(lpGuiInputBuffer != 0, "Input hasn't been locked for write");
            lpGuiInputBuffer->GetGuiEvents()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lCrashEvent),
                lCrashEvent.GetEventType(),
                static_cast<s32>(sizeof(lCrashEvent)));

            lbWasPlayerCarCrashing = lbCrashState;

            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[crash-hud] posting GUI 377 GuiPlayerCrashingStateChangeEvent state="
                    << static_cast<s32>(lCrashEvent.meCurrentState)
                    << (lbCrashState ? " (START_CRASHED)" : " (LEAVE_CRASHED)") << "\n";
            }
        }
    }

    // ---- THE ENGINE-STATE CHANGE POST (GUI event 379 -> GuiCache +0x4B20) -------------
    // LANDED 2026-08-25 (hud reveal gate). This replaces a FLAG that deferred the leg AND
    // mis-numbered it "374": GuiPlayerEngineEvent's own GetEventType() is 379
    // (BrnGuiDemangledEventTypes.h:267, size 4), and 379 is the id GuiCache::RecEvent's
    // +0x4B20 store keys on. The old note's "its consumer is not on this build's
    // reconstructed path yet" was ALSO stale: all three console consumers are in the tree
    // (FBurnMainHudState::UpdateWFInit and ::UpdateRunning both read +0x4B20 today).
    //
    // Console order: AFTER the crashing post above and the DrivableFromCrash post, and
    // BEFORE the IsPlayerCarActive gate -- because, like the crashing read, the engine read
    // is -1-guarded and is safe while the player car is inactive. Decoded
    // instruction-for-instruction from @0x823E591C..0x823E59AC:
    //   lwz r9,0x2858(iface) ; li r10,4 ; cmpwi -1 ; [ne] lwz r10,0x285C(iface)
    //        -> leState = (playerIndex == -1) ? E_..._ENGINE_STATE_COUNT(4)
    //                                        : iface->mePlayerEngineState
    //           (+0x2858 == 10328 and +0x285C == 10332 are the two words this tree already
    //            names mePlayerActiveRaceCarIndex / mePlayerEngineState on the interface,
    //            and `li r10,4` / `li r17,2` are literally E_..._COUNT and E_..._RUNNING)
    //   lwz r11,dword_82F241F8 ; cmpw ; beq -> the whole leg is skipped unless the RAW
    //        state word changed. The latch is a file-scope static: image bytes at
    //        0x82F241F8 read 00 00 00 04 (x360rd) == E_..._ENGINE_STATE_COUNT, so the very
    //        first publish (OFF, 0) moves the latch but posts NOTHING -- both sides of the
    //        on/off test are false. That is the console's own no-spurious-post behaviour.
    //   subf r11,r17,r11 ; cntlzw ; extrwi 1,26   (twice: once on the latch, once on the
    //        fresh state) -> lbWasOn = (latch == RUNNING), lbIsOn = (leState == RUNNING)
    //   cmplw ; beq -> POST ONLY WHEN THE BOOLEAN FLIPS (a STARTING->STOPPING move changes
    //        the raw word but not the bool, and the console stays quiet for it)
    //   cntlzw ; extrwi 1,26 ; xori 1 ; stw -> the payload word is the bool itself
    //        (the cntlzw/xori pair is just the compiler round-tripping it): 1 == E_ENGINE_ON.
    //   stw r11,dword_82F241F8 -> latch := the RAW state word (recomputed, -1-guarded),
    //        AFTER the post and OUTSIDE the bool test -- so it tracks every raw move.
    {
        // (EActiveRaceCarEngineState lives at BrnWorld::RaceCarEntityModuleIO namespace scope,
        //  not inside the interface class.)
        const s32 KI_ENGINE_STATE_COUNT =
            static_cast<s32>(BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_COUNT);
        const s32 KI_ENGINE_STATE_RUNNING =
            static_cast<s32>(BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING);

        const s32 liEngineState =
            (lpActiveInterface->GetPlayerActiveRaceCarIndex() == E_ACTIVE_RACE_CAR_INDEX_INVALID)
                ? KI_ENGINE_STATE_COUNT
                : static_cast<s32>(lpActiveInterface->GetPlayerEngineState());

        static s32 lseLastEngineState = KI_ENGINE_STATE_COUNT;   // console dword_82F241F8 == 4
        if (liEngineState != lseLastEngineState)
        {
            const bool lbIsOn  = (liEngineState      == KI_ENGINE_STATE_RUNNING);
            const bool lbWasOn = (lseLastEngineState == KI_ENGINE_STATE_RUNNING);
            if (lbIsOn != lbWasOn)
            {
                BrnGui::GuiPlayerEngineEvent lEngineEvent;
                *reinterpret_cast<s32*>(lEngineEvent.maData) =
                    lbIsOn ? BrnGui::GuiPlayerEngineEvent::E_ENGINE_ON
                           : BrnGui::GuiPlayerEngineEvent::E_ENGINE_OFF;

                CGS_ASSERT(lpGuiInputBuffer != 0, "Input hasn't been locked for write");
                lpGuiInputBuffer->GetGuiEvents()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lEngineEvent),
                    lEngineEvent.GetEventType(),
                    static_cast<s32>(sizeof(lEngineEvent)));

                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[hud-reveal] posting GUI 379 GuiPlayerEngineEvent engineOn="
                        << (lbIsOn ? 1 : 0) << " (raw state " << lseLastEngineState
                        << " -> " << liEngineState << ")\n";
                }
            }
            lseLastEngineState = liEngineState;
        }
    }

    if (!lpActiveInterface->IsPlayerCarActive())
        return;

    const EActiveRaceCarIndex lePlayerIndex =
        lpWorldOutputBuffer->GetPlayerActiveRaceCarIndex();
    CGS_ASSERT(lePlayerIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");   // :304

    // ---- THE PLAYER RACE-CAR-ID POST (GUI event 376 -> GuiCache case 376) ------------
    // [hud H3b tracking slice 2026-08-25] the console seat between the index assert and
    // the boost post: {active, global} with the :316/:319 range asserts. The global index
    // read (UpdateOutputBuffer::GetPlayerGlobalRaceCarIndex) carries the "Player car index
    // hasn't been set" assert inside its own body. The cache's case-376 store is the
    // +19200/+19204 pair that GATES the case-199 player-icon store -- without this post
    // the satnav player position never lands in the cache.
    {
        CGS_ASSERT(lePlayerIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");   // :316
        const EGlobalRaceCarIndex lePlayerGlobalIndex =
            lpWorldOutputBuffer->GetPlayerGlobalRaceCarIndex();
        CGS_ASSERT(lePlayerGlobalIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                   "lePlayerGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");   // :319

        BrnGui::GuiPlayerRaceCarIdEvent lIdEvent;
        lIdEvent.mePlayerActiveRaceCarIndex = static_cast<s32>(lePlayerIndex);
        lIdEvent.mePlayerGlobalRaceCarIndex = static_cast<s32>(lePlayerGlobalIndex);
        // (direct queue push -- the member AddGuiEvent<T> @0x823DA458 folds to
        // AddEvent(&event, 376, 8); the 206 post above documents the convention)
        CGS_ASSERT(lpGuiInputBuffer != 0, "Input hasn't been locked for write");   // CgsGuiModule.h:286
        lpGuiInputBuffer->GetGuiEvents()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lIdEvent),
            lIdEvent.GetEventType(),
            static_cast<s32>(sizeof(lIdEvent)));
    }

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

    // ---- THE HUD UPDATE POST (GUI event 147 -> GuiCache case 147) --------------------
    // [hud H3b tracking slice 2026-08-25] the console seat after the boost post: the
    // player RaceCarState's {(s32)mfSpeedMPH, (s32)mfRPM, mi8Gear} (BE stores +0/+4/+8).
    // The cache's case-147 store (+19208) is the GuiPlayerInfo miSpeedMph the satnav
    // component's view-distance/zoom math reads.
    {
        const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState =
            lpActiveInterface->GetRaceCarState(lePlayerIndex);
        if (lpRaceCarState == 0)
        {
            CgsDev::Assert::BeginAssert();
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            lacMessage[0] = 0;
            CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Invalid race car state in BrnGameModule::BridgeWorldToGui";
            CgsDev::Assert::FireAssert(lacMessage, KPC_ASSERT_FILE, 359);
            CgsDev::Assert::EndAssert();
        }
        else
        {
            BrnGui::GuiEventUpdateHud lHudEvent;
            lHudEvent.miSpeedMph = static_cast<s32>(lpRaceCarState->mfSpeedMPH);  // @972
            lHudEvent.miRPM      = static_cast<s32>(lpRaceCarState->mfRPM);       // @984
            lHudEvent.mi8Gear    = lpRaceCarState->mi8Gear;                       // @1092
            lHudEvent.mau8Pad[0] = 0; lHudEvent.mau8Pad[1] = 0; lHudEvent.mau8Pad[2] = 0;
            // (direct queue push -- AddGuiEvent<GuiEventUpdateHud> @0x823DA5C8 ==
            // AddEvent(&event, 147, 12))
            lpGuiInputBuffer->GetGuiEvents()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lHudEvent),
                lHudEvent.GetEventType(),
                static_cast<s32>(sizeof(lHudEvent)));
        }
    }

    // FLAG deferred (console order, between the HUD post and the satnav post): the
    // GuiEventPlayerWrecked edge (off the module's player-wrecked byte) -- its consumer
    // is not on this build's reconstructed path yet.

    // ---- THE SATNAV ICON POST (GUI event 199 -> GuiCache case 199) -------------------
    // [hud H3b tracking slice 2026-08-25] the console's per-frame icon publish
    // (@0x823E5F30..0x823E64E8): walk the GLOBAL interface's occupied-slot bit array
    // (capped at 48 icons), fill one SatNavIconInfo per car -- position lane, speed in
    // MPH (the global speeds ride in m/s; flt_830180B0 == 2.2369363 scales back),
    // county/district, the heading angle vs north off the car's At vector, the active
    // index byte and the type arm (player 0 / rival-AI 3 + model id / network 2 + model
    // id / else the "Unknown car type" assert) -- then post the whole record.
    // The console's DoWorstCase pre-pass is gated on the GuiDebugComponent's worst-case
    // HUD toggle (BSS byte @0x82FB508A, default 0, written only by UpdateWorstCaseHUD
    // @0x8250CBC8); the debug component is not on this build, so the gate is constant
    // false and the call is FLAG-omitted with it, not paraphrased.
    {
        using BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface;

        const RCEntityGlobalRaceCarOutputInterface* lpGlobalInterface =
            lpWorldOutputBuffer->GetRaceCarGlobalOutputInterface();
        CGS_ASSERT(lpGlobalInterface != 0, "lpGlobalRaceCarOutput");            // :410

        if (lpGlobalInterface != 0)
        {
            const f32 KF_MPS_TO_MPH = 2.2369363f;   // flt_830180B0 (the cinit-stored 1/0.44704)

            static BrnGui::GuiEventUpdateSatNav lSatNavEvent;   // 2320B record; static, not
            // stack -- the single-threaded build's oversized-event-local convention.
            s32 liNumIcons = 0;

            const CgsContainers::BitArray<35u> lOccupied =
                lpGlobalInterface->GetGlobalRaceCarBitArray();

            for (s32 liGlobal = 0;
                 liGlobal < E_GLOBAL_RACE_CAR_INDEX_COUNT &&
                 liNumIcons < BrnGui::GuiEventUpdateSatNav::KI_MAX_SAT_NAV_ICONS;
                 ++liGlobal)
            {
                if (!lOccupied.IsBitSet(static_cast<u32>(liGlobal)))
                    continue;

                const EGlobalRaceCarIndex leGlobal =
                    static_cast<EGlobalRaceCarIndex>(liGlobal);
                BrnGui::GuiEventUpdateSatNav::SatNavIconInfo& lrIcon =
                    lSatNavEvent.maIconInfo[liNumIcons];

                // Default type first (the console's `stb 3, +0x28` before the arms).
                lrIcon.SetIconType(
                    BrnGui::GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_RIVAL);

                // Position lane (lvx128 16*idx / stvx128 into the icon head).
                const Vector3 lPosition = lpGlobalInterface->GetRaceCarPosition(leGlobal);
                Vector4 lv4Lane;
                lv4Lane.x = lPosition.x; lv4Lane.y = lPosition.y;
                lv4Lane.z = lPosition.z; lv4Lane.w = 0.0f;
                lrIcon.SetPositionLane(lv4Lane);

                // Speed (m/s -> MPH; `lfs speeds[idx]` * flt_830180B0).
                lrIcon.SetSpeedMph(
                    lpGlobalInterface->GetRaceCarSpeed(leGlobal) * KF_MPS_TO_MPH);

                // County / district off the world-region pair (@+0x460 stride 8).
                const BrnWorld::WorldRegion lRegion =
                    lpGlobalInterface->GetWorldRegion(leGlobal);
                lrIcon.SetCounty(lRegion.GetCounty());     // asserts leCounty >= 0 (:1857)
                lrIcon.SetDistrict(lRegion.GetDistrict()); // asserts leDistrict >= 0 (:1873)

                // Heading: normalise the At vector; NaN lanes -> 0.0 (the console's three
                // vcmpeqfp self-tests). Otherwise the angle vs NORTH (0,0,1) via
                // acos(clamp(dot,-1,1)) (XMVectorACos), sign-resolved against UP (0,1,0)
                // through the cross product: below the plane -> 2pi - angle. Constants:
                // unk_82181520 = (0,0,1), unk_82181510 = (0,1,0), 0x82034E30 = 2pi.
                {
                    const Vector3 lAt = lpGlobalInterface->GetRaceCarAt(leGlobal);
                    const bool lbNaN = (lAt.x != lAt.x) || (lAt.y != lAt.y) ||
                                       (lAt.z != lAt.z);
                    if (lbNaN)
                    {
                        lrIcon.SetRotation(0.0f);
                    }
                    else
                    {
                        const f32 lfLenSq = lAt.x * lAt.x + lAt.y * lAt.y + lAt.z * lAt.z;
                        f32 lfRotation = 0.0f;
                        if (lfLenSq > 0.0f)
                        {
                            const f32 lfInvLen = 1.0f / sqrtf(lfLenSq);
                            // dot(normalised At, north(0,0,1)) == At.z / |At|
                            f32 lfDot = lAt.z * lfInvLen;
                            if (lfDot > 1.0f)  lfDot = 1.0f;
                            if (lfDot < -1.0f) lfDot = -1.0f;
                            lfRotation = acosf(lfDot);
                            // cross(dHat, north) . up == -dHat.x ; below the plane flips.
                            const f32 lfSide = -lAt.x * lfInvLen;
                            if (lfSide < 0.0f)
                            {
                                const f32 KF_TWO_PI = 6.2831855f;  // 0x82034E30 lane 0
                                lfRotation = KF_TWO_PI - lfRotation;
                            }
                        }
                        lrIcon.SetRotation(lfRotation);
                    }
                }

                lrIcon.SetHiddenDriveThru(false);          // `stb 0, +0x23`

                // Active index byte (`stb activeIdx[idx], +0x26`).
                lrIcon.SetActiveRaceCarIndex(
                    lpGlobalInterface->GetActiveRaceCarIndex(leGlobal));

                // The type arms (IsPlayer / IsRivalAI+modelId / IsNetwork+modelId).
                if (lpGlobalInterface->IsPlayer(leGlobal))
                {
                    lrIcon.SetIconType(
                        BrnGui::GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR);
                }
                else if (lpGlobalInterface->IsRivalAI(leGlobal))
                {
                    lrIcon.SetIconType(
                        BrnGui::GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_RIVAL);
                    lrIcon.SetCgsId(lpGlobalInterface->GetCarModelId(leGlobal));
                }
                else if (lpGlobalInterface->IsNetwork(leGlobal))
                {
                    lrIcon.SetIconType(
                        BrnGui::GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_NETWORKRIVAL);
                    lrIcon.SetCgsId(lpGlobalInterface->GetCarModelId(leGlobal));
                }
                else
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert("Unknown car type", KPC_ASSERT_FILE, 466);
                    CgsDev::Assert::EndAssert();
                }

                CGS_ASSERT(lrIcon.GetRotation() == lrIcon.GetRotation(),
                           "!RwMath::IsNaN( lpIcon->mfRotation )");             // :469

                ++liNumIcons;
            }

            lSatNavEvent.miNumIcons = liNumIcons;          // @+0x900 (the case-199 count)

            // [FLAG] `if (byte_82FB508A) DoWorstCase()` omitted -- see the banner above.

            // (direct queue push -- AddGuiEvent<GuiEventUpdateSatNav> @0x823D93A0 ==
            // AddEvent(&event, 199, 2320))
            lpGuiInputBuffer->GetGuiEvents()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lSatNavEvent),
                lSatNavEvent.GetEventType(),
                static_cast<s32>(sizeof(lSatNavEvent)));

            // [DIAG] NOT IN THE X360 BINARY -- [satnav-diag] the first two 199 posts.
            {
                static s32 siLeft = 2;
                if (siLeft > 0 && CgsDev::Log::gpDebugPrint != 0 && liNumIcons > 0)
                {
                    --siLeft;
                    const BrnGui::GuiEventUpdateSatNav::SatNavIconInfo& lrFirst =
                        lSatNavEvent.maIconInfo[0];
                    *CgsDev::Log::gpDebugPrint
                        << "[satnav-diag] bridge 199: icons=" << liNumIcons
                        << " first type=" << static_cast<s32>(lrFirst.GetIconTypeByte())
                        << " pos=(" << lrFirst.GetPositionLane().x
                        << "," << lrFirst.GetPositionLane().z
                        << ") rot=" << lrFirst.GetRotation()
                        << "\n";
                }
            }
        }
    }

    // ---- THE RACE-CAR-INFO POST (GUI event 207 -> GuiCache case 207) -----------------
    // [hud H3b tracking slice 2026-08-25] the console's closing leg (@0x823E64E8..): a
    // Construct'ed GuiRaceCarInfoEvent filled per occupied GLOBAL slot that carries a
    // valid ACTIVE index -- position lane by ACTIVE index, the rival-id identity qword,
    // and the five flag bytes {used=1, connecting(flags&0x20), disconnected(flags&0x80),
    // inRange, crashing(active state mbCrashing when flags&1)}. The cache's case-207
    // consumption of this record IS the mRaceCarInfo SoA (maRaceCarUsed and friends) --
    // the IsActiveRaceCarIndexUsed gate on the case-199 player store reads it, and the
    // sat-nav renderer's rival icons read GetRaceCarPosition off it.
    {
        using BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface;

        const RCEntityGlobalRaceCarOutputInterface* lpGlobalInterface =
            lpWorldOutputBuffer->GetRaceCarGlobalOutputInterface();
        if (lpGlobalInterface != 0)
        {
            static BrnGui::GuiRaceCarInfoEvent lInfoEvent;   // 240B; the oversized-local convention
            lInfoEvent.Construct();
            s32 liNumEntries = 0;

            const CgsContainers::BitArray<35u> lOccupied =
                lpGlobalInterface->GetGlobalRaceCarBitArray();

            for (s32 liGlobal = 0; liGlobal < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++liGlobal)
            {
                if (!lOccupied.IsBitSet(static_cast<u32>(liGlobal)))
                    continue;

                const EGlobalRaceCarIndex leGlobal =
                    static_cast<EGlobalRaceCarIndex>(liGlobal);
                const EActiveRaceCarIndex leActive =
                    lpGlobalInterface->GetActiveRaceCarIndex(leGlobal);
                CGS_ASSERT(leActive < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                           "leCurrentActiveCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT"); // :499
                if (leActive == E_ACTIVE_RACE_CAR_INDEX_INVALID ||
                    leActive >= E_ACTIVE_RACE_CAR_INDEX_COUNT)
                    continue;

                const Vector3 lPosition = lpGlobalInterface->GetRaceCarPosition(leGlobal);
                Vector4 lv4Lane;
                lv4Lane.x = lPosition.x; lv4Lane.y = lPosition.y;
                lv4Lane.z = lPosition.z; lv4Lane.w = 0.0f;

                // The crashing byte rides only on active (flags bit 0) cars; the console
                // reads GetRaceCarState(active)->mbCrashing (@1098) under that gate.
                bool lbCrashing = false;
                if (lpActiveInterface->IsRaceCarActive(leActive))
                {
                    const BrnPhysics::Vehicle::RaceCarState* lpState =
                        lpActiveInterface->GetRaceCarState(leActive);
                    lbCrashing = (lpState != 0) && lpState->mbCrashing;
                }

                lInfoEvent.SetEntry(
                    static_cast<s32>(leActive),
                    lv4Lane,
                    lpGlobalInterface->GetRivalId(leGlobal),          // the identity qword
                    true,                                             // used
                    lpActiveInterface->IsCarConnecting(leActive),     // flags & 0x20
                    lpActiveInterface->IsCarDisconnected(leActive),   // flags & 0x80
                    lpGlobalInterface->IsInRange(leGlobal),
                    lbCrashing);
                ++liNumEntries;
            }

            lInfoEvent.SetNumEntries(liNumEntries);

            // (direct queue push -- AddGuiEvent<GuiRaceCarInfoEvent> @0x823DA738 ==
            // AddEvent(&event, 207, 240))
            lpGuiInputBuffer->GetGuiEvents()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lInfoEvent),
                lInfoEvent.GetEventType(),
                static_cast<s32>(sizeof(lInfoEvent)));
        }
    }

    // FLAG deferred (console order, after the race-car-info post): the stunt-info post
    // (377) and the remaining per-frame vehicle telemetry. Each lands with its consumer.
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
