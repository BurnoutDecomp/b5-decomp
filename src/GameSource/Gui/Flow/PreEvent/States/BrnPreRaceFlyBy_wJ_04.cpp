// ===================================================================================
// BrnGui::PreRaceFlyByState -- wave-J partfile 04: the state machine + its icon resolver.
//   Update               @0x824DC540  (DWARF BrnPreRaceFlyBy.cpp:308)
//   SetEventIconResource @0x824BB5F8  (DWARF BrnPreRaceFlyBy.cpp:1689)
//
// Both bodies were asm-walked from BURNOUT_X360_ARTIST.XEX (Update:
// scratchpad/waveJ/asm_update.txt), the raw `assembly` array arbitrating over Hex-Rays.
//
// 2026-08-03 RECONCILIATION: this partfile was parked behind exactly three missing
// declarations -- GuiEventShowHideSatNav's real MapType/mfFadeTime/mbShow payload,
// StateInterface::OutputViewState/OutputInternalState (channels 41/42), and
// SatNavIconInfo's landmark CgsID at +0x10 with its GetCgsId() accessor. All three have
// landed, so both bodies are here. The shapes those additions had to preserve are pinned
// by their own headers now: sizeof(SatNavIconInfo) == 0x30 and
// sizeof(GuiEventShowHideSatNav) == 12 (the RAW payload -- the 24-byte console record is
// the GuiEventWrapper<T,41|42> the two StateInterface templates build around it).
//
// LINK NOTE for the conductor -- `cl /c` cannot see unresolved externals, so gate-green is
// not link-green. Split by what the tree actually holds today:
//
//   (a) ALREADY LINK-SATISFIED, by this class's own wave-J partfiles:
//       TriggerExitState / AppendExpectedComponents / UpdateIconManager and EVERY static
//       member definition (BrnPreRaceFlyBy_wJ_01.cpp); SetupComponents /
//       HandleIncomingEvents / HandleAptEvents (wJ_03); CalculateZoomFactor /
//       FindEventDirection (wJ_05); the five Set*Description workers (wJ_06 / wJ_07).
//
//   (b) RESOLVED 2026-08-26 (wave E1) -- was 'defined, but in the non-compiling HUD
//       fork'. IsMapApplicableToGameMode / IsMapPanApplicableToGameMode (and the ctor)
//       moved into BrnPreRaceFlyBy_wJ_01.cpp when
//       GameSource/Gui/Flow/HUD/States/BrnPreRaceFlyBy.{h,cpp} was deleted and
//       BrnHudFlow.cpp was re-pointed at this class's real header. Now (a).
//
//   (c) STILL UNDEFINED -- the three callees of THESE two bodies that are
//       declaration-only in the tree (checked for both an out-of-line body and a
//       header-inline one):
//       MainMapComponent::Update and MainMapComponent::SetZoom (declared BrnMainMap.h:104
//       / :110; BrnMainMap.cpp:34 defines only RecvEvent), and
//       GuiCache::GetLandmarkInfoFromIndex (declared BrnGuiCache.h:421).
//       StateInterface::PlayAptMovie (CgsGuiStateInterface.cpp:46),
//       GuiComponent::AddOutputAptViewState (CgsGuiComponent.cpp:40),
//       GuiCache::GetEventDestinationLandmarkIndex (BrnGuiCache_wB_res.cpp:69) and
//       gGuiResourceIdentifier (BrnGuiCache.cpp:37) are all DEFINED -- they are named here
//       only because an earlier revision of this note wrongly listed them as missing.
// ===================================================================================

#include "GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (PlayAptMovie / the out queue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue::AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventUpdateSatNav::SatNavIconInfo / GuiFlow
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventShowHideSatNav

namespace GSM = BrnGameState::GameStateModuleIO;

namespace BrnGui
{

// -------------------------------------------------------------------------------------
// SetEventIconResource @ 0x824BB5F8 (DWARF cpp:1689) -- pick the large event icon the
// fly-by loads, from the game mode (and, for the two landmark-destination modes, from the
// destination landmark's CgsID).
//
// Both streamed asserts are lowered to CGS_ASSERT with their static text, per project
// policy; the value the console streamed after the text is the switch scrutinee itself.
// Neither is fatal -- the console runs on and stores the north icon.
// -------------------------------------------------------------------------------------
void PreRaceFlyByState::SetEventIconResource()
{
    CGS_ASSERT(mpGuiCache, "mpGuiCache");                                     // cpp:1694
    CGS_ASSERT(mpGuiCache->GetGameMode() >= GSM::E_MODE_NONE,                 // cpp:1695
               "mpGuiCache->GetGameMode() >= BrnGameState::GameStateModuleIO::E_MODE_NONE");
    CGS_ASSERT(mpGuiCache->GetGameMode() < GSM::E_MODE_OFFLINE_COUNT,         // cpp:1696
               "mpGuiCache->GetGameMode() < BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_COUNT");

    switch (static_cast<GSM::EGameModeType>(mpGuiCache->GetGameMode()))
    {
        case GSM::E_MODE_OFFLINE_RACE:
        case GSM::E_MODE_MARKED_MAN:
        {
            // The two modes that fly by to a named destination: the icon is the compass
            // point baked into the destination landmark's id.
            GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
            mpGuiCache->GetLandmarkInfoFromIndex(mpGuiCache->GetEventDestinationLandmarkIndex(),
                                                 &lLandmarkInfo);

            switch (lLandmarkInfo.GetCgsId())
            {
                case 0x880E4: mLargeIconResource.muId = 108; break;  // gGuiResourceIdentifier[108] == "DestSW"
                case 0x880E5: mLargeIconResource.muId = 109; break;  // "DestS"
                case 0x880E6: mLargeIconResource.muId = 107; break;  // "DestW"
                case 0x880E7: mLargeIconResource.muId = 110; break;  // "DestSE"
                case 0x880E8: mLargeIconResource.muId = 112; break;  // "DestNE"
                case 0x880E9: mLargeIconResource.muId = 105; break;  // "DestN"
                case 0x880EA: mLargeIconResource.muId = 106; break;  // "DestNW"
                case 0x88109: mLargeIconResource.muId = 111; break;  // "DestE"
                default:
                    // cpp:1723. Non-fatal: the console falls through to the north icon.
                    CGS_ASSERT(false, "Invalid destination ID (skippable) - ");
                    mLargeIconResource.muId = 105;                   // "DestN"
                    break;
            }
            break;
        }

        case GSM::E_MODE_ROAD_RAGE:
            mLargeIconResource.muId = 113;                           // "LargeRoadRageIcon"
            break;

        case GSM::E_MODE_STUNT_ATTACK:
            mLargeIconResource.muId = 114;                           // "LargeFreestyleIcon"
            break;

        case GSM::E_MODE_BURNING_ROUTE:
            mLargeIconResource.muId = 115;                           // "LargeBurningRouteIcon"
            break;

        default:
            // cpp:1751. Non-fatal, same north-icon fallback.
            CGS_ASSERT(false, "Invalid game mode (skippable) - ");
            mLargeIconResource.muId = 105;                           // "DestN"
            break;
    }
}

namespace
{
    // id 163 -- the one-shot "fly-by time expired" GUI-out post the Update tail latches
    // behind mbEndRequestSent. The X360 stack-builds {1, 163, 12} at
    // 0x824DC988..0x824DC9A0 and calls AddEvent(record, 40, 16) (`li r6,0x10` /
    // `li r5,0x28`): header word 0 == the PAYLOAD size (1), word 2 == the payload offset
    // (12). The 1-byte payload at +12 is never written by the emitter; the record is 16
    // because a 1-byte payload at +12 pads to 16, which is the AddEvent size. The house
    // shape for a 1-byte payload is BrnGuiDemangledEventTypes.h's GuiEventShowHideBoostBar
    // (id 214 size 1).
    // FLAG: the type name is consumer-derived (no DWARF row survives for X360 id 163), and
    // the payload byte's meaning is not recovered -- nothing in the image ever stores it.
    // File-local by the PausedHudState precedent; each wave-J partfile that posts it
    // carries its own anonymous-namespace copy, so there is no link collision.
    struct GuiEventPreRaceFlyByTimeExpired : public CgsGui::GuiEvent<163>
    {
        u8 mu8Payload;   // +0x0C -- the 1-byte payload the size word names; UNWRITTEN by the emitter

        GuiEventPreRaceFlyByTimeExpired()
            : CgsGui::GuiEvent<163>(
                  static_cast<u32>(sizeof(mu8Payload)),                                   // X360 word0 == 1
                  static_cast<u32>(offsetof(GuiEventPreRaceFlyByTimeExpired, mu8Payload))) // X360 word2 == 12
            , mu8Payload(0)
        {
        }
    };

    // Host layout pin: CgsModule::Event is an empty base and CgsGui::GuiEvent<N> is the
    // three header words, so the record is 16 bytes on the host exactly as on the console
    // -- the AddEvent size argument below is a host sizeof, never the console's baked 16.
    static_assert(sizeof(GuiEventPreRaceFlyByTimeExpired) == 16,
                  "host record matches the X360 16-byte id-163 post");

    // The AddEvent channel the X360 passes for a GUI out-event post (`li r5, 0x28`).
    // Same channel the committed OutputGuiEvent<T> instantiations use.
    const s32 KI_GUI_OUT_EVENT_CHANNEL = 40;

    // The ShowHideSatNav fade the LOADING arm requests. The X360 loads the POOLED 0.5f
    // (flt_82065668), not the class static KF_MAP_FADEIN_TIME (@0x82065D58, same value),
    // so the source literal is reproduced here rather than the named constant.
    const f32 KF_SATNAV_FADEIN_TIME = 0.5f;

    // The debug-print category bit every gated log line in this TU tests
    // (`ld gxMessageFilterFlags; clrldi r11,r11,63`).
    const u64 KX_MESSAGE_FILTER_DEBUG = 1;
}

// -------------------------------------------------------------------------------------
// Update @ 0x824DC540 (DWARF cpp:308) -- the per-frame fly-by state machine.
//
// Reconstructed from the raw asm (scratchpad/waveJ/asm_update.txt), arbitrated over
// Hex-Rays. Notes taken from the asm rather than the pseudocode:
//  * The UNLOADED arm's inner switch is a 14-case jump table on the game mode with THREE
//    distinct arms: {0,3,5,6,7,8,9} run the load sequence, {10,11,13} branch STRAIGHT to
//    the common tail (0x824DC8E8) doing nothing at all, and every other value -- {1,2,4,12}
//    and anything above 13, which the unsigned `cmplwi 0xD` also catches for negatives --
//    calls TriggerExitState first.
//  * SetZoom's zoom argument is a float: the asm sets r4=3 and r6=1 and SKIPS r5
//    (0x824DC83C..0x824DC848), because a float travels in f1 and forfeits its GPR slot.
//    Register numbers are not parameter positions.
//  * NaN polarity, twice. The ICON_DELAY compare is `fcmpu; bge <skip>`, and `bge` is taken
//    when unordered, so the ordered `<` that is FALSE for NaN matches it exactly. The tail
//    countdown is `fcmpu; bgt <skip>`, taken when unordered, so the one-shot gate must be
//    written `!(mfTimeRemaining > 0.0f)` -- a naive `<= 0.0f` would post the event on a NaN
//    the console skips.
//  * Every console offset in the listing (0x978 meCurrentState, 0x988 mpGuiCache,
//    0x9E58 the cache's game-mode word, 0x1C mpStateInterface, ...) is a 32-bit console
//    offset and is NOT reproduced: every member is reached by name.
// -------------------------------------------------------------------------------------
void PreRaceFlyByState::Update()
{
    switch (meCurrentState)
    {
        case E_PRERACE_UNLOADED:
        {
            const GSM::EGameModeType leGameMode =
                static_cast<GSM::EGameModeType>(mpGuiCache->GetGameMode());

            switch (leGameMode)
            {
                case GSM::E_MODE_OFFLINE_RACE:
                case GSM::E_MODE_ROAD_RAGE:
                case GSM::E_MODE_BURNING_ROUTE:
                case GSM::E_MODE_ELIMINATOR:
                case GSM::E_MODE_STUNT_ATTACK:
                case GSM::E_MODE_MARKED_MAN:
                case GSM::E_MODE_TRAFFIC_ATTACK:
                {
                    // cpp:328 / cpp:329. Both are unreachable from inside this arm (the
                    // switch already bounded the mode) but the X360 emits them, so they
                    // are part of the source.
                    CGS_ASSERT(mpGuiCache->GetGameMode() >= 0,
                               "mpGuiCache->GetGameMode() >= 0");
                    CGS_ASSERT(mpGuiCache->GetGameMode() < GSM::E_MODE_OFFLINE_COUNT,
                               "mpGuiCache->GetGameMode() < BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_COUNT");

                    const CgsGui::sResourceTuple lPerModeScreen = maPerGamemodeScreens[leGameMode];

                    mbDoMapPan = IsMapPanApplicableToGameMode(leGameMode);

                    if (mLargeIconResource.muId == 0)
                    {
                        SetEventIconResource();
                        CGS_ASSERT(0 != mLargeIconResource.muId,   // cpp:338
                                   "0 != mLargeIconResource.muId");
                    }

                    // The gate is short-circuiting in the asm: each of the three tests
                    // branches straight to the common tail on false.
                    if (mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad)
                        && mpGuiCache->EnsureResourceIsLoaded(lPerModeScreen)
                        && mpGuiCache->EnsureResourceIsLoaded(mLargeIconResource))
                    {
                        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[lPerModeScreen.muId], 2);
                        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[mLargeIconResource.muId], 1);
                        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_HUD);
                        AppendExpectedComponents();
                        meCurrentState = E_PRERACE_LOADING_COMPONENTS;
                    }
                    break;
                }

                case GSM::E_MODE_ONLINE_RACE:               // 10 (== E_MODE_ONLINE_MODE_START)
                case GSM::E_MODE_ONLINE_ROAD_RAGE:          // 11
                case GSM::E_MODE_ONLINE_BURNING_HOME_RUN:   // 13
                    // The three online modes that own a pre-race fly-by run no load
                    // sequence here -- the asm jumps them straight to the common tail.
                    break;

                default:
                    // {1, 2, 4, 12} and every out-of-range mode: nothing to fly by.
                    TriggerExitState();
                    break;
            }
            break;
        }

        case E_PRERACE_LOADING_COMPONENTS:
            if (mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_HUD))
            {
                const GSM::EGameModeType leGameMode =
                    static_cast<GSM::EGameModeType>(mpGuiCache->GetGameMode());

                SetupComponents();

                mfTimeRemaining = KAF_MODE_TYPE_PRE_EVENT_DURATION[leGameMode];
                meCurrentState  = E_PRERACE_ACTIVE_EVENT_TITLES;
                mStateAnimator.AddOutputAptViewState("apt_Transition", "titlebarsIn", false);

                if (IsMapApplicableToGameMode(leGameMode))
                {
                    // The three payload words go in through the type's own writer. Its
                    // parameter order is the DWARF's (map type, show, fade time) while the
                    // payload order is (map type, fade time, show) -- see the note on
                    // GuiEventShowHideSatNav::Construct. The map-type enumerator is that
                    // type's own MapType, not MainMapComponent's EMapType (both spell the
                    // main map as 0, which is the value the X360 stores here).
                    GuiEventShowHideSatNav lShowSatNav;
                    lShowSatNav.Construct(GuiEventShowHideSatNav::E_MAPTYPE_MAIN, true,
                                          KF_SATNAV_FADEIN_TIME);
                    mpStateInterface->OutputViewState(lShowSatNav);
                    mpStateInterface->OutputInternalState(lShowSatNav);

                    // PPC float ABI: the zoom rides in f1 and skips r5 -- r4 is the zoom
                    // mode (3 == E_ZOOMFACTOR_CUSTOM) and r6 the apply-now flag.
                    mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_CUSTOM,
                                              CalculateZoomFactor(), true);

                    mfIconAnimationStartTime = mpGuiCache->GetTime();
                    meCurrentState = E_PRERACE_ACTIVE_MAP_ICON_DELAY;
                }
            }
            break;

        case E_PRERACE_ACTIVE_MAP_ICON_DELAY:
        {
            // fcmpu + bge-skip: `<` is the ordered predicate that matches the console's
            // unordered (NaN) behaviour -- both fall through to the title bars only when
            // the comparison is ordered and true.
            const f32 lfIconAnimationEndTime =
                KAF_ICON_ANIMATION_DELAY[mpGuiCache->GetGameMode()] + mfIconAnimationStartTime;

            if (lfIconAnimationEndTime < mpGuiCache->GetTime())
            {
                mfIconAnimationStartTime = mpGuiCache->GetTime();
                meCurrentState = E_PRERACE_ACTIVE_EVENT_TITLES;
            }
            break;
        }

        case E_PRERACE_ACTIVE_EVENT_TITLES:
        case E_PRERACE_ACTIVE_MAP_INTRO:
        case E_PRERACE_ACTIVE_SHOW_MAP:
        case E_PRERACE_ACTIVE_MEDALS:
        case E_PRERACE_ACTIVE_TRANS_OUT:
        case E_PRERACE_ACTIVE_DONE:
            // The animated states are driven entirely by the incoming apt events.
            break;

        default:
            // E_PRERACE_INVALID lands here too: the switch bound is the UNSIGNED
            // `cmplwi cr6, r28, 8`, so -1 takes the default arm.
            if ((CgsDev::Message::gxMessageFilterFlags & KX_MESSAGE_FILTER_DEBUG) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Invalid state"
                                           << static_cast<s32>(meCurrentState) << "\n";
            }
            break;
    }

    // ---- common tail (0x824DC8E8): every arm above, plus the {10,11,13} shortcut ----
    if (meCurrentState != E_PRERACE_INVALID)
    {
        if (IsMapApplicableToGameMode(static_cast<GSM::EGameModeType>(mpGuiCache->GetGameMode())))
        {
            mv2WorldCenterPoint = mMainMapComponent.Update(mv2WorldCenterPoint);

            if (meCurrentState > E_PRERACE_ACTIVE_MAP_ICON_DELAY)
            {
                UpdateIconManager();
            }
        }
    }

    if (meCurrentState > E_PRERACE_LOADING_COMPONENTS)
    {
        mfTimeRemaining -= mpGuiCache->GetTimeStep();

        // `bgt` skips the post, and `bgt` is TAKEN when the compare is unordered, so the
        // gate is the negated ordered predicate -- NOT `mfTimeRemaining <= 0.0f`.
        if (!(mfTimeRemaining > 0.0f) && !mbEndRequestSent)
        {
            GuiEventPreRaceFlyByTimeExpired lTimeExpired;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                &lTimeExpired, KI_GUI_OUT_EVENT_CHANNEL, static_cast<s32>(sizeof(lTimeExpired)));
            mbEndRequestSent = true;
        }
    }

    HandleIncomingEvents();
}

}
