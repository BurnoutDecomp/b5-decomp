// GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.cpp
//
// Created 2026-09-15 by tools/work/fold_partfiles.py --create-parent (b5-decomp issue #20).
// This family had NO parent TU: its bodies lived in 7 wave partfile(s), each
// with its own hand-written mount line in tools/build/build_game_exe.bat. They are folded
// here in MOUNT ORDER; every partfile's own header comment block is kept verbatim above
// its bodies (the address annotations are the evidence trail). No body was edited.
//
// Folded, in mount order:
//     BrnPreRaceFlyBy_wJ_01.cpp
//     BrnPreRaceFlyBy_wJ_02.cpp
//     BrnPreRaceFlyBy_wJ_03.cpp
//     BrnPreRaceFlyBy_wJ_04.cpp
//     BrnPreRaceFlyBy_wJ_05.cpp
//     BrnPreRaceFlyBy_wJ_06.cpp
//     BrnPreRaceFlyBy_wJ_07.cpp
//
// The header of the first of them (BrnPreRaceFlyBy_wJ_01.cpp) follows verbatim, as this file's own.

// ===================================================================================
// BrnGui::PreRaceFlyByState -- wave-J partfile 01: the small leaves + the TU's statics.
//   PreRaceFlyByState (ctor)     @0x82514E58
//   IsMapApplicableToGameMode    @0x824B3120  (h:331)
//   IsMapPanApplicableToGameMode @0x824B3190  (h:396)
//   TriggerExitState             @0x824C6BD0  (cpp:722)
//   AppendExpectedComponents     @0x824B4DB0  (cpp:742)
//   UpdateIconManager            @0x824C7B70  (cpp:1874)
//
// The first three landed here 2026-08-26 (wave E1) when the pre-wave fork
// GameSource/Gui/Flow/HUD/States/BrnPreRaceFlyBy.{h,cpp} was retired -- that .cpp held the
// only definitions of all three and could never be built, because it compiled against an
// empty-shell copy of this class. See each body's own note.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the raw `assembly` listing of each address
// arbitrates over the Hex-Rays pseudocode throughout.
//
// This partfile also carries EVERY out-of-line definition of PreRaceFlyByState's static
// members (the wave-J spec assigns them to group 1 so the six partfiles cannot collide).
// Every value below was read out of the image -- the dumps live in
// scratchpad/waveJ/prfb_rodata.txt (tables), prfb_init.txt (the runtime-initialised
// rects' initialiser stubs) and prfb_flts.txt (the individual float slots).
// ===================================================================================

// the union of the BrnPreRaceFlyBy_w*.cpp partfiles' #include lines, first occurrence wins, mount order (2026-09-15)
#include "GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue::AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache + GuiFlow
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // BrnGui::GuiEventShowHideHud (id 148)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiAudioTriggerEvent
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                      // BrnGui::MapIconManager
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (the streamed assert)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameSource/Gui/SatNav/BrnMainMap.h"                             // MainMapComponent + MainMapParameterBundle
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsID / CgsIDConvertToString
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // GuiEventAptTrigger(+Payload)
#include <cstring>   // strcmp (the X360 inlines it into HandleAptEvents)
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include <stdlib.h>                                                       // getenv (the [flyby] diag gate only)
#include <cmath>                                            // std::sqrt / std::floor
#include "SDKs/XboxMath/XMVectorACos.h"                      // XboxMath::XMVectorACos (X360 0x821F0980)
#include "GameSource/GameState/BrnGameStateTypes.h"          // BrnGameState::LandmarkIndex (complete: passed by value)
#include "SharedClasses/Progression/BrnRaceEventData.h"      // BrnProgression::RaceEventData (+ CheckpointData)
#include "GameSource/Gui/BrnGuiWorldDataController.h"        // GetEventInfoFromEventId
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"            // BrnGui::MapTransform
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // ParameterFormatType + the clock formatter
#include "GameSource/GameState/Progression/BrnProfile.h"                  // Profile / ProfileEvent
#include "GameSource/GameState/Progression/BrnDerivedCars.h"              // BrnProgression::DerivedCarArray
#include "GameSource/GameState/BrnGameStateSharedIO.h"                   // GSM::EGameModeType
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"   // BrnGameState::ECurrentMedalTargetTime

// ============================================================================
// FOLDED FROM BrnPreRaceFlyBy_wJ_01.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// Its header is THIS FILE'S header, at the top -- not repeated here.
// ============================================================================

namespace BrnGui
{
    // ===============================================================================
    // Static member definitions (this TU's file-scope const data).
    // ===============================================================================

    // @0x82F26BE0 -- the one resource the fly-by state asks the loader for up front.
    const CgsGui::sResourceTuple PreRaceFlyByState::maResourcesToLoad[1] =
    {
        { 200u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // {200, 7}
    };
    // @0x82F26BE8 == 1.
    const u32 PreRaceFlyByState::muNumResourcesToLoad = 1u;

    // @0x82F26C00 -- the per-game-mode pre-event screen bundle, indexed by
    // GameStateModuleIO::EGameModeType 0..9. Ids run 207..216, all of type 4
    // (E_GUI_RESOURCETYPE_APT).
    const CgsGui::sResourceTuple PreRaceFlyByState::maPerGamemodeScreens[10] =
    {
        { 207u, CgsGui::E_GUI_RESOURCETYPE_APT },   // race
        { 208u, CgsGui::E_GUI_RESOURCETYPE_APT },   // face-off
        { 209u, CgsGui::E_GUI_RESOURCETYPE_APT },   // crash / showtime
        { 210u, CgsGui::E_GUI_RESOURCETYPE_APT },   // road rage
        { 211u, CgsGui::E_GUI_RESOURCETYPE_APT },   // pursuit
        { 212u, CgsGui::E_GUI_RESOURCETYPE_APT },   // burning route
        { 213u, CgsGui::E_GUI_RESOURCETYPE_APT },   // eliminator
        { 214u, CgsGui::E_GUI_RESOURCETYPE_APT },   // stunt attack
        { 215u, CgsGui::E_GUI_RESOURCETYPE_APT },   // survival / marked man
        { 216u, CgsGui::E_GUI_RESOURCETYPE_APT },   // traffic attack
    };

    // @0x82065CAC -- the GUI event ids OnEnter registers for (and OnLeave drops).
    const s32 PreRaceFlyByState::maiEventToObserve[8] = { 6, 21, 64, 159, 160, 162, 164, 213 };
    // @0x82065CCC == 8.
    const s32 PreRaceFlyByState::miNumEventsObserved = 8;

    // Apt component names (@0x82065CD0 onward; the "flyByHud_mc" slot is @0x82065D04 and
    // the sat-nav icon base name @0x82065D40).
    const char PreRaceFlyByState::KAC_EVENT_NAME_TEXTFIELD_NAME[10] = "EventName";
    const char PreRaceFlyByState::KAC_MODE_TYPE_TEXTFIELD_NAME[9]   = "RaceType";
    const char PreRaceFlyByState::KAC_LARGE_EVENT_ICON_NAME[9]      = "destIcon";
    const char PreRaceFlyByState::KAC_STATE_ANIMATOR_NAME[14]       = "flyByAnim_cpt";
    const char PreRaceFlyByState::KAC_STATE_COMPONENT_NAME[12]      = "flyByHud_mc";
    const char PreRaceFlyByState::macSatNavIconBaseName[11]         = "SatNavIcon";

    // @0x82F26BEC -- the five description text-field component names.
    const char* const PreRaceFlyByState::KAAC_EVENT_DESC_TEXTFIELD_NAMES[5] =
    {
        "DescriptionText1",
        "DescriptionText2",
        "DescriptionText3",
        "DescriptionText4",
        "DescriptionText5",
    };

    // @0x82065D10 -- how long the titles stay up, per game mode (5.63 == 0x40B428F6).
    const f32 PreRaceFlyByState::KAF_MODE_TYPE_PRE_EVENT_DURATION[10] =
    {
        6.0f, 5.63f, 5.63f, 4.0f, 5.63f, 8.0f, 5.63f, 5.63f, 8.0f, 5.63f,
    };

    // @0x82065D38 / @0x82065D3C. NAMING NOTE: the two scalars sit between the duration
    // table and the "SatNavIcon" literal; their names come from cpp declaration-order
    // adjacency (inference), the VALUES are exact image reads.
    const f32 PreRaceFlyByState::KF_MAP_PAN_TIME       = 5.0f;
    const f32 PreRaceFlyByState::KF_MAP_PAN_RESET_TIME = 1.0f;

    // @0x82065D50..@0x82065D5C -- same adjacency-derived naming, exact values.
    const f32 PreRaceFlyByState::KF_PRERACE_MAP_VIEW_BUFFERZONE_X = 50.0f;
    const f32 PreRaceFlyByState::KF_PRERACE_MAP_VIEW_BUFFERZONE_Y = 50.0f;
    const f32 PreRaceFlyByState::KF_MAP_FADEIN_TIME               = 0.5f;
    const f32 PreRaceFlyByState::KF_MAP_FADEOUT_TIME              = 0.5f;

    // @0x82065D60 / @0x82065D88 -- the icon-reveal animation length and the delay before
    // it starts, both per game mode (1.85 == 0x3FECCCCD).
    const f32 PreRaceFlyByState::KAF_ICON_ANIMATION_TIME[10] =
    {
        1.85f, 1.85f, 1.85f, 1.85f, 1.85f, 2.0f, 1.85f, 1.85f, 2.0f, 1.85f,
    };
    const f32 PreRaceFlyByState::KAF_ICON_ANIMATION_DELAY[10] =
    {
        1.85f, 1.85f, 1.85f, 1.85f, 1.85f, 3.8f, 1.85f, 1.85f, 3.6f, 1.85f,
    };

    // The four rects are ZERO in the static image on the X360 and filled by per-static
    // initialiser stubs at boot (scratchpad/waveJ/prfb_init.txt):
    //   0x82C54BD0 -> 0x82FB4AB0 = { *0x8206B494, *0x8206B490, 0, 0 }
    //   0x82C54C10 -> 0x82FB4AA0 = { *0x8206B494, *0x8206B490, 0, 0 }
    //   0x82C54C50 -> 0x82FB4C30 = { *0x82001CC0, *0x82047A6C, *0x82001C98, *0x82069E24 }
    //   0x82C54C98 -> 0x82FB4AF0 = { *0x820662EC, *0x82047A6C, *0x82058108, *0x82069E1C }
    // with the referenced float slots read as (prfb_flts.txt)
    //   0x8206B490 = 349.79999, 0x8206B494 = 638.0, 0x82047A6C = 0.1875,
    //   0x82001C98 = 1.0, 0x82069E24 = 0.83888888, 0x820662EC = 0.4,
    //   0x82058108 = 0.9375, 0x82069E1C = 0.78333336, 0x82001CC0 = 0.0.
    const Vector4 PreRaceFlyByState::KV4_VIEW_RECT    = { 0.0f, 0.1875f, 1.0f, 0.83888888f };
    const Vector4 PreRaceFlyByState::KV4_PADDING_RECT = { 0.4f, 0.1875f, 0.9375f, 0.78333336f };

    // INFERENCE, stated plainly: which of 0x82FB4AB0 / 0x82FB4AA0 is LONG and which is
    // TALL is taken from initialiser-stub order == declaration order (the lower stub
    // address, 0x82C54BD0 -> 0x82FB4AB0, is the first-declared). The two rects hold
    // IDENTICAL values, so CalculateZoomFactor's pick between them is value-neutral and
    // the naming cannot be settled from the image. The z/w lanes are the `std r9, 0(r11)`
    // zero pair in both stubs.
    const Vector2 PreRaceFlyByState::K_PRERACE_LONG_DISPLAY_RECT = { 638.0f, 349.79999f, 0.0f, 0.0f };
    const Vector2 PreRaceFlyByState::K_PRERACE_TALL_DISPLAY_RECT = { 638.0f, 349.79999f, 0.0f, 0.0f };

    // The two in-class-initialised integral statics still need a definition for the
    // link (both are odr-used: KI_PRERACEMAP_NUMICONS by the icon loops, and
    // KI_MAX_LINES_DESCRIPTION_TEXT bounds maEventDescriptionText).
    const s32 PreRaceFlyByState::KI_MAX_LINES_DESCRIPTION_TEXT;
    const s32 PreRaceFlyByState::KI_PRERACEMAP_NUMICONS;

    // ===============================================================================
    // Bodies
    // ===============================================================================

    // Local alias for the game-mode enum the two IsMap* predicates switch on. TU-local
    // (declared inside namespace BrnGui), so it cannot collide with the file-scope
    // `namespace GSM` aliases the sibling partfiles declare.
    namespace GSM = BrnGameState::GameStateModuleIO;

//
// PreRaceFlyByState::PreRaceFlyByState @0x82514E58
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82514E58.json)
//
// MOVED HERE 2026-08-26 (wave E1) from the retired pre-wave fork
// GameSource/Gui/Flow/HUD/States/BrnPreRaceFlyBy.cpp, which was written against an
// empty-shell copy of this class and memset member spans the shell never declared.
// Rebuilt against the real members named by BrnPreRaceFlyBy.h.
//
// The console ctor is vtable stores + one real member ctor, nothing else:
//     stw r9,  0(r31)        <- the most-derived PreRaceFlyByState vtable (off_82077020)
//     stw r11, 0x38 / 0x160 / 0x288 / 0x3B0 / 0x4D8 / 0x600 / 0x728(r31)
//                            <- TextField's vtable (off_82072F8C) into mEventName, mModeType
//                               and maEventDescriptionText[0..4] (base 0x288, stride 0x128)
//     stw r9,  0x850(r31)    <- IconComponent's vtable (off_82072F90) into mLargeEventIcon
//     stw r8,  0x8E4(r31)    <- AnimationComponent's vtable (off_82072F68) into mStateAnimator
//     stw r7,  0(r10), r10 = r31 + 0x9A0
//                            <- MainMapComponent's vtable (off_82076608) into mMainMapComponent
//     bl  BrnGui::MapManager::MapManager   with r3 = r31 + 0x9A0 + 0x8C  (== +0xA2C)
//                            <- MainMapComponent's own MapManager member (BrnMainMap.h:181,
//                               "X360 comp+0x8C"); on console it is a tail store-and-call
//                               emitted as part of constructing the embedded component.
// Every one of those stores belongs to an embedded sub-object whose OWN ctor lays it down,
// so the modelled effect is exactly "construct the CgsGui::State base + the embedded GUI
// sub-objects", which default member construction reproduces on the host. (Same shape and
// same treatment as ImageGalleryState::ImageGalleryState @0x82500328,
// GameSource/Gui/Flow/Screen/States/BrnImageGallery.cpp:39.)
//
// NOTE, deliberate: the console ctor sets NO member payload -- meCurrentState,
// mfTimeRemaining, mbEndRequestSent, mbDoMapPan, mfIconAnimationStartTime, mpGuiCache,
// mpIconManager, mIconManagerOwnerId, mv2WorldCenterPoint, mbHiddenDueToPause and
// miPreviousIconCount are all left indeterminate here and seeded by OnEnter
// (BrnPreRaceFlyBy_wJ_02.cpp). Adding initialisers would be inventing console behaviour.
    PreRaceFlyByState::PreRaceFlyByState()
        : CgsGui::State()
    {
    }

//
// IsMapApplicableToGameMode @0x824B3120 (DWARF BrnPreRaceFlyBy.h:331)
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824B3120.json)
//
// MOVED HERE 2026-08-26 (wave E1) from the retired HUD fork; body unchanged -- the fork's
// two predicates were already asm-exact, they were only unbuildable in that TU.
//
// The X360 is a jump table biased by -2 (`addi r11, r4, -2` / `cmplwi cr6, r11, 0xE`), so
// modes 0 and 1 fall straight through to the default. Its own comments give the arms:
//   loc_824B3180 `li r3, 0`  -- "jumptable cases 0-2,5,7,13,14" (biased) == modes 2,3,4,7,9,15,16
//   loc_824B3188 `li r3, 1`  -- "default case, cases 3,4,6,8-12" (biased) == every other mode
//
// CONSOLE SEMANTICS, NOT A BUG: E_MODE_STUNT_ATTACK (7) is in the FALSE set, so the stunt-run
// fly-by legitimately shows NO minimap -- titles and description only. OnEnter/OnLeave/Update
// gate the whole MainMapComponent + MapIconManager arm on this predicate, so for a Stunt Run
// the map never loads, never fades in and never pans. Do not "fix" a missing stunt minimap.
    bool PreRaceFlyByState::IsMapApplicableToGameMode(GSM::EGameModeType leGameMode)
    {
        switch (leGameMode)
        {
            case GSM::E_MODE_OFFLINE_SHOWTIME:       // 2
            case GSM::E_MODE_ROAD_RAGE:              // 3
            case GSM::E_MODE_PURSUIT:                // 4
            case GSM::E_MODE_STUNT_ATTACK:           // 7
            case GSM::E_MODE_TRAFFIC_ATTACK:         // 9
            case GSM::E_MODE_ONLINE_FREE_BURN_LOBBY: // 15
            case GSM::E_MODE_ONLINE_SHOWTIME:        // 16
                return false;
            default:
                return true;
        }
    }

//
// IsMapPanApplicableToGameMode @0x824B3190 (DWARF BrnPreRaceFlyBy.h:396)
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824B3190.json)
//
// MOVED HERE 2026-08-26 (wave E1) from the retired HUD fork; body unchanged.
//
// Unbiased jump table (`cmplwi cr6, r4, 8`), so only modes 0..8 index it:
//   loc_824B31D4 `li r3, 1`  -- "jumptable cases 0,5,6,8"     == RACE / BURNING_ROUTE /
//                                                                ELIMINATOR / MARKED_MAN
//   loc_824B31DC `li r3, 0`  -- "default case, cases 1-4,7"   == everything else, and every
//                                                                mode > 8 by the bgt above
//
// These are exactly the point-to-point / route modes: the pan sweeps the map from the start
// to the destination landmark. Stunt attack (7) is FALSE here too -- doubly so, since
// IsMapApplicableToGameMode already suppressed the map for it. mbDoMapPan latches this in
// OnEnter.
    bool PreRaceFlyByState::IsMapPanApplicableToGameMode(GSM::EGameModeType leGameMode)
    {
        switch (leGameMode)
        {
            case GSM::E_MODE_OFFLINE_RACE:   // 0
            case GSM::E_MODE_BURNING_ROUTE:  // 5
            case GSM::E_MODE_ELIMINATOR:     // 6
            case GSM::E_MODE_MARKED_MAN:     // 8
                return true;
            default:
                return false;
        }
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824C6BD0.json, asm arbitrated over Hex-Rays).
//
// Notes taken from the asm rather than the pseudocode:
//  * The X360 builds the record on the stack as { 1, 148, 12 } followed by one byte set
//    to 1, and calls VariableEventQueue<65536,16>::AddEvent(queue, record, 42, 16) --
//    `li r6, 0x10` (record size 16) and `li r5, 0x2A` (channel 42) at 0x824C6BE4/0x824C6BE8.
//    That is exactly the inlined body of StateInterface::OutputInternalState<T>: the three
//    words are GuiEventWrapper<T, 42>'s { payload size, event type, payload offset }
//    header, and the payload is the homed 1-byte BrnGui::GuiEventShowHideHud (id 148).
//    The payload-size word is 1, NOT sizeof(record) - 12 -- the lone payload byte pads the
//    record out to 16, so the subtraction would publish 4.
//  * `lwz r11, 0x1C(r31)` / `addi r3, r11, 0xC` is mpStateInterface's own out-queue at the
//    console offset 12; on the host that is GetOutputEventQueue(), which
//    OutputInternalState reaches for us.
//  * SendStateEvent("BF_PROCEED") is a real `bl` (0x824C6C28), and meCurrentState is set
//    to -1 AFTER it returns (`li r11, -1` / `stw r11, 0x978(r31)`).
//  * No floats, so there is no NaN-polarity decision to make.
//
// 2026-08-03 RECONCILIATION: this body previously stack-built the GuiEventWrapper by hand
// because CgsGuiStateInterface.h had no OutputInternalState. It does now
// (CgsGuiStateInterface.h:195), and its committed comment lists THIS very instantiation --
// "<GuiEventShowHideHud> {1,148,12} + 1 -> 16 @0x82493C98" -- so the named one-line call
// is used and the hand-built record is gone. Same record, byte for byte.
    // @0x824C6BD0 (cpp:722) -- leave the fly-by: put the HUD back up, ask the flow to move
    // on ("BF_PROCEED"), and park the state machine so Update/HandleIncomingEvents stop.
    void PreRaceFlyByState::TriggerExitState()
    {
        GuiEventShowHideHud lShowHud;
        lShowHud.maData[0] = 1;   // show the HUD again
        mpStateInterface->OutputInternalState(lShowHud);

        SendStateEvent("BF_PROCEED");
        meCurrentState = E_PRERACE_INVALID;
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824B4DB0.json, asm arbitrated over Hex-Rays).
//
// Notes taken from the asm rather than the pseudocode:
//  * Every registration goes through sub_824F87C0 == the committed
//    GuiCache::AppendExpectedAptComponent(GuiFlow, const char*) name-taking entry, always
//    with `li r4, 1` == E_GUIFLOW_HUD.
//  * The component arguments are `this + 0x3C`, `+0x164`, `+0x28C`, `+0x854`, `+0x8E8` --
//    each is the member's own offset PLUS 4, i.e. GuiComponent::macName. On the host that
//    is GetName(); the console +4 is deliberately NOT reproduced (32-bit vptr).
//  * THE DESCRIPTION LOOP RUNS 3, NOT 5. `li r30, 3` at 0x824B4E10, walking the array with
//    `addi r29, r29, 0x128`: only maEventDescriptionText[0..2] are registered even though
//    the array holds KI_MAX_LINES_DESCRIPTION_TEXT == 5. 0x128 == 296 is the CONSOLE
//    sizeof(TextField) and is deliberately not reproduced -- the host's is 312 (measured
//    with a compile probe), so the walk is written as ordinary array indexing by name.
//  * The SPrintf capacity is 63 (`li r4, 0x3F`) into a 128-byte stack buffer, and the
//    terminator store `stb r26, var_41(r1)` lands at buffer + 63 -- the belt-and-braces
//    clear of the capacity byte, done AFTER the format call and BEFORE the registration.
//  * Hex-Rays mangles the varargs (it pairs "SatNavIcon" with the loop counter in a
//    __SPAIR64__ and shows nine phantom stack args). The asm is unambiguous:
//    r3 = buffer, r4 = 63, r5 = "%s_%s%d", r6 = "flyByHud_mc", r7 = "SatNavIcon", r8 = i.
//  * No floats, so there is no NaN-polarity decision to make.
    // @0x824B4DB0 (cpp:742) -- tell the cache every apt component this state waits on
    // before it will call the HUD flow layer ready: the title/mode/description text
    // fields, the destination icon, the state animator, the fly-by movie clip itself and
    // its 16 sat-nav icon slots.
    void PreRaceFlyByState::AppendExpectedComponents()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:750 (non-fatal)

        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_HUD, mEventName.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_HUD, mModeType.GetName());

        // Only the first THREE description lines are expected -- see the asm note above.
        for (s32 liLine = 0; liLine < 3; ++liLine)
        {
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_HUD,
                                                   maEventDescriptionText[liLine].GetName());
        }

        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_HUD, mLargeEventIcon.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_HUD, mStateAnimator.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_HUD, KAC_STATE_COMPONENT_NAME);

        for (s32 liIcon = 0; liIcon < KI_PRERACEMAP_NUMICONS; ++liIcon)
        {
            char lacIconComponentName[128];
            CgsCore::SPrintf(lacIconComponentName, 63, "%s_%s%d",
                             KAC_STATE_COMPONENT_NAME, macSatNavIconBaseName, liIcon);
            lacIconComponentName[63] = 0;
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_HUD, lacIconComponentName);
        }
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824C7B70.json, asm arbitrated over Hex-Rays;
// listing at scratchpad/waveJ/asm_updateiconmgr.txt).
//
// Notes taken from the asm rather than the pseudocode:
//  * `li r29, 0` at 0x824C7B90 is hoisted ABOVE the state test, so the same zero feeds
//    both the conditional pokes and the unconditional miNumUsedIcons store. The
//    miNumUsedIcons = 0 + Update() pair at loc_824C7C94 runs on EVERY call once the
//    manager pointer is non-null, including while the state is >= TRANS_OUT.
//  * `cmpwi cr6, r11, 7 / bge` is a SIGNED integer compare on meCurrentState against
//    E_PRERACE_ACTIVE_TRANS_OUT (7) -- no float, no NaN question.
//  * The game mode is read from the cache BEFORE GetTime is called (r3 still holds the
//    cache pointer across `lwzx r30, r3, 0x9E58`), then used to index
//    KAF_ICON_ANIMATION_TIME. The console offsets 0x9E58/0x9E5C/0x9E60 are NOT
//    reproduced -- they are reached through the committed accessors.
//  * NaN POLARITY, the two fsel's at 0x824C7BF0 / 0x824C7BFC. fsel picks frC
//    when frA >= 0 and frB otherwise, and a NaN frA takes the frB arm:
//      - `fneg f12, t; fsel t, f12, 0.0, t`  ==  "t <= 0 -> 0, NaN -> stays NaN".
//        Written `if (t <= 0.0f) t = 0.0f;` -- with NaN the comparison is false and t is
//        left alone, exactly as the hardware does. (`!(t > 0.0f)` would wrongly zero NaN.)
//      - `fsubs f12, 1.0, t; fsel t, f12, t, 1.0`  ==  "t > 1 -> 1, NaN -> 1".
//        Written `if (!(t <= 1.0f)) t = 1.0f;` -- the negated ordered predicate, so NaN
//        takes the clamp arm. (`if (t > 1.0f)` would wrongly leave NaN through.)
//  * `bl HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID` loads r3 (cache),
//    r4 (event id), f1 (the clamped t) and r6 = 0 -- r5 is SKIPPED because the float
//    consumes its GPR slot. So the third C++ parameter is the bool, and it is false.
//  * `cmpw cr6, miPreviousIconCount, liNum / bge` skips only the audio post; the
//    `stw r30, 0x1034(r31)` that updates miPreviousIconCount sits after the branch
//    target and runs unconditionally inside the state < TRANS_OUT arm.
//  * The audio record is built by GuiAudioTriggerEvent::Construct with r4 = 7,
//    r5 = "" (the shared empty-string sentinel unk_820046A7), r6 = "CodeMapScrollEnd",
//    r7 = "" -- i.e. (action, componentName, label, movieName).
    // @0x824C7B70 (cpp:1874) -- drive the shared map icon manager while the fly-by runs:
    // reveal the event's landmark icons over KAF_ICON_ANIMATION_TIME, chirp once each
    // time the revealed count grows, and keep the manager's selection state pinned to
    // this event's junction. The used-icon count is reset and the manager re-run every
    // frame regardless of the animation state.
    void PreRaceFlyByState::UpdateIconManager()
    {
        if (mpIconManager == 0)
        {
            return;
        }

        if (meCurrentState < E_PRERACE_ACTIVE_TRANS_OUT)
        {
            const s32 liGameMode = mpGuiCache->GetGameMode();

            // clamp01 of the elapsed fraction of the icon reveal. See the fsel/NaN note.
            f32 lfAnimationT = (mpGuiCache->GetTime() - mfIconAnimationStartTime)
                             / KAF_ICON_ANIMATION_TIME[liGameMode];
            if (lfAnimationT <= 0.0f)
            {
                lfAnimationT = 0.0f;
            }
            if (!(lfAnimationT <= 1.0f))
            {
                lfAnimationT = 1.0f;
            }

            const s32 liNumActiveIcons =
                mpGuiCache->HACK_FindABetterPlaceForMe_SetActiveLandmarksByEventID(
                    mpGuiCache->GetEventID(), lfAnimationT, false);

            if (miPreviousIconCount < liNumActiveIcons)
            {
                GuiAudioTriggerEvent lScrollEnd;
                lScrollEnd.Construct(7, "", "CodeMapScrollEnd", "");
                mpStateInterface->OutputGuiEvent(lScrollEnd);
            }
            miPreviousIconCount = liNumActiveIcons;

            // Direct member writes: the X360 emits raw stwx/stbx here, there are no
            // accessors on MapIconManager for these. Friendship, not invented setters.
            mpIconManager->meIconFilterMode       = MapIconManager::E_ICONFILTER_ALL;
            mpIconManager->mbIsDisplayingEventInfo = false;
            mpIconManager->miSelectedCheckpoint    = 0;
            mpIconManager->muSelectedJunctionID    = mpGuiCache->GetJunctionID();
        }

        mpIconManager->miNumUsedIcons = 0;
        mpIconManager->Update();
    }
}

// ============================================================================
// FOLDED FROM BrnPreRaceFlyBy_wJ_02.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// wave-J partfile 02 -- BrnGui::PreRaceFlyByState::OnEnter @0x824C6498 (cpp:187)
//                       BrnGui::PreRaceFlyByState::OnLeave @0x824C68F0 (cpp:565)
//
// Both bodies come from the raw X360 asm (OnEnter: the disassembly at the bottom of
// scratchpad/waveJ/prfb_xrefs.txt; OnLeave: scratchpad/waveJ/asm_onleave.txt), arbitrated
// over Hex-Rays throughout.
//
// The two records this partfile puts on the wire, and how each is published:
//   * id 532, 16 bytes on channel 40 (OnEnter). A BAKED-HEADER post: the file-local type
//     below derives CgsGui::GuiEvent<532> and goes straight onto the out-queue, because
//     the committed OutputGuiEvent<T> direct-passes the event and prepends nothing (see
//     the FLAG in CgsGuiStateInterface.h).
//   * id 213, 24 bytes on channels 41 and 42 (OnLeave). A WRAPPED post: the payload type
//     BrnGui::GuiEventShowHideSatNav is deliberately the RAW 12-byte payload with no
//     GuiEvent<213> base, so it must travel through StateInterface::OutputViewState /
//     OutputInternalState, which build the GuiEventWrapper<T,41|42> header the console
//     stack-builds here (BrnGuiDemangledEventTypes.h:566 names this very call site).
//
// 2026-08-03 RECONCILIATION: this partfile was parked while GuiCache's
// UnloadResource/GetMapIconManager/RefreshMapState/SetPreRaceFlyByActive,
// MapIconManager's SetIconsVisible/SetOwnerParameters/ReleaseResources + flag members,
// GuiEventDrawEventIcons::EIconDisplayType and GuiEventShowHideSatNav's real shape were
// all missing. Every one of them has since landed, so both bodies are here. The OnLeave
// sat-nav post was rebuilt onto OutputViewState/OutputInternalState in the same pass --
// posting the now-raw 12-byte struct directly would have shipped a headerless record.
//
// The class header GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.h needs no change:
// every static table, every embedded component and IsMapApplicableToGameMode compile
// from it as committed.
// ===================================================================================


namespace BrnGui
{
    namespace GSM = BrnGameState::GameStateModuleIO;

    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) ----------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // 0x28

        // The log-category bit the debug print is gated on (`clrldi r11,r11,63`).
        const u64 KX_MESSAGE_FILTER_BIT = 1;

        // id 532 -- the "pre-race fly-by entered" GUI-out notification. X360 OnEnter builds
        // { 1, 532, 12 } at 0x824C64E4..0x824C64F0 and posts a 16-byte record on channel 40
        // (`li r6,0x10` / `li r5,0x28`). Per CgsGuiEvent.h the first header word is the
        // PAYLOAD size, so the payload is ONE byte at +12 -- the emitter simply never
        // writes it, and a 1-byte payload at +12 pads the record out to the 16 AddEvent is
        // given. The house shape for a 1-byte payload is BrnGuiDemangledEventTypes.h's
        // GuiEventShowHideBoostBar (id 214 size 1). Exactly the record BrnPausedHudState
        // posts for the same id (BrnPausedHudState.cpp:24). FLAG: consumer-derived type
        // name -- no DWARF row survives for the X360 id-532 event, and the payload byte's
        // meaning is not recovered (nothing in the image ever stores it).
        struct GuiEventPreRaceFlyByEnter : public CgsGui::GuiEvent<532>
        {
            u8 mu8Payload;   // +0x0C -- the 1-byte payload the size word names; UNWRITTEN by the emitter

            GuiEventPreRaceFlyByEnter()
                : CgsGui::GuiEvent<532>(
                      static_cast<u32>(sizeof(mu8Payload)),                            // X360 word0 == 1
                      static_cast<u32>(offsetof(GuiEventPreRaceFlyByEnter, mu8Payload))) // X360 word2 == 12
                , mu8Payload(0)
            {
            }
        };
        // Host layout pin: CgsGui::GuiEvent<N> is the three header words over an empty
        // CgsModule::Event base, so the record is 16 bytes on the host exactly as on the
        // console -- the AddEvent size argument below is a host sizeof, never a baked 16.
        static_assert(sizeof(GuiEventPreRaceFlyByEnter) == 16,
                      "host record matches the X360 16-byte id-532 post");
    }

    // ================================================================================
    //  OnEnter  @ 0x824C6498  (cpp:187)
    //
    //  Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
    //  (disassembly in scratchpad/waveJ/prfb_xrefs.txt; asm arbitrated over Hex-Rays).
    //
    //  Bring the fly-by up: observe the flow's event set, announce the entry, cache the
    //  GuiCache, construct every embedded component against the "flyByHud_mc" apt clip,
    //  reset the state's own scalars, and -- for the modes that get a map -- take
    //  ownership of the shared MapIconManager for the pre-race route display.
    //
    //  Notes taken from the asm rather than the pseudocode:
    //   * The description-field loop really runs all FIVE fields: r30 walks
    //     off_82F26BEC and stops at &qword_82F26C00, i.e. 20 bytes / 4 == 5 iterations
    //     (0x824C65F0/0x824C65F8). It is AppendExpectedComponents that only registers 3.
    //   * mEventName / mModeType / the description fields / mStateAnimator go through the
    //     component vtable slot 0 (`lwz r11,0(rN)` + `bctrl`) == the virtual
    //     GuiComponent::Construct; mLargeEventIcon is a DIRECT call to
    //     BrnGui::IconComponent::Construct (0x824C6618) because that overload takes the
    //     extra state-identifier table argument (here null).
    //   * The four `stb 0` at mainmap+0x678..0x67B (0x824C66AC..0x824C66BC) are the
    //     committed MainMapComponent::SetStickMapToScreenEdges(false x4) inlined -- the
    //     setter is called by name, the console offsets are recorded only.
    //   * SetOwnerParameters takes NINE parameters, two of them on the stack, which
    //     Hex-Rays dropped: the outgoing parameter save area starts at sp+0x14 with
    //     8-byte slots, so sp+0x54 (`li r6,5` stored at 0x824C689C) is parameter 8 and
    //     sp+0x5C (`stw r26` == "flyByHud_mc" at 0x824C6894) is parameter 9. r6 is then
    //     reloaded with 16 for parameter 3. Its RETURN VALUE replaces mIconManagerOwnerId
    //     (`stw r3, 0x990(r31)` at 0x824C68BC).
    //   * The icon-display-type argument is 5 == E_ICON_DISPLAY_TYPE_COUNT, i.e. the
    //     one-past-the-end sentinel, not a real display set. Recorded as measured.
    //   * The debug print is the inlined StrStreamBase::operator<<(s32) (the "%d"/"0x%X"
    //     AppendFormat split at 0x824C6824/0x824C684C is that operator's print-mode
    //     branch); on the host it is the committed operator<< chain.
    //   * No float COMPARE anywhere in this body -- f31 only ever carries 0.0f -- so there
    //     is no NaN-polarity decision to make.
    //   * Every console byte offset quoted below is a 32-bit-ABI reference only; all
    //     member access is by name so the host's own layout applies.
    // ================================================================================
    void PreRaceFlyByState::OnEnter()
    {
        // X360 +0x1034. -1 means "no icon count seen yet", so the first UpdateIconManager
        // pass always counts as an increase and fires the map-scroll-end sound.
        miPreviousIconCount = -1;

        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // Announce the fly-by entry on the GUI-out channel (X360 record size 16).
        {
            GuiEventPreRaceFlyByEnter lEnter;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEnter),
                KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lEnter)));
        }

        // Both asserts are the committed accessor path's own (non-fatal on the X360, so the
        // fetch below runs regardless -- exactly as the console does).
        CGS_ASSERT(mpStateInterface->GetAccessPointers() != 0,
                   "mpAccessPointers != NULL");  // CgsGuiStateInterface.h:344
        CgsGui::GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
        CGS_ASSERT(lpAccessPointers->GetGuiCache() != 0, "mpGuiCache");  // CgsGuiShared.h:201

        mpGuiCache     = lpAccessPointers->GetGuiCache();
        meCurrentState = E_PRERACE_UNLOADED;

        // ---- the components, all parented on the fly-by clip ---------------------------
        mEventName.Construct(KAC_EVENT_NAME_TEXTFIELD_NAME, mpStateInterface,
                             KAC_STATE_COMPONENT_NAME);
        mModeType.Construct(KAC_MODE_TYPE_TEXTFIELD_NAME, mpStateInterface,
                            KAC_STATE_COMPONENT_NAME);

        for (s32 liLine = 0; liLine < KI_MAX_LINES_DESCRIPTION_TEXT; ++liLine)
        {
            maEventDescriptionText[liLine].Construct(KAAC_EVENT_DESC_TEXTFIELD_NAMES[liLine],
                                                     mpStateInterface,
                                                     KAC_STATE_COMPONENT_NAME);
        }

        // No state-identifier table and no parent clip: the destination icon is addressed
        // by name and driven with the string-keyed SetState.
        mLargeEventIcon.Construct(KAC_LARGE_EVENT_ICON_NAME, mpStateInterface, 0, 0);
        mStateAnimator.Construct(KAC_STATE_ANIMATOR_NAME, mpStateInterface, 0);

        // ---- the state's own scalars ----------------------------------------------------
        mbEndRequestSent = false;   // X360 +0x980 (stb)
        mbDoMapPan       = false;   // X360 +0x981 (stb)
        mfTimeRemaining  = 0.0f;    // X360 +0x97C

        MainMapComponent::MainMapParameterBundle lMapParameters;
        lMapParameters.mv4ViewRect    = KV4_VIEW_RECT;
        lMapParameters.mv4PaddingRect = KV4_PADDING_RECT;
        lMapParameters.meMapType      = GuiEventRenderMainMap::E_MAPTYPE_PRERACE;  // X360 li 1
        mMainMapComponent.Construct(mpStateInterface, &lMapParameters);
        mMainMapComponent.Prepare();

        mpIconManager = 0;

        // The pre-race map is free to sit wherever the world centre puts it.
        mMainMapComponent.SetStickMapToScreenEdges(false, false, false, false);

        mfIconAnimationStartTime = 0.0f;
        mv2WorldCenterPoint.SetZero();   // X360 `stvx` of a zeroed vector at +0x1020

        // Stores in the console's order: the type word first, then the id (the id 0 means
        // "no large event icon resolved yet"; SetEventIconResource fills it in). X360 `li r10,4`
        // == E_GUI_RESOURCETYPE_APT.
        mLargeIconResource.meType = CgsGui::E_GUI_RESOURCETYPE_APT;
        mLargeIconResource.muId   = 0;

        mbHiddenDueToPause  = false;
        mIconManagerOwnerId = MapIconManager::E_PRERACE_FLYBY_MAP;

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:271 (non-fatal)

        // X360 `stbx r24, mpGuiCache, 0xA015` -- the cache-side "a fly-by is running" latch.
        mpGuiCache->SetPreRaceFlyByActive(true);

        // The 15-case jumptable at 0x824C6744 IS IsMapApplicableToGameMode inlined (false for
        // modes 2,3,4,7,9,15,16); the method is the named face of it.
        if (IsMapApplicableToGameMode(static_cast<GSM::EGameModeType>(mpGuiCache->GetGameMode())))
        {
            mpIconManager = mpGuiCache->GetMapIconManager();
            CGS_ASSERT(mpIconManager != 0, "mpIconManager");   // cpp:277 (non-fatal)

            mpIconManager->SetIconsVisible(true);

            if ((CgsDev::Message::gxMessageFilterFlags & KX_MESSAGE_FILTER_BIT) != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "MAPICONMANAGER: PreRaceFlyBy is calling SetOwnerParameters with OwnerID "
                    << static_cast<s32>(mIconManagerOwnerId)
                    << ".\n";
            }

            // The manager hands back the owner id it actually granted, which is what the
            // release path later quotes -- hence the self-assignment through the call.
            mIconManagerOwnerId = mpIconManager->SetOwnerParameters(
                mpStateInterface,
                macSatNavIconBaseName,          // "SatNavIcon"
                KI_PRERACEMAP_NUMICONS,         // 16
                mIconManagerOwnerId,
                false,                          // lbUseRoadSigns
                false,                          // lbShowingDriveThrus
                false,                          // lbAllowDriveThruSelection
                GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT,   // X360 stack arg 8 == 5
                KAC_STATE_COMPONENT_NAME);      // X360 stack arg 9 == "flyByHud_mc"

            // Friend pokes: the X360 writes these three straight through the manager
            // (stb +0xAA1C, stw +0xAA04, stb +0xAA20) -- there is no setter for them.
            mpIconManager->mbRotateSatNav        = false;
            mpIconManager->meIconSizeMode        = MapIconManager::E_ICONSIZE_LARGE;
            mpIconManager->mbShowingPreRaceRoute = true;
        }
    }

    // ================================================================================
    //  OnLeave  @ 0x824C68F0  (cpp:565)
    //
    //  Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
    //  (disassembly in scratchpad/waveJ/asm_onleave.txt; asm arbitrated over Hex-Rays).
    //
    //  Tear the fly-by down: fade the sat-nav out on both the view and internal channels,
    //  drop the apt clips, stop observing, release the screens the mode pulled in, and
    //  hand the icon manager back.
    //
    //  Notes taken from the asm rather than the pseudocode:
    //   * The two sat-nav posts are the SAME record sent twice, on channel 41 then 42
    //     (0x824C6924 `li r5,0x29` / 0x824C695C `li r5,0x2A`), size 24: header
    //     { 12, 213, 12 } + { map type 0, 0.0f, a byte-zero word }. Hex-Rays's `v14`
    //     shuffle is just the compiler reusing the stack slots.
    //   * The two 20-byte channel-41 posts of { 8, 18, 12 } + { "", 2 } / { "", 1 } are
    //     CgsGui::StateInterface::PlayAptMovie inlined -- VERIFIED against the committed
    //     body (CgsGuiStateInterface.cpp:46), which posts exactly that record on channel
    //     41 with sizeof == 20. Called by name here. The X360's `unk_820046A7` is the
    //     shared empty-string sentinel, i.e. "".
    //   * `cmpwi cr6, r11, 0xA` + `bge` at 0x824C6B44 is a SIGNED compare and the ONLY
    //     guard on the per-gamemode screen index: the online modes (10..16) skip the
    //     unload, but E_MODE_NONE (-1) does NOT -- see the note at that call.
    //   * meCurrentState = -1 is stored between ClearExpectedAptComponentList and the
    //     icon-manager release (0x824C6B78); that order is kept.
    //   * The game mode is read from the cache twice (0x824C6A6C for the map predicate,
    //     0x824C6B40 for the screen index) -- both are the same accessor.
    //   * No float COMPARE in this body (the only float is the 0.0f fade), so there is no
    //     NaN-polarity decision to make.
    //   * Every console byte offset quoted is a 32-bit-ABI reference only; all member
    //     access is by name.
    // ================================================================================
    void PreRaceFlyByState::OnLeave()
    {
        // Fade the main sat-nav map out immediately (fade time 0.0f, show false). The same
        // record goes to the view layer (channel 41) and to the internal listeners (42);
        // both go through the StateInterface templates, which build the
        // GuiEventWrapper<T,channel> header the console stack-builds here. Posting the
        // struct straight onto the out-queue would ship a headerless 12-byte record --
        // GuiEventShowHideSatNav is deliberately the RAW 12-byte payload with no
        // CgsGui::GuiEvent<213> base (BrnGuiDemangledEventTypes.h:566).
        {
            GuiEventShowHideSatNav lHideSatNav;
            lHideSatNav.Construct(GuiEventShowHideSatNav::E_MAPTYPE_MAIN, false, 0.0f);

            mpStateInterface->OutputViewState(lHideSatNav);
            mpStateInterface->OutputInternalState(lHideSatNav);
        }

        // Level 2 with an empty name: stop whatever the fly-by had playing on that layer.
        mpStateInterface->PlayAptMovie("", 2);

        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // Streamed diagnostic (X360 builds it into CgsDev::Assert::gpcMessageBuffer through a
        // StrStream; the house idiom folds that to a local buffer). Non-fatal -- everything
        // below dereferences mpGuiCache anyway, exactly as the console does.
        if (mpGuiCache == 0)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Gui Cache should be setup in the OnEnter";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                lacMessage,
                "..\\..\\..\\GameSource\\Gui/Flow/PreEvent/States/BrnPreRaceFlyBy.cpp",
                600);
            CgsDev::Assert::EndAssert();
        }

        // Same inlined predicate as OnEnter (the 15-case jumptable at 0x824C6A90).
        if (IsMapApplicableToGameMode(static_cast<GSM::EGameModeType>(mpGuiCache->GetGameMode())))
        {
            // Put the shared map back to whatever the rest of the GUI expects.
            mpGuiCache->RefreshMapState();
            mpStateInterface->PlayAptMovie("", 1);
        }

        // The large event icon is only loaded once SetEventIconResource has resolved one.
        if (mLargeIconResource.muId != 0)
        {
            mpGuiCache->UnloadResource(mLargeIconResource);
        }

        // MEASURED CONSOLE QUIRK, reproduced as-is: the guard is one-sided. The X360 emits
        // `cmpwi cr6, r11, 0xA` + `bge` (0x824C6B44) and nothing else, so an E_MODE_NONE (-1)
        // mode passes the test and indexes one tuple BEFORE maPerGamemodeScreens. Kept faithful
        // rather than silently hardened -- flag for the verify round if the host cares.
        const s32 liGameMode = mpGuiCache->GetGameMode();
        if (liGameMode < GSM::E_MODE_OFFLINE_COUNT)   // X360 signed `cmpwi 0xA` + `bge`
        {
            mpGuiCache->UnloadResource(maPerGamemodeScreens[liGameMode]);
        }

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_HUD);

        meCurrentState = E_PRERACE_INVALID;

        if (mpIconManager != 0)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & KX_MESSAGE_FILTER_BIT) != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "MAPICONMANAGER: PreRaceFlyBy is calling ReleaseResources.\n";
            }

            mpIconManager->ReleaseResources(mpStateInterface, mIconManagerOwnerId);
        }

        // X360 `stbx r29, mpGuiCache, 0xA015` -- clear the latch OnEnter set.
        mpGuiCache->SetPreRaceFlyByActive(false);
    }
}

// ============================================================================
// FOLDED FROM BrnPreRaceFlyBy_wJ_03.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::PreRaceFlyByState -- wave-J partfile 03: the event / flow trio.
//   b5-decomp/src/GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy_wJ_03.cpp
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (the raw `assembly` array
// arbitrated over Hex-Rays throughout -- the pseudocode of two members of this group is
// varargs-mangled):
//   SetupComponents      @0x824D6228  (cpp:638,  assert cpp:689)
//   HandleIncomingEvents @0x824D6410  (cpp:795,  asserts cpp:819/927/984)
//   HandleAptEvents      @0x824C6C48  (cpp:996,  asserts cpp:1001 / cpp:1088)
// All three bodies are HERE. (Two of them were parked while the shared headers were thin;
// GuiCache::GetEventID, StateInterface::OutputViewState/OutputInternalState and
// GuiEventShowHideSatNav's real fields have all landed since, so nothing is parked now.)
//
// The other functions of the class live in sibling wave-J partfiles; the owning header is
// BrnPreRaceFlyBy.h. Every class static named here (KAC_STATE_COMPONENT_NAME) is DECLARED
// there and DEFINED in partfile 01 -- the definitions are deliberately not repeated (one
// definition per program).
//
// CONSOLE-LITERAL NOTE: no X360 member displacement is reproduced as a number anywhere in
// this file -- 0x978 (meCurrentState), 0x981 (mbDoMapPan) and 0x8E4 (mStateAnimator) are
// reached by member name, so the host's own LLP64 layout applies. They appear in comments
// only. There are no float comparisons in these bodies, so there is no NaN-polarity
// decision to make.
// ===================================================================================


namespace BrnGui
{
    // House file-local alias (precedent: the sibling PreEvent / HUD state TUs).
    namespace GSM = BrnGameState::GameStateModuleIO;

    namespace
    {
        // The state's inbound GUI queue. CgsGui::State only holds an INCOMPLETE
        // `InputBuffer::GuiEventQueue*`, so the concrete queue type has to be named here to
        // drain it; <18432,16> is the committed GUI queue shape (CgsGuiModule.h:44,
        // CgsGuiModuleIO.h:91) and this is the house idiom -- identical typedef and cast in
        // BrnBootAttract.cpp:15/:50.
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- the observed event ids (PreRaceFlyByState::maiEventToObserve @0x82065CAC
        //      == {6, 21, 64, 159, 160, 162, 164, 213}; the array is declared in the
        //      header and defined in partfile 01) --------------------------------------
        // ATTESTED names -- each id is owned by a type already homed in the tree:
        const s32 KI_EVENT_CONTROLLER_INPUT_PRESSED = 6;    // CgsGui::GuiEventControllerInputPressed
        const s32 KI_EVENT_APT_TRIGGER              = 21;   // CgsGui::GuiEventAptTrigger (payload == GuiEventAptTriggerPayload)
        const s32 KI_EVENT_GUI_CACHE                = 64;   // the GuiCache refresh event (BrnCarSelectMain_wG_03.cpp)
        const s32 KI_EVENT_PRERACE_MESSAGES         = 159;  // BrnGui::GuiEventPreRaceMessages
        const s32 KI_EVENT_PRERACE_TRIGGER          = 160;  // BrnGui::GuiEventPreraceTrigger
        const s32 KI_EVENT_SHOW_HIDE_SAT_NAV        = 213;  // BrnGui::GuiEventShowHideSatNav
        // FLAG: role-derived names. Ids 162 and 164 have no homed payload type (164 is
        // emitted as a bare CgsGui::GuiEvent<164> -- CgsGuiModule_AddGuiEvent_Inst.cpp
        // @0x823D2B68 -- and 162 has no instantiation at all). These names record only
        // what THIS state does with each id; they are not attested outside this switch.
        const s32 KI_EVENT_FLYBY_ABORT              = 162;  // exits immediately
        const s32 KI_EVENT_FLYBY_END                = 164;  // drives the graceful trans-out arm
    }

    // -------------------------------------------------------------------------------
    // HandleIncomingEvents  @0x824D6410   (cpp:795)
    // Drain the state's inbound GUI queue once per Update, dispatch the eight observed
    // ids, forward every event to the embedded map component, then clear the queue.
    // -------------------------------------------------------------------------------
    void PreRaceFlyByState::HandleIncomingEvents()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        for (s32 liEventType = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventType = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            // A handler that ran a state event has already left the flow; stop dispatching
            // and let the Clear below drop whatever is still queued.
            if (meCurrentState == E_PRERACE_INVALID)
                break;

            switch (liEventType)
            {
                case KI_EVENT_CONTROLLER_INPUT_PRESSED:
                case KI_EVENT_GUI_CACHE:
                case KI_EVENT_SHOW_HIDE_SAT_NAV:
                    // Observed only so the map component below sees them.
                    break;

                case KI_EVENT_APT_TRIGGER:
                    HandleAptEvents(reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent));
                    break;

                case KI_EVENT_PRERACE_MESSAGES:
                {
                    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:819

                    // The online modes have no fly-by: any pre-race message ends it.
                    const s32 liGameMode = mpGuiCache->GetGameMode();
                    if (liGameMode >= GSM::E_MODE_ONLINE_MODE_START
                        && (liGameMode <= GSM::E_MODE_ONLINE_ROAD_RAGE
                            || liGameMode == GSM::E_MODE_ONLINE_BURNING_HOME_RUN))
                    {
                        TriggerExitState();
                    }
                    break;
                }

                case KI_EVENT_PRERACE_TRIGGER:
                    // HandlePreRaceTriggerEvent (DWARF cpp:984) is INLINED here: the X360
                    // emits only its null-payload assert at this arm and then falls to the
                    // shared RecvEvent tail, so the assert is the whole surviving effect.
                    CGS_ASSERT(lpEvent != 0, "lpPreRaceTrigger");   // cpp:984
                    break;

                case KI_EVENT_FLYBY_ABORT:
                    TriggerExitState();
                    break;

                case KI_EVENT_FLYBY_END:
                    if (meCurrentState < E_PRERACE_ACTIVE_EVENT_TITLES || mbHiddenDueToPause)
                    {
                        // Nothing is on screen yet (or the pause menu hid it), so there is
                        // no transition to play -- leave immediately.
                        TriggerExitState();
                        meCurrentState = E_PRERACE_ACTIVE_DONE;
                    }
                    else
                    {
                        meCurrentState = E_PRERACE_ACTIVE_TRANS_OUT;
                        mLargeEventIcon.SetState("transOut");
                        mStateAnimator.AddOutputAptViewState(
                            "apt_Transition",
                            mbDoMapPan ? "transoutMap" : "transout",
                            false);

                        // Fade the sat-nav out over half a second, on both the view and
                        // the internal-state channels (the X360 posts the same record to
                        // 41 and then 42).
                        GuiEventShowHideSatNav lSatNavEvent;
                        lSatNavEvent.Construct(GuiEventShowHideSatNav::E_MAPTYPE_MAIN,
                                               /*lbShow*/ false, /*lfFadeTime*/ 0.5f);
                        mpStateInterface->OutputViewState<GuiEventShowHideSatNav>(lSatNavEvent);
                        mpStateInterface->OutputInternalState<GuiEventShowHideSatNav>(lSatNavEvent);
                    }
                    break;

                default:
                    // cpp:927 -- the message names Update because this drain is inlined
                    // into it on the console. Non-fatal.
                    CGS_ASSERT(false, "Unexpected event in PreRaceFlyByState::Update");
                    break;
            }

            mMainMapComponent.RecvEvent(lpEvent, liEventType);
        }

        lpInQueue->Clear();
    }

    // -------------------------------------------------------------------------------
    // HandleAptEvents  @0x824C6C48   (cpp:996)
    // The apt view's "transition complete" callback for this screen's root clip: step the
    // fly-by presentation on to whatever the state's just-finished transition leads to.
    //
    // Notes taken from the asm rather than the pseudocode:
    //  * The null-payload assert is NON-fatal (BeginAssert / FireAssert / EndAssert with
    //    no early-out, 0x824C6C74..0x824C6CDC) -- the very next instruction dereferences
    //    the payload regardless. Reproduced as written.
    //  * The do-while at 0x824C6CF8..0x824C6D18 is an INLINED strcmp with the literal as
    //    the LEFT operand (`subf r8, r8(name), r9(literal)`); written back as a strcmp
    //    call in the same operand order.
    //  * The state switch is `meCurrentState - 2` over 6 table slots (0x824C6D28
    //    `addi r11, r11, -2` / `cmplwi cr6, r11, 5`), so only states 2..7 reach an arm and
    //    everything else -- including E_PRERACE_INVALID -- falls into the assert. Slots 0
    //    (MAP_ICON_DELAY) and 4 (MEDALS) point at the function's own epilogue: real no-ops.
    //  * Every transition arm writes meCurrentState BEFORE calling AddOutputAptViewState
    //    (the `stw` precedes the `bl` in all four), and the TRANS_OUT arm calls
    //    TriggerExitState FIRST, then overwrites the E_PRERACE_INVALID that call leaves
    //    behind with E_PRERACE_ACTIVE_DONE (0x824C6E1C `bl` then 0x824C6E24 `stw r11,
    //    0x978`). Both orderings are preserved.
    //  * AddOutputAptViewState is the CgsGui::GuiComponent base method invoked on
    //    mStateAnimator (X360 `addi r3, r30, 0x8E4`); the `li r6, 0` is lbImmediate.
    // -------------------------------------------------------------------------------
    void PreRaceFlyByState::HandleAptEvents(const CgsGui::GuiEventAptTriggerPayload* lpTrigger)
    {
        // cpp:1001 -- the X360 streams this through a CgsDev::StrStream; per project policy
        // that is lowered to the static text. Non-fatal: the body reads it either way.
        CGS_ASSERT(lpTrigger != 0, "Invalid event passed to PreRaceFlyByState::HandleAptEvents");

        if (lpTrigger->meEventType != CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE)
            return;

        if (strcmp(KAC_STATE_COMPONENT_NAME, lpTrigger->mpacComponentName) != 0)
            return;

        switch (meCurrentState)
        {
            case E_PRERACE_ACTIVE_MAP_ICON_DELAY:
                break;

            case E_PRERACE_ACTIVE_EVENT_TITLES:
                // The title bars have finished coming in: pan the map in when this event
                // has one, otherwise go straight to the medals.
                if (mbDoMapPan)
                {
                    meCurrentState = E_PRERACE_ACTIVE_MAP_INTRO;
                    mStateAnimator.AddOutputAptViewState("apt_Transition", "mapIn", false);
                }
                else
                {
                    meCurrentState = E_PRERACE_ACTIVE_MEDALS;
                    mStateAnimator.AddOutputAptViewState("apt_Transition", "medalsInNoMap", false);
                }
                break;

            case E_PRERACE_ACTIVE_MAP_INTRO:
                meCurrentState = E_PRERACE_ACTIVE_SHOW_MAP;
                mStateAnimator.AddOutputAptViewState("apt_Transition", "showMap", false);
                break;

            case E_PRERACE_ACTIVE_SHOW_MAP:
                meCurrentState = E_PRERACE_ACTIVE_MEDALS;
                mStateAnimator.AddOutputAptViewState("apt_Transition", "medalsInMap", false);
                break;

            case E_PRERACE_ACTIVE_MEDALS:
                break;

            case E_PRERACE_ACTIVE_TRANS_OUT:
                TriggerExitState();
                meCurrentState = E_PRERACE_ACTIVE_DONE;
                break;

            default:
                // cpp:1088 -- streamed as "Not expecting to receive a trans complete from
                // the screen when we are in state " << meCurrentState << "\n"; lowered to
                // the static text per project policy, the streamed value being the switch
                // scrutinee itself.
                CGS_ASSERT(false,
                           "Not expecting to receive a trans complete from the screen when we are in state \n");
                break;
        }
    }

    namespace
    {
        // The pre-race mode-name string-id table @0x82F27840 (image read), indexed by
        // GSM::EGameModeType 0..9.
        //
        // DWARF-attested global, NOT this TU's data: dwarfdump GameSource/Gui/BrnGuiShared.cpp:34
        // declares `extern const char *[10] KAPC_GAMEMODE_STRINGIDS;` attributed to
        // BrnGuiShared.cpp:154, and the five consecutive tables at 0x82F277E0..0x82F27868
        // reproduce that file's declaration order exactly (POSITION x8, POSITION_LOWERCASE x8,
        // DIRECTION x8 @0x82F27820, GAMEMODE x10 @0x82F27840, GAMEMODE_PLURAL x10). It carries
        // the DWARF name here but stays file-local because its home does not exist yet.
        // DELETE-WHEN GameSource/Gui/BrnGuiShared.cpp lands (declare it in BrnGuiShared.h and
        // index the shared one).
        const char* const KAPC_GAMEMODE_STRINGIDS[GSM::E_MODE_OFFLINE_COUNT] =
        {
            "GAMEMODE_RACE",          // 0  E_MODE_OFFLINE_RACE
            "GAMEMODE_FACEOFF",       // 1  E_MODE_FACE_OFF
            "GAMEMODE_CRASH",         // 2  E_MODE_OFFLINE_SHOWTIME
            "GAMEMODE_ROADRAGE",      // 3  E_MODE_ROAD_RAGE
            "GAMEMODE_PURSUIT",       // 4  E_MODE_PURSUIT
            "GAMEMODE_BURNINGROUTE",  // 5  E_MODE_BURNING_ROUTE
            "GAMEMODE_ELIMINATOR",    // 6  E_MODE_ELIMINATOR
            "GAMEMODE_STUNTATTACK",   // 7  E_MODE_STUNT_ATTACK
            "GAMEMODE_SURVIVAL",      // 8  E_MODE_MARKED_MAN
            "GAMEMODE_TRAFFICATTACK", // 9  E_MODE_TRAFFIC_ATTACK
        };
    }

    // -------------------------------------------------------------------------------
    // SetupComponents  @0x824D6228   (cpp:638)
    // Once every apt component of the fly-by screen has initialised: fill the per-mode
    // description text, start the large event icon's transition-in, and label the screen
    // with the event's name and its mode name.
    // -------------------------------------------------------------------------------
    void PreRaceFlyByState::SetupComponents()
    {
        // GuiCache far word +0x9E58 == GetGameMode(); reached by accessor, not by offset.
        s32 liGameMode = mpGuiCache->GetGameMode();

        switch (liGameMode)
        {
            case GSM::E_MODE_OFFLINE_RACE:
                SetRaceDescription();
                break;

            case GSM::E_MODE_OFFLINE_SHOWTIME:
                // Crash has no pre-race description text: the arm branches straight to the
                // shared tail at 0x824D6368.
                break;

            case GSM::E_MODE_ROAD_RAGE:
                SetRoadRageDescription();
                break;

            case GSM::E_MODE_BURNING_ROUTE:
                SetBurningRouteDescription();
                break;

            case GSM::E_MODE_STUNT_ATTACK:
                SetFreestyleDescription();
                break;

            case GSM::E_MODE_MARKED_MAN:
                SetMarkedManDescription();
                break;

            default:
                // cpp:689 -- streamed as "Unknown game mode (" << mode << ")\n"; lowered
                // to the static text per project policy. Non-fatal, and the console then
                // executes `mr r29, r26` with r26 == 0 (0x824D6364), i.e. it CLAMPS the
                // mode to E_MODE_OFFLINE_RACE for the rest of the body. The switch bound
                // is the UNSIGNED `cmplwi cr6, r29, 8` / `bgt` at 0x824D624C, and the
                // jumptable sends cases 1, 4 and 6 here as well.
                CGS_ASSERT(false, "Unknown game mode ()\n");
                liGameMode = GSM::E_MODE_OFFLINE_RACE;
                break;
        }

        mLargeEventIcon.SetState("transIn");

        // The localisation id the screen's title field shows, and the format it resolves
        // under. 31 chars + the manual terminator == the console's 32-byte stack buffer.
        char lacEventTextId[32];
        CgsLanguage::LanguageManager::ParameterFormatType leEventTextFormat;

        if (liGameMode == GSM::E_MODE_PURSUIT)
        {
            // Pursuit labels the screen with the hunted car's name rather than an event
            // id -- but this arm is UNREACHABLE on this build, and deliberately kept
            // because the binary has it: mode 4 takes the switch's default arm above,
            // which clamps the mode to 0 before this `cmpwi cr6, r29, 4` at 0x824D6378
            // ever runs.
            char lacPursuitCarId[16];
            CgsIDConvertToString(mpGuiCache->GetPursuitCarID(), lacPursuitCarId);
            CgsCore::SPrintf(lacEventTextId, 31, "CAR_CAPS_%s", lacPursuitCarId);
            leEventTextFormat = CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP;
        }
        else
        {
            // GuiCache far word +0x9E5C == GetEventID() (the declaration this body waits on).
            CgsCore::SPrintf(lacEventTextId, 31, "EV_%06u", mpGuiCache->GetEventID());
            leEventTextFormat = CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP_TOUPPER;
        }
        lacEventTextId[31] = 0;

        mEventName.SetLocalisedText(lacEventTextId, leEventTextFormat);
        mModeType.SetLocalisedText(KAPC_GAMEMODE_STRINGIDS[liGameMode],
                                   CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
    }
}

// ============================================================================
// FOLDED FROM BrnPreRaceFlyBy_wJ_04.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
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

    // =================================================================================
    // [DIAG] [FLAG PC bring-up] NOT IN THE X360 BINARY -- the `[flyby]` state-edge rung.
    // Same logger, same BRN_PROP_DIAG env gate and the same first-N latch as the
    // `[evt-flow]` ladder in GameBridgeGameStateToX_EventFlowGuiEvents.cpp.
    //
    // WHY IT IS HERE: the fly-by has no other observable. Every transition past
    // E_PRERACE_ACTIVE_EVENT_TITLES is driven by an apt TRANSITION_COMPLETE trigger the
    // movie's own ActionScript posts (CgsAptCommunicator::sMethod_SendAptEvent -> event 21),
    // and the exit is driven by GUI event 164 -- so "the overlay never goes away" has three
    // different possible stopping points (no 164, no trans-out transition-complete, or a
    // state that never left EVENT_TITLES) and NOTHING in the log distinguishes them.
    // Placed AFTER HandleIncomingEvents so an event-driven transition is reported on the
    // frame it happens, not the next one. Edge-filtered, so a stalled state prints once.
    // DELETE-WHEN the fly-by is confirmed to reach E_PRERACE_INVALID through
    // TriggerExitState in a boot-drive run.
    // =================================================================================
    {
        static const bool sbDiag           = ( getenv( "BRN_PROP_DIAG" ) != 0 );
        static s32        siFlyByDiagLeft  = 32;
        static s32        siLastLoggedState = -2;   // not any EPreRaceState value

        if ( sbDiag && siFlyByDiagLeft > 0 &&
             static_cast<s32>(meCurrentState) != siLastLoggedState &&
             CgsDev::Log::gpDebugPrint != 0 )
        {
            --siFlyByDiagLeft;
            siLastLoggedState = static_cast<s32>(meCurrentState);
            *CgsDev::Log::gpDebugPrint
                << "[flyby] meCurrentState -> " << static_cast<s32>(meCurrentState)
                << " (-1=INVALID/exited 0=UNLOADED 1=LOADING 2=ICONDELAY 3=TITLES 4=MAPINTRO"
                   " 5=SHOWMAP 6=MEDALS 7=TRANSOUT 8=DONE) timeRemaining " << mfTimeRemaining
                << " endRequestSent " << (mbEndRequestSent ? 1 : 0) << "\n";
        }
    }
}

}

// ============================================================================
// FOLDED FROM BrnPreRaceFlyBy_wJ_05.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::PreRaceFlyByState -- wave-J partfile 05: the two VMX workers.
//   FindEventDirection  @0x824B4EC8  (DWARF BrnPreRaceFlyBy.cpp:1596)
//   CalculateZoomFactor @0x824BE8F0  (DWARF BrnPreRaceFlyBy.cpp:1763)
//
// Both bodies were reconstructed from the raw X360 assembly
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824B4EC8.json and /0x824BE8F0.json, field
// `assembly`), arbitrated over the Hex-Rays pseudocode -- which for these two is mostly
// unreadable `__asm` soup and, for FindEventDirection, is actively WRONG (it renders the
// degree conversion as the constant 57.29578 instead of angle * 57.29578).
//
// (CalculateZoomFactor was parked while GuiCache::GetEventID / GetLandmarkInfoFromID and
// RaceEventData's checkpoint accessors were missing; all four have landed since, and the
// necessary-and-sufficient set was proven with a shadow-include compile probe --
// scratchpad/waveJ/probe_g05/, built with the gate's own flags: clean.)
//
// ALL VMX CONSTANTS BELOW WERE DUMPED FROM THE IMAGE with headless IDA
// (scratchpad/waveJ/dump_g05_consts.py -> scratchpad/waveJ/g05_consts.txt), not inferred:
//   flt_82067490 = 0.41421398520469666  (tan 22.5 degrees -- the half-sector offset)
//   flt_820037C8 = -1.0     flt_82001CC0 = 0.0     flt_82004928 = 360.0
//   flt_820652A8 = 57.295780181884766   (radians -> degrees)
//   flt_8206748C = 0.02222222276031971  (1 / 45)
//   unk_820652B0 = {6.2831854820251465, ...}  (2*pi; the lvlx + vspltw takes lane 0)
//   unk_82181510 = {0.0, 1.0, 0.0, 0.0}       (the +Y axis, the cross-product sign probe)
// ===================================================================================


namespace GSM = BrnGameState::GameStateModuleIO;

namespace BrnGui
{

namespace
{
    // The reference direction FindEventDirection measures every event bearing against.
    // The X360 stack-builds it lane by lane from three rodata literals
    // (flt_82067490 / flt_82001CC0 / flt_820037C8) -- there is no rodata vector and no
    // class static for it, so it is reproduced here as a file-local constant.
    // 0.41421399 == tan(22.5 degrees): rotating the -Z map "north" by half a 45-degree
    // sector is what makes the later floor(degrees / 45) land each bearing in the middle
    // of its compass sector instead of on the boundary.
    // FLAG: the CONSTANT NAME is ours (consumer-derived); the three VALUES are image reads.
    const f32 KF_SECTOR_REFERENCE_X = 0.41421399f;
    const f32 KF_SECTOR_REFERENCE_Y = 0.0f;
    const f32 KF_SECTOR_REFERENCE_Z = -1.0f;

    // rodata scalars the bearing math loads (all image reads; names ours).
    const f32 KF_TWO_PI              = 6.2831853f;   // unk_820652B0 lane 0
    const f32 KF_RADIANS_TO_DEGREES  = 57.29578f;    // flt_820652A8
    const f32 KF_DEGREES_PER_TURN    = 360.0f;       // flt_82004928
    const f32 KF_ONE_OVER_SECTOR_DEG = 0.022222223f; // flt_8206748C == 1/45

    // flt_82F27384 -- the 16:9 base aspect the shared map-zoom solver is called
    // against. IMAGE READ, not inferred: headless IDA dumped it as 1.7777777910232544
    // (scratchpad/waveJ/dump_g05_consts.py -> scratchpad/waveJ/g05_consts.txt).
    const f32 KF_MAP_BASE_ASPECT_RATIO = 1.7777778f;

    // ---- the three 3-lane vector ops the X360 emits as VMX128 ----
    //
    // The console does all of this in registers: vmsum3fp128 for the dot products,
    // vrsqrtefp + one Newton-Raphson step (vmulfp/vnmsubfp/vmaddfp) for the reciprocal
    // square root, and a vpermwi128 0x63 (the yzx lane rotate) pair for the cross
    // product. None of that has a portable PC equivalent and the refinement step only
    // exists to recover precision the estimate instruction throws away, so -- exactly as
    // rw/math/vpu/types.h and BrnMapUtils.h already state for this codebase -- these are
    // reconstructed at the SEMANTIC level with scalar math, not transliterated.
    // (Reconstruction-local helpers: the X360 has no out-of-line calls for them.)

    f32 Dot3(const Vector3& lv3A, const Vector3& lv3B)
    {
        return (lv3A.x * lv3B.x) + (lv3A.y * lv3B.y) + (lv3A.z * lv3B.z);
    }

    Vector3 Cross3(const Vector3& lv3A, const Vector3& lv3B)
    {
        const Vector3 lv3Result = { (lv3A.y * lv3B.z) - (lv3A.z * lv3B.y),
                                    (lv3A.z * lv3B.x) - (lv3A.x * lv3B.z),
                                    (lv3A.x * lv3B.y) - (lv3A.y * lv3B.x),
                                    0.0f };
        return lv3Result;
    }

    Vector3 Normalise3(const Vector3& lv3In)
    {
        const f32 lfInverseLength = 1.0f / std::sqrt(Dot3(lv3In, lv3In));
        const Vector3 lv3Result = { lv3In.x * lfInverseLength,
                                    lv3In.y * lfInverseLength,
                                    lv3In.z * lfInverseLength,
                                    0.0f };
        return lv3Result;
    }
}

// -------------------------------------------------------------------------------------
// FindEventDirection @ 0x824B4EC8 (DWARF cpp:1596) -- which of the eight compass sectors
// the event's destination landmark lies in, as seen from the current world camera.
//
// Notes taken from the asm rather than the pseudocode:
//  * The two asserts are cpp:1601 (`li r5, 0x641`) and cpp:1604 (`li r5, 0x644`); the
//    second fires when the game mode is none of {0, 5, 8} -- the three the assert string
//    names (OFFLINE_RACE / BURNING_ROUTE / MARKED_MAN).
//  * `lvx128 v127, r4, 0x4AE0` is the committed GuiCache::GetWorldCameraPosition() far
//    member. The console offset 0x4AE0 is NOT reproduced -- the accessor is called by
//    name so the host's own layout applies.
//  * `lhz r4, var_A0(r1)` after the sret call reads the FIRST halfword of the returned
//    BrnGameState::LandmarkIndex (big-endian, hence Hex-Rays' `HIWORD`); on the host the
//    value is simply passed by value into GetLandmarkInfoFromIndex.
//  * Operation ORDER is the asm's: normalise both -> dot -> clamp -> acos -> cross-product
//    sign probe -> radians-to-degrees -> wrap -> floor. (The pseudocode's `v20 = 57.29578`
//    is a Hex-Rays artefact of the splat-and-store round trip; the asm multiplies the
//    ANGLE by 57.29578 at 0x824B50A8 and reloads lane 0 at 0x824B50B4.)
//  * CLAMP POLARITY: `vmaxfp v0, v0, -1` then `vminfp v1, v0, 1` are select-style ops
//    (max(a,b) == a > b ? a : b), so a NaN dot falls through to the OTHER operand. The
//    ternaries below reproduce that exactly; `if (x < -1.0f) x = -1.0f;` would NOT.
//  * NaN POLARITY on the two wrap loops: the console guards them with `bge` (0x824B50C0)
//    and `ble` (0x824B50D4), both of which are TAKEN when unordered, i.e. a NaN skips
//    both loops. `while (deg < 0)` and `while (deg > 360)` are the matching ordered
//    predicates -- both false for NaN -- so the plain C++ shape is already correct here.
//  * The sign probe is `vcmpgtfp. v0, {0,0,0,0}, crossY`, an ORDERED greater-than: NaN
//    leaves the angle alone. `crossY < 0.0f` matches.
    ECompassPoints PreRaceFlyByState::FindEventDirection()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1601

        CGS_ASSERT((mpGuiCache->GetGameMode() == GSM::E_MODE_OFFLINE_RACE)
                       || (mpGuiCache->GetGameMode() == GSM::E_MODE_BURNING_ROUTE)
                       || (mpGuiCache->GetGameMode() == GSM::E_MODE_MARKED_MAN),
                   "(mpGuiCache->GetGameMode() == GsmIO::E_MODE_OFFLINE_RACE) || "
                   "(mpGuiCache->GetGameMode() == GsmIO::E_MODE_BURNING_ROUTE) || "
                   "(mpGuiCache->GetGameMode() == GsmIO::E_MODE_MARKED_MAN)");   // cpp:1604

        const Vector4& lv4CameraPosition = mpGuiCache->GetWorldCameraPosition();

        const Vector3 lv3Reference = { KF_SECTOR_REFERENCE_X,
                                       KF_SECTOR_REFERENCE_Y,
                                       KF_SECTOR_REFERENCE_Z,
                                       0.0f };

        GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
        mpGuiCache->GetLandmarkInfoFromIndex(mpGuiCache->GetEventDestinationLandmarkIndex(),
                                             &lLandmarkInfo);

        // The icon record's leading 16-byte lane is the landmark's world position -- the
        // `lvx128 v13, r0, <info>` at 0x824B4FC0.
        const Vector4& lv4LandmarkPosition = lLandmarkInfo.GetPositionLane();
        const Vector3 lv3ToLandmark = { lv4LandmarkPosition.x - lv4CameraPosition.x,
                                        lv4LandmarkPosition.y - lv4CameraPosition.y,
                                        lv4LandmarkPosition.z - lv4CameraPosition.z,
                                        0.0f };

        const Vector3 lv3ReferenceDirection = Normalise3(lv3Reference);
        const Vector3 lv3LandmarkDirection  = Normalise3(lv3ToLandmark);

        f32 lfCosAngle = Dot3(lv3ReferenceDirection, lv3LandmarkDirection);
        lfCosAngle = (lfCosAngle > -1.0f) ? lfCosAngle : -1.0f;   // vmaxfp
        lfCosAngle = (lfCosAngle <  1.0f) ? lfCosAngle :  1.0f;   // vminfp

        f32 lfAngle = XboxMath::XMVectorACos(lfCosAngle);         // bl XMVectorACos @0x824B5024 (FX-GATE)

        // acos only ever returns [0, pi], so the half-turn the bearing actually lies in is
        // recovered from the sign of the cross product's Y component -- the console dots
        // the cross with unk_82181510 == the +Y axis and tests it against zero.
        const Vector3 lv3Cross = Cross3(lv3ReferenceDirection, lv3LandmarkDirection);
        if (lv3Cross.y < 0.0f)
            lfAngle = KF_TWO_PI - lfAngle;

        f32 lfDegrees = lfAngle * KF_RADIANS_TO_DEGREES;
        while (lfDegrees < 0.0f)
            lfDegrees += KF_DEGREES_PER_TURN;
        while (lfDegrees > KF_DEGREES_PER_TURN)
            lfDegrees -= KF_DEGREES_PER_TURN;

        // 360 degrees / 8 compass points == one 45-degree sector per point.
        const s32 liEventDirection =
            static_cast<s32>(std::floor(lfDegrees * KF_ONE_OVER_SECTOR_DEG));

        // cpp:1648 -- `cmpwi cr6, r31, 8` / `blt`. The 8 is ECompassPoints'
        // E_COMPASS_POINTS_COUNT, which BrnGuiShared.h now defines, so the enumerator is
        // named rather than the measured literal spelled out.
        CGS_ASSERT(liEventDirection < E_COMPASS_POINTS_COUNT,
                   "E_COMPASS_POINTS_COUNT > leEventDirection");

        return static_cast<ECompassPoints>(liEventDirection);
    }

// -------------------------------------------------------------------------------------
// CalculateZoomFactor @ 0x824BE8F0 (DWARF cpp:1763) -- fit the whole event (its start
// position plus every checkpoint) into the pre-race map view: accumulate the {x, z}
// bounding rectangle of those world positions, centre the map on it, and hand the
// rectangle to the shared map-zoom solver.
//
// Notes taken from the asm rather than the pseudocode (the pseudocode is `__asm` soup and
// drops all four arguments of the tail call):
//  * `bl sub_824F8AF0` @0x824BE914 is the committed GuiCache::GetProfileEventDisplayInfo
//    (r3 = the cache, r4 = the event id) -- see BrnGuiCache.h.
//  * The "mpWorldDataController" assert at 0x824BE92C carries the file string
//    GameSource/Gui/BrnGuiCache... and line 0x914 == 2324, i.e. it belongs to
//    GuiCache::GetWorldDataController(), which the X360 INLINES here (two direct
//    `lwz r11, 0x4064(r30)` loads). It is NOT reproduced at this call site: calling the
//    committed accessor by name brings its own assert with it.
//  * The two asserts this function does own are cpp:1781 (`li r5, 0x6F5`, "lpEventStart")
//    and cpp:1782 (`li r5, 0x6F6`, "lpRaceEventData"). Note the ORDER: the console fetches
//    BOTH records first and only then null-checks them, so a null display record does not
//    short-circuit the event-info lookup.
//  * The checkpoint bounds assert inside the loop is BrnRaceEventData.h:953 (`li r5,
//    0x3B9`) -- RaceEventData::GetCheckpointData's own assert, inlined by the console
//    alongside the raw `*(base + i * 0x28)` load. De-inlined here to the named accessor,
//    which carries that assert.
//  * CONSOLE OFFSETS NOT REPRODUCED: +0x9E5C (event id), +0x4064 (world-data controller),
//    +0x18/+0x1C (checkpoint array/count), the 0x28 checkpoint stride, and the
//    `stvx128 v0, r26, 0xFF0` centre store (0x9A0 mMainMapComponent + 0x650) are all
//    reached by name on the host, so the host's own layout applies.
//  * The `vperm` with mask unk_82CDA450 = {00 01 02 03 | 18 19 1A 1B | 00 01 02 03 |
//    00 01 02 03} (image read) takes a world position to the 2D map plane as
//    {x, z, x, x}; only lanes 0 and 1 are ever read back, so the reconstruction keeps a
//    plain 2-lane Vector2 {x = world X, y = world Z}.
//  * SELECT POLARITY: `vminfp v1, v12, v0` / `vmaxfp v2, v11, v0` are select-style ops
//    (min(a,b) == a < b ? a : b) with the RUNNING bound as operand a, so a NaN world
//    coordinate replaces the running bound rather than being rejected. The ternaries
//    below reproduce that exactly; std::min/std::max would not.
//  * `vcmpgtfp. v0, height, width` @0x824BEAD0 is an ORDERED greater-than, and the branch
//    at 0x824BEB0C takes the LONG rect when the bit is clear -- so `height > width`
//    (false for NaN -> LONG) is the matching C++ predicate.
//  * The `vrefp` + two Newton-Raphson steps on the splatted 2.0 (flt_82065670, image read)
//    is just a reciprocal: the centre is (min + max) * 0.5. The refinement is precision
//    recovery for the estimate instruction and is deliberately NOT transliterated (same
//    policy as rw/math/vpu/types.h and BrnMapUtils.h).
//  * The two display rects hold IDENTICAL values ({638.0f, 349.79999f}, recovered from
//    their runtime initialiser stubs), so the branch is value-neutral; it is kept because
//    the binary has it. Which of the two addresses is LONG and which is TALL is an
//    INFERENCE from initialiser order == declaration order (see BrnPreRaceFlyBy.h) --
//    0x82FB4AA0 is taken on the taller-than-wide arm, which is why it is read as TALL.
// -------------------------------------------------------------------------------------
    f32 PreRaceFlyByState::CalculateZoomFactor()
    {
        const u32 luEventId = mpGuiCache->GetEventID();

        const SatNavEventDisplayInfo* const lpEventStart =
            mpGuiCache->GetProfileEventDisplayInfo(luEventId);

        const BrnProgression::RaceEventData* const lpRaceEventData =
            mpGuiCache->GetWorldDataController()->GetEventInfoFromEventId(luEventId);

        CGS_ASSERT(lpEventStart != 0, "lpEventStart");           // cpp:1781
        CGS_ASSERT(lpRaceEventData != 0, "lpRaceEventData");     // cpp:1782

        // Seed the bounding rectangle with the event's start position, flattened onto the
        // map plane ({x, z}).
        Vector2 lv2Min = { lpEventStart->mv3Position.x, lpEventStart->mv3Position.z, 0.0f, 0.0f };
        Vector2 lv2Max = lv2Min;

        for (s32 liCheckpointIndex = 0;
             liCheckpointIndex < lpRaceEventData->GetCheckpointCount();
             ++liCheckpointIndex)
        {
            GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
            mpGuiCache->GetLandmarkInfoFromID(
                lpRaceEventData->GetCheckpointData(liCheckpointIndex)->GetLandmarkId(),
                &lLandmarkInfo);

            const Vector4& lv4Position = lLandmarkInfo.GetPositionLane();
            const Vector2 lv2Point = { lv4Position.x, lv4Position.z, 0.0f, 0.0f };

            lv2Min.x = (lv2Min.x < lv2Point.x) ? lv2Min.x : lv2Point.x;   // vminfp
            lv2Min.y = (lv2Min.y < lv2Point.y) ? lv2Min.y : lv2Point.y;
            lv2Max.x = (lv2Max.x > lv2Point.x) ? lv2Max.x : lv2Point.x;   // vmaxfp
            lv2Max.y = (lv2Max.y > lv2Point.y) ? lv2Max.y : lv2Point.y;
        }

        const f32 lfWidth  = lv2Max.x - lv2Min.x;
        const f32 lfHeight = lv2Max.y - lv2Min.y;

        const Vector2 lv2Centre = { (lv2Max.x + lv2Min.x) * 0.5f,
                                    (lv2Max.y + lv2Min.y) * 0.5f,
                                    0.0f, 0.0f };
        mMainMapComponent.SetDesiredWorldCentre(lv2Centre);

        const Vector2& lv2DisplayRect = (lfHeight > lfWidth) ? K_PRERACE_TALL_DISPLAY_RECT
                                                             : K_PRERACE_LONG_DISPLAY_RECT;

        return MapTransform::CalculateZoomFactor(lv2Min, lv2Max, lv2DisplayRect,
                                                 KF_MAP_BASE_ASPECT_RATIO);
    }
}

// ============================================================================
// FOLDED FROM BrnPreRaceFlyBy_wJ_06.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::PreRaceFlyByState -- wave-J partfile 06: the compass description family.
//   SetRaceDescription         @0x824C6E90  (DWARF BrnPreRaceFlyBy.cpp:1128)
//   SetMarkedManDescription    @0x824C7470  (DWARF BrnPreRaceFlyBy.cpp:1346)
//   SetBurningRouteDescription @0x824C76D8  (DWARF BrnPreRaceFlyBy.cpp:1451)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the raw `assembly` listing of each address
// arbitrates over the Hex-Rays pseudocode throughout (the three listings are in
// scratchpad/waveJ/asm_setrace.txt, asm_g06_0x824C7470.txt and asm_g06_0x824C76D8.txt).
//
// 2026-08-03 RECONCILIATION: all three bodies were parked behind six missing names --
// SatNavIconInfo::GetCgsId(), Profile::{GetNumWinsForGameMode, GetCurrentProgressionRank,
// GetEventCount}, GuiCache::GetEventID() and the whole
// GameSource/GameState/Progression/BrnDerivedCars.h header. Every one has landed, so all
// three bodies are here.
//
// SIGNED-COMPARE NOTE (this is the one place the landed accessor set changes the code):
// Profile::GetEventCount() is `u32` (DWARF BrnProfile.h:1009; the committed header at
// BrnProfile.h:356 matches), but the X360's profile-event walk compares SIGNED --
// `cmpwi`/`ble` on the pre-guard and `cmpw`/`blt` on the bound. A raw
// `s32 i < lpProfile->GetEventCount()` would promote i to unsigned and invert that guard,
// so each walk hoists the count into an s32 first.
//
// FILE-SCOPE DATA THE GATE ALREADY VERIFIED: the `sizeof(GuiEventPreRaceDescription) == 40`
// static_assert passes, i.e. the host record really is byte-for-byte the 40-byte record
// the X360 posts.
//
// CORRECTIONS TO THE WAVE-J SPEC, measured from the asm (spec section 3 flagged both as
// "READ ASM"):
//   * The id-464 payload slot order is the SAME in all three workers: +8 is the compass
//     direction FindEventDirection returned, +12 is the game-mode ordinal (0 race, 8
//     marked man, 5 burning route). The spec's guesses "markedman {8, dir}" and
//     "burning {5, car-word}" are both wrong -- there is no car word in the payload.
//   * The per-mode profile tallies confirm those ordinals independently: the workers read
//     maiWinsPerOfflineGameMode/maiLossesPerOfflineGameMode at +468/+548 biased by
//     4*mode (0x1D4/0x224 for mode 0, 0x1F4/0x244 for mode 8, 0x1E8/0x238 for mode 5).
//   * SetMarkedManDescription's PART3 really is SnPrintf("%f") of the target time -- a
//     raw float print, not a clock format -- and its parameter format word is
//     E_FORMAT_INTEGER (11). SetBurningRouteDescription's PART3 uses the language
//     manager's clock formatter and passes E_FORMAT_TEXT (0).
// ===================================================================================


namespace GSM = BrnGameState::GameStateModuleIO;

namespace BrnGui
{

namespace
{
    typedef CgsLanguage::LanguageManager LM;

    // The DIRECTION_* localisation-id table @0x82F27820 (image read; dump at
    // scratchpad/waveJ/probe_PreRaceFlyBy_6/verify2.txt), indexed by ECompassPoints.
    //
    // DWARF-attested global, NOT this TU's data: dwarfdump GameSource/Gui/BrnGuiShared.cpp:31
    // declares `extern const char *[8] KAPC_COMPASS_POINT_STRINGIDS;` attributed to
    // BrnGuiShared.cpp:140 -- external linkage, owned by BrnGuiShared.cpp. The five
    // consecutive tables at 0x82F277E0..0x82F27868 reproduce that file's declaration order
    // exactly (POSITION x8, POSITION_LOWERCASE x8, DIRECTION x8 @0x82F27820, GAMEMODE x10,
    // GAMEMODE_PLURAL x10), which is how the attribution was confirmed from the image.
    // It carries the DWARF name here but stays file-local because its home does not exist
    // yet. DELETE-WHEN GameSource/Gui/BrnGuiShared.cpp lands (declare it in BrnGuiShared.h
    // and index the shared one).
    const char* const KAPC_COMPASS_POINT_STRINGIDS[E_COMPASS_POINTS_COUNT] =
    {
        "DIRECTION_N",  "DIRECTION_NW", "DIRECTION_W",  "DIRECTION_SW",
        "DIRECTION_S",  "DIRECTION_SE", "DIRECTION_E",  "DIRECTION_NE",
    };

// (fold: an identical definition of KI_GUI_OUT_EVENT_CHANNEL was dropped here -- this TU defines it once, above)
    const u32 KU_DESCRIPTION_BUFFER_LEN = 128;

    struct GuiEventPreRaceDescription : public CgsGui::GuiEvent<464>
    {
        alignas(8) CgsID mLandmarkId;   // +16
        s32 miDirection;                // +24
        s32 miGameMode;                 // +28
        s32 miTimesPlayed;              // +32
        u8  mbEventFlag;                // +36
        u8  mu8ProgressionRank;         // +37

        // Header words are DERIVED from the host record, not the console immediates:
        // word0 == the payload byte count (X360 0x18 == 24 == 40 - 16) and word2 == the
        // payload offset (X360 0x10 == 16). The sizeof pin below is what makes the two
        // agree with the console record.
        GuiEventPreRaceDescription()
            : CgsGui::GuiEvent<464>(
                  static_cast<u32>(sizeof(GuiEventPreRaceDescription)
                                   - offsetof(GuiEventPreRaceDescription, mLandmarkId)),
                  static_cast<u32>(offsetof(GuiEventPreRaceDescription, mLandmarkId)))
            , mLandmarkId(0)
            , miDirection(0)
            , miGameMode(0)
            , miTimesPlayed(0)
            , mbEventFlag(0)
            , mu8ProgressionRank(0)
        {
        }
    };
    static_assert(sizeof(GuiEventPreRaceDescription) == 40,
                  "the host record matches the X360 40-byte id-464 post");
}

// -------------------------------------------------------------------------------------
// SetBurningRouteDescription @ 0x824C76D8 (DWARF cpp:1451) -- the burning-route flavour:
// the compass line, the destination landmark, the target time as a clock, and the name of
// the alternate-livery car the route pays out. See the banner above for the asm notes.
// -------------------------------------------------------------------------------------
void PreRaceFlyByState::SetBurningRouteDescription()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1456

    const ECompassPoints leEventDirection = FindEventDirection();
    maEventDescriptionText[0].SetLocalisedText("PRE_BURNINGROUTE_PART1", LM::E_FORMAT_ID_LOOKUP, 1,
                                               KAPC_COMPASS_POINT_STRINGIDS[leEventDirection],
                                               LM::E_FORMAT_ID_LOOKUP);

    GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
    mpGuiCache->GetLandmarkInfoFromIndex(mpGuiCache->GetEventDestinationLandmarkIndex(),
                                         &lLandmarkInfo);

    char lacBuffer[KU_DESCRIPTION_BUFFER_LEN];
    CgsCore::SnPrintf(lacBuffer, KU_DESCRIPTION_BUFFER_LEN, "LM_%llu", lLandmarkInfo.GetCgsId());
    lacBuffer[KU_DESCRIPTION_BUFFER_LEN - 1] = 0;
    maEventDescriptionText[1].SetLocalisedText("PRE_BURNINGROUTE_PART2", LM::E_FORMAT_ID_LOOKUP, 1,
                                               lacBuffer, LM::E_FORMAT_ID_LOOKUP);

    // The route's target time is rendered as a clock by the language manager. PPC float
    // ABI: the time travels in f1 and SKIPS its GPR slot, so the console's r6 (128) is the
    // buffer size, not a fourth argument -- matching the committed
    // LanguageManager::FormatMinutesSecondsStringMediumText(char*, f32, s32) const.
    mpStateInterface->GetLanguageManager()->FormatMinutesSecondsStringMediumText(
        lacBuffer, mpGuiCache->GetTargetTimeInEvent(),
        static_cast<s32>(KU_DESCRIPTION_BUFFER_LEN));
    maEventDescriptionText[2].SetLocalisedText("PRE_BURNINGROUTE_PART3", LM::E_FORMAT_ID_LOOKUP, 1,
                                               lacBuffer, LM::E_FORMAT_TEXT);

    maEventDescriptionText[3].SetLocalisedText("PRE_BURNINGROUTE_PART4", LM::E_FORMAT_ID_LOOKUP);

    // ---- the alternate-livery car this route pays out ----
    CGS_ASSERT(mpGuiCache->GetWorldDataController() != 0, "lpWorldDataController");   // cpp:1521
    const WorldDataController* lpWorldDataController = mpGuiCache->GetWorldDataController();

    const BrnProgression::RaceEventData* lpEventData =
        lpWorldDataController->GetEventInfoFromEventId(mpGuiCache->GetEventID());
    CGS_ASSERT(lpEventData != 0, "lpEventData");                                     // cpp:1523

    // cpp:1524 -- the X360 inlines IsSpecialEvent() to "the record's +0x10 doubleword is
    // not null". It then loads that SAME doubleword a second time as the players-car id
    // and asserts it again at cpp:1526, so both asserts are kept, in source order.
    // (No kCGSID_NULL constant is committed anywhere in the tree yet; the literal 0 the
    // X360 compares against stands in for it.)
    CGS_ASSERT(lpEventData->GetEventInstanceId() != 0, "lpEventData->IsSpecialEvent()");
    const CgsID lPlayersCarId = lpEventData->GetEventInstanceId();
    CGS_ASSERT(lPlayersCarId != 0, "kCGSID_NULL != lPlayersCarId");                  // cpp:1526
    CGS_ASSERT(lpWorldDataController->GetVehicleList() != 0,
               "lpWorldDataController->GetVehicleList()");                           // cpp:1529

    BrnProgression::DerivedCarArray lCarVariants;
    lCarVariants.ConstructPatternLiveryList(lpWorldDataController->GetVehicleList(), lPlayersCarId);

    // Walk past the player's own car to the first pattern-livery variant. The X360
    // compares the index against the live count SIGNED (`cmpw`), hence the cast; and it
    // adds no end-of-array guard -- if every entry matches, the indexed accessor's own
    // bounds assert is what fires.
    s32 liVariant = 0;
    while (liVariant < static_cast<s32>(lCarVariants.GetLength())
           && lCarVariants.GetItem(liVariant) == lPlayersCarId)
    {
        ++liVariant;
    }
    CGS_ASSERT(lPlayersCarId != lCarVariants.GetItem(liVariant),
               "lPlayersCarId != lCarVariants.GetItem(liVariant)");                  // cpp:1538

    char lacCarId[KI_CGSID_STRING_LEN];
    CgsIDConvertToString(lCarVariants.GetItem(liVariant), lacCarId);
    lacCarId[KI_CGSID_STRING_LEN - 1] = 0;
    CgsCore::SnPrintf(lacBuffer, KU_DESCRIPTION_BUFFER_LEN, "CAR_CAPS_%s", lacCarId);
    lacBuffer[KU_DESCRIPTION_BUFFER_LEN - 1] = 0;
    maEventDescriptionText[4].SetLocalisedText("PRE_BURNINGROUTE_PART5", LM::E_FORMAT_ID_LOOKUP, 1,
                                               lacBuffer, LM::E_FORMAT_ID_LOOKUP);

    // ---- the id-464 medals-panel record ----
    const BrnProgression::Profile* lpProfile = mpGuiCache->GetProfile();

    GuiEventPreRaceDescription lDescription;
    lDescription.mLandmarkId        = lLandmarkInfo.GetCgsId();
    lDescription.miDirection        = leEventDirection;
    lDescription.miGameMode         = GSM::E_MODE_BURNING_ROUTE;
    lDescription.miTimesPlayed      = lpProfile->GetNumWinsForGameMode(GSM::E_MODE_BURNING_ROUTE)
                                    + lpProfile->GetNumLossesForGameMode(GSM::E_MODE_BURNING_ROUTE);
    lDescription.mu8ProgressionRank = static_cast<u8>(lpProfile->GetCurrentProgressionRank());

    const u32 luEventId = mpGuiCache->GetEventID();
    const BrnProgression::ProfileEvent* lpProfileEvent = 0;
    // Profile::GetEventCount() is u32; hoist it into an s32 so the walk keeps the
    // console's SIGNED cmpwi/ble pre-guard and cmpw/blt bound (see the banner).
    const s32 liEventCount = static_cast<s32>(lpProfile->GetEventCount());
    for (s32 liEvent = 0; liEvent < liEventCount; ++liEvent)
    {
        if (lpProfile->GetEvent(liEvent)->GetID() == luEventId)
        {
            lpProfileEvent = lpProfile->GetEvent(liEvent);
            break;
        }
    }
    // [PC GUARD RETIRED 2026-08-27, D1 profile-event-list wave] -- its own DELETE-WHEN ("the
    // profile event list is populated on this build") came due. The guard existed because
    // Profile::AddEvent @0x82359EB8 had no caller on this build, so the profile's event table was
    // empty for the whole run; ProgressionManager::UnlockToProgressionRank @0x8239DDE8 -- the
    // single xref to AddEvent in the whole XEX -- now runs at boot and gives the profile one
    // ProfileEvent per authored event junction. The key matches by construction: GuiCache's
    // muEventID IS the event-junction id (GuiCache::RecEvent case 93, `muEventID =
    // lpPrepare->muEventJunctionID`), and the junction id is exactly what the producer stores
    // in each record. Console read (unconditional, no null test) restored.
    // This worker takes the SPECIAL-EVENT bit, not the rank-win bit.
    lDescription.mbEventFlag =
        (lpProfileEvent->GetFlags()
         & BrnProgression::ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE) != 0;

    mpStateInterface->GetOutputEventQueue()->AddEvent(
        &lDescription, KI_GUI_OUT_EVENT_CHANNEL, static_cast<s32>(sizeof(lDescription)));
}

// -------------------------------------------------------------------------------------
// SetMarkedManDescription @ 0x824C7470 (DWARF cpp:1346) -- the marked-man (survival)
// flavour: the compass line, the destination landmark, the survival timer and its caption.
// See the banner above for the per-instruction asm notes.
// -------------------------------------------------------------------------------------
void PreRaceFlyByState::SetMarkedManDescription()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1351

    const ECompassPoints leEventDirection = FindEventDirection();
    maEventDescriptionText[0].SetLocalisedText("PRE_SURVIVAL_PART1", LM::E_FORMAT_ID_LOOKUP, 1,
                                               KAPC_COMPASS_POINT_STRINGIDS[leEventDirection],
                                               LM::E_FORMAT_ID_LOOKUP);

    GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
    mpGuiCache->GetLandmarkInfoFromIndex(mpGuiCache->GetEventDestinationLandmarkIndex(),
                                         &lLandmarkInfo);

    char lacBuffer[KU_DESCRIPTION_BUFFER_LEN];
    CgsCore::SnPrintf(lacBuffer, KU_DESCRIPTION_BUFFER_LEN, "LM_%llu", lLandmarkInfo.GetCgsId());
    lacBuffer[KU_DESCRIPTION_BUFFER_LEN - 1] = 0;
    maEventDescriptionText[1].SetLocalisedText("PRE_SURVIVAL_PART2", LM::E_FORMAT_ID_LOOKUP, 1,
                                               lacBuffer, LM::E_FORMAT_ID_LOOKUP);

    // The survival target time goes out as a raw "%f" -- the X360 really does print the
    // float, then hand the resulting text to the field with an INTEGER parameter format.
    CgsCore::SnPrintf(lacBuffer, KU_DESCRIPTION_BUFFER_LEN, "%f",
                      mpGuiCache->GetTargetTimeInEvent());
    lacBuffer[KU_DESCRIPTION_BUFFER_LEN - 1] = 0;
    maEventDescriptionText[2].SetLocalisedText("PRE_SURVIVAL_PART3", LM::E_FORMAT_ID_LOOKUP, 1,
                                               lacBuffer, LM::E_FORMAT_INTEGER);

    maEventDescriptionText[3].SetLocalisedText("PRE_SURVIVAL_PART4", LM::E_FORMAT_ID_LOOKUP);
    maEventDescriptionText[4].SetText("");

    // ---- the id-464 medals-panel record ----
    const BrnProgression::Profile* lpProfile = mpGuiCache->GetProfile();

    GuiEventPreRaceDescription lDescription;
    lDescription.mLandmarkId        = lLandmarkInfo.GetCgsId();
    lDescription.miDirection        = leEventDirection;
    lDescription.miGameMode         = GSM::E_MODE_MARKED_MAN;
    lDescription.miTimesPlayed      = lpProfile->GetNumWinsForGameMode(GSM::E_MODE_MARKED_MAN)
                                    + lpProfile->GetNumLossesForGameMode(GSM::E_MODE_MARKED_MAN);
    lDescription.mu8ProgressionRank = static_cast<u8>(lpProfile->GetCurrentProgressionRank());

    const u32 luEventId = mpGuiCache->GetEventID();
    const BrnProgression::ProfileEvent* lpProfileEvent = 0;
    // Profile::GetEventCount() is u32; hoist it into an s32 so the walk keeps the
    // console's SIGNED cmpwi/ble pre-guard and cmpw/blt bound (see the banner).
    const s32 liEventCount = static_cast<s32>(lpProfile->GetEventCount());
    for (s32 liEvent = 0; liEvent < liEventCount; ++liEvent)
    {
        if (lpProfile->GetEvent(liEvent)->GetID() == luEventId)
        {
            lpProfileEvent = lpProfile->GetEvent(liEvent);
            break;
        }
    }
    // [PC GUARD RETIRED 2026-08-27, D1 profile-event-list wave] -- twin of the guard retired in
    // SetBurningRouteDescription above; same DELETE-WHEN, same producer
    // (ProgressionManager::UnlockToProgressionRank @0x8239DDE8), same key. Console read restored.
    lDescription.mbEventFlag =
        (lpProfileEvent->GetFlags() & BrnProgression::ProfileEvent::E_FLAG_RANK_WIN) != 0;

    mpStateInterface->GetOutputEventQueue()->AddEvent(
        &lDescription, KI_GUI_OUT_EVENT_CHANNEL, static_cast<s32>(sizeof(lDescription)));
}

// -------------------------------------------------------------------------------------
// SetRaceDescription @ 0x824C6E90 (DWARF cpp:1128) -- fill the description lines for an
// offline race and publish the medals-panel record.
//
// Notes taken from the asm rather than the pseudocode:
//  * The parameterised SetLocalisedText calls are the POSITIONAL-parameter overload
//    (sub_824E7800): `(id, format, liNumParams, <text, format> ...)`, one pair each.
//  * The trailing three lines are blanked by a counted loop (r30 = 3, stride 0x128 from
//    this+0x4D8 == maEventDescriptionText[2]), not by three unrolled calls.
//  * Both profile tallies come off the SAME base (cache+0x405C) at +0x1D4 and +0x224 --
//    the mode-0 slots of maiWinsPerOfflineGameMode / maiLossesPerOfflineGameMode.
//  * The profile-event walk leaves a NULL record pointer when nothing matches and
//    dereferences it anyway; reproduced, see the comment at the site.
// -------------------------------------------------------------------------------------
void PreRaceFlyByState::SetRaceDescription()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1133

    const ECompassPoints leEventDirection = FindEventDirection();
    maEventDescriptionText[0].SetLocalisedText("PRE_RACE_PART1", LM::E_FORMAT_ID_LOOKUP, 1,
                                               KAPC_COMPASS_POINT_STRINGIDS[leEventDirection],
                                               LM::E_FORMAT_ID_LOOKUP);

    GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
    mpGuiCache->GetLandmarkInfoFromIndex(mpGuiCache->GetEventDestinationLandmarkIndex(),
                                         &lLandmarkInfo);

    char lacBuffer[KU_DESCRIPTION_BUFFER_LEN];
    CgsCore::SnPrintf(lacBuffer, KU_DESCRIPTION_BUFFER_LEN, "LM_%llu", lLandmarkInfo.GetCgsId());
    lacBuffer[KU_DESCRIPTION_BUFFER_LEN - 1] = 0;
    maEventDescriptionText[1].SetLocalisedText("PRE_RACE_PART2", LM::E_FORMAT_ID_LOOKUP, 1,
                                               lacBuffer, LM::E_FORMAT_ID_LOOKUP);

    // A race uses only the first two lines; blank the rest.
    for (s32 liLine = 2; liLine < KI_MAX_LINES_DESCRIPTION_TEXT; ++liLine)
        maEventDescriptionText[liLine].SetText("");

    // ---- the id-464 medals-panel record ----
    const BrnProgression::Profile* lpProfile = mpGuiCache->GetProfile();

    GuiEventPreRaceDescription lDescription;
    lDescription.mLandmarkId        = lLandmarkInfo.GetCgsId();
    lDescription.miDirection        = leEventDirection;
    lDescription.miGameMode         = GSM::E_MODE_OFFLINE_RACE;
    lDescription.miTimesPlayed      = lpProfile->GetNumWinsForGameMode(GSM::E_MODE_OFFLINE_RACE)
                                    + lpProfile->GetNumLossesForGameMode(GSM::E_MODE_OFFLINE_RACE);
    lDescription.mu8ProgressionRank = static_cast<u8>(lpProfile->GetCurrentProgressionRank());

    const u32 luEventId = mpGuiCache->GetEventID();
    const BrnProgression::ProfileEvent* lpProfileEvent = 0;
    // Profile::GetEventCount() is u32; hoist it into an s32 so the walk keeps the
    // console's SIGNED cmpwi/ble pre-guard and cmpw/blt bound (see the banner).
    const s32 liEventCount = static_cast<s32>(lpProfile->GetEventCount());
    for (s32 liEvent = 0; liEvent < liEventCount; ++liEvent)
    {
        if (lpProfile->GetEvent(liEvent)->GetID() == luEventId)
        {
            lpProfileEvent = lpProfile->GetEvent(liEvent);
            break;
        }
    }
    // NOTE: the X360 leaves the record pointer NULL when the walk finds no match and reads its
    // flags half-word regardless -- a genuine console null-dereference, reproduced.
    // [PC GUARD RETIRED 2026-08-27, D1 profile-event-list wave] the guard that stood here was
    // there only because the profile held no events at all on this build; its DELETE-WHEN is now
    // due (ProgressionManager::UnlockToProgressionRank @0x8239DDE8 populates the table at boot --
    // see the retirement banner in SetBurningRouteDescription above). Console read restored.
    lDescription.mbEventFlag =
        (lpProfileEvent->GetFlags() & BrnProgression::ProfileEvent::E_FLAG_RANK_WIN) != 0;

    mpStateInterface->GetOutputEventQueue()->AddEvent(
        &lDescription, KI_GUI_OUT_EVENT_CHANNEL, static_cast<s32>(sizeof(lDescription)));
}

}

// ============================================================================
// FOLDED FROM BrnPreRaceFlyBy_wJ_07.cpp (wave J) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===================================================================================
// BrnGui::PreRaceFlyByState -- wave-J partfile 07: the "%d target" description pair.
//   SetRoadRageDescription  @0x824C7078  (DWARF BrnPreRaceFlyBy.cpp:1203)
//   SetFreestyleDescription @0x824C7230  (DWARF BrnPreRaceFlyBy.cpp:1266)
//
// Both bodies were reconstructed instruction by instruction from the raw X360 assembly
// (dumped to scratchpad/waveJ/asm_g07_roadrage.txt and asm_g07_freestyle.txt from
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x824C7078.json / 0x824C7230.json). Both
// pseudocode listings carry the Hex-Rays "local variable allocation has failed" warning
// and are wrong about the SPrintf parameter and the whole event payload, so the asm
// arbitrated everything.
//
// 2026-08-03 RECONCILIATION: both bodies were parked behind six missing names --
// GuiCache::{GetRequiredScoreForMedal, GetEventID},
// BrnGameState::E_CURRENT_MEDAL_TARGET_TIME_GOLD, and Profile::{GetNumWinsForGameMode,
// GetCurrentProgressionRank, GetEventCount}. Every one has landed, so both bodies are
// here. Note GetNumWinsForGameMode reads the +468 array and is NOT the committed
// GetNumRankWinsForGameMode, which reads the different +508 array.
//
// SIGNED-COMPARE NOTE: Profile::GetEventCount() is `u32` (DWARF BrnProfile.h:1009; the
// committed BrnProfile.h:356 matches), but the X360's profile-event walk compares SIGNED:
// `cmpwi cr6, r10, 0` / `ble` @0x824C7194 for the pre-guard and `cmpw cr6, r9, r10` /
// `blt` @0x824C71B4 for the bound (the same pair at 0x824C73D0 / 0x824C73F0 in the
// freestyle body). A raw `s32 i < lpProfile->GetEventCount()` promotes i to unsigned and
// inverts that guard, so both walks hoist the count into an s32 first. The compile gate
// cannot catch this -- both spellings build clean.
//
// CONSOLE-LITERAL NOTE: no X360 displacement is reproduced as a number anywhere here --
// the description lines are reached by INDEX (so the host's own TextField stride applies,
// not the console's 0x128) and the profile/cache far words through accessors. The only
// numeric literals that carry over are byte counts of char buffers (the 128 SPrintf cap
// and its 127 terminator index); the event-record header words are derived with
// sizeof/offsetof off the host record, not baked from the console immediates.
// ===================================================================================


// House file-local alias (same spelling as the sibling PreEvent partfiles).
namespace GSM = BrnGameState::GameStateModuleIO;

namespace BrnGui
{

namespace
{
// (fold: an identical definition of KI_CHANNEL_GUI_OUT was dropped here -- this TU defines it once, above)

    // `li r9, 8` @0x824C7390, stored into the payload's direction word. That 8 is exactly
    // BrnGui::ECompassPoints' E_COMPASS_POINTS_COUNT (BrnGuiShared.h) -- one past the
    // 8-entry DIRECTION_* table, i.e. "this event has no compass direction" -- so the
    // enumerator is named rather than the measured literal repeated. FLAG: the ROLE name
    // below is this partfile's; the value is the header's.
    const s32 KI_COMPASS_DIRECTION_NONE = E_COMPASS_POINTS_COUNT;

// (fold: an identical definition of GuiEventPreRaceDescription was dropped here -- this TU defines it once, above)
    static_assert(sizeof(GuiEventPreRaceDescription) == 40,
                  "the host record matches the X360 40-byte id-464 post");
}

// -------------------------------------------------------------------------------
// SetFreestyleDescription  @0x824C7230   (cpp:1266)
// The stunt-run blurb: the "%d" score target on line 1, the target time as a mid-text
// minutes/seconds string on line 2, three blank lines, and the same id-464 description
// record the other four description workers post.
//
// Notes taken from the raw asm rather than the pseudocode:
//  * Line 1 is identical in shape to the road-rage one: the GuiCache FLOAT
//    mafTargetScores[0] (X360 +0x9F34) through `lfsx`/`fctiwz` (0x824C7290..0x824C7298)
//    into "%d", capped at 128 with a manual terminator at index 127, then
//    SetLocalisedText("PRE_STUNTRACE_PART1", 9, 1, buffer, 11).
//  * The target time is read through the committed GetTargetTimeInEvent(): the X360
//    inlines it here (`addis r30, r11, 1` / `addi r30, r30, -0x60D0` == cache + 0x9F30),
//    INCLUDING its own "0.0f <= mfTargetTime" assert (BrnGuiCache.h:2979, fired at
//    0x824C72EC when the `fcmpu`/`bge` at 0x824C72E4 falls through). The committed
//    accessor body already carries that assert, so calling it reproduces the console.
//  * FormatMinutesSecondsStringMediumText's PPC argument slots are r3 (this), r4 (the
//    buffer), f1 (the time) and r6 (128): the float travels in an FPR and SKIPS r5, so
//    r6 is the buffer size, not a fourth argument. That matches the committed
//    LanguageManager::FormatMinutesSecondsStringMediumText(char*, f32, s32) const.
//  * PART2's PARAMETER format is 0 == E_FORMAT_TEXT (`li r8, 0` at 0x824C7330) -- the
//    string is already formatted -- while the source id itself still resolves under 9.
//    That single immediate is the only shape difference from line 1.
//  * The blanking loop is `r29 = this + 0x4D8, r30 = 3, stride 0x128` -- description
//    fields 2..4 (0 and 1 were just filled). Reached by INDEX, so the host's own
//    TextField stride applies (0x128 is a console stride, comment only).
//  * The profile tallies are again INLINED array reads: `lwz r11, 0x1F0` == the wins
//    array (base 468) at index 7 and `lwz r7, 0x240` == the losses array (base 548) at
//    index 7 -- both E_MODE_STUNT_ATTACK.
//  * The profile-event walk, the NULL dereference when nothing matches, the
//    `(muFlags >> 2) & 1` == E_FLAG_RANK_WIN test and the AddEvent(record, 40, 40) post
//    are identical to the road-rage body; see its banner.
//
// The only float comparison reachable from this body is the >= 0.0f assert inside the
// committed GetTargetTimeInEvent(), so there is no NaN-polarity decision to make here.
// -------------------------------------------------------------------------------
void PreRaceFlyByState::SetFreestyleDescription()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1271

    char lacParameterText[128];
    CgsCore::SPrintf(lacParameterText, sizeof(lacParameterText), "%d",
                     static_cast<s32>(mpGuiCache->GetRequiredScoreForMedal(
                         BrnGameState::E_CURRENT_MEDAL_TARGET_TIME_GOLD)));
    lacParameterText[sizeof(lacParameterText) - 1] = 0;

    maEventDescriptionText[0].SetLocalisedText(
        "PRE_STUNTRACE_PART1",
        CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
        1,
        lacParameterText,
        CgsLanguage::LanguageManager::E_FORMAT_INTEGER);

    mpStateInterface->GetLanguageManager()->FormatMinutesSecondsStringMediumText(
        lacParameterText, mpGuiCache->GetTargetTimeInEvent(),
        static_cast<s32>(sizeof(lacParameterText)));

    maEventDescriptionText[1].SetLocalisedText(
        "PRE_STUNTRACE_PART2",
        CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
        1,
        lacParameterText,
        CgsLanguage::LanguageManager::E_FORMAT_TEXT);

    for (s32 liLine = 2; liLine < KI_MAX_LINES_DESCRIPTION_TEXT; ++liLine)
    {
        maEventDescriptionText[liLine].SetText("");
    }

    const BrnProgression::Profile* lpProfile = mpGuiCache->GetProfile();

    GuiEventPreRaceDescription lDescription;
    lDescription.mLandmarkId        = 0;                          // no destination landmark
    lDescription.miDirection        = KI_COMPASS_DIRECTION_NONE;
    lDescription.miGameMode         = GSM::E_MODE_STUNT_ATTACK;
    lDescription.miTimesPlayed      = lpProfile->GetNumWinsForGameMode(GSM::E_MODE_STUNT_ATTACK)
                                    + lpProfile->GetNumLossesForGameMode(GSM::E_MODE_STUNT_ATTACK);
    lDescription.mu8ProgressionRank = static_cast<u8>(lpProfile->GetCurrentProgressionRank());

    const u32 luEventId = mpGuiCache->GetEventID();
    const BrnProgression::ProfileEvent* lpProfileEvent = 0;
    // Profile::GetEventCount() is u32; hoist it into an s32 so the walk keeps the
    // console's SIGNED cmpwi/ble pre-guard and cmpw/blt bound (see the banner).
    const s32 liEventCount = static_cast<s32>(lpProfile->GetEventCount());
    for (s32 liEvent = 0; liEvent < liEventCount; ++liEvent)
    {
        if (lpProfile->GetEvent(liEvent)->GetID() == luEventId)
        {
            lpProfileEvent = lpProfile->GetEvent(liEvent);
            break;
        }
    }
    // The console dereferences this unconditionally.
    // [PC GUARD RETIRED 2026-08-27, D1 profile-event-list wave] -- this was THE guard that took
    // the AV (run 20260827_133948, callstack SetupComponents -> SetFreestyleDescription ->
    // ProfileEvent::GetFlags reading 0x4), and its DELETE-WHEN ("the profile event list is
    // populated on this build") is now due. The cause was that Profile::AddEvent @0x82359EB8 had
    // no caller: its ONE xref in the whole XEX is ProgressionManager::UnlockToProgressionRank
    // @0x8239DDE8, which is now bodied and runs at boot, giving the profile one ProfileEvent per
    // authored event junction (120 on the retail Progression.dat). GuiCache::muEventID IS that
    // junction id (GuiCache::RecEvent case 93, `muEventID = lpPrepare->muEventJunctionID`), so a
    // running event always has a record and the walk above always hits. Console read restored.
    lDescription.mbEventFlag = static_cast<u8>(
        (lpProfileEvent->GetFlags() & BrnProgression::ProfileEvent::E_FLAG_RANK_WIN) != 0);

    mpStateInterface->GetOutputEventQueue()->AddEvent(&lDescription, KI_CHANNEL_GUI_OUT,
                                                     static_cast<s32>(sizeof(lDescription)));
}

// -------------------------------------------------------------------------------
// SetRoadRageDescription  @0x824C7078   (cpp:1203)
// Road rage's pre-race blurb: one line of "beat <N> takedowns", four blank lines, and
// the id-464 description record for the medals panel.
//
// Notes taken from the raw asm rather than the pseudocode:
//  * The "%d" parameter is the GuiCache FLOAT mafTargetScores[0] (X360 +0x9F34) run
//    through `fctiwz` (0x824C70D4..0x824C70DC: `lfsx f0, r10, r9` / `fctiwz` / `stfiwx`),
//    a round-toward-zero float->s32 conversion -- exactly what a C cast to s32 does. It
//    is NOT the s32 miScoreTarget (+0x9FC8) that the committed GetTargetScoreInEvent
//    returns; the lfsx/fctiwz pair proves the float.
//  * SPrintf's cap (128, `li r4, 0x80`) and the manual terminator index (127, `stb r27,
//    var_31` where var_31 == buffer + 0x7F) are BYTE COUNTS of a char buffer, not
//    pointer-width quantities, so they carry to the host unchanged.
//  * SetLocalisedText's immediates are 9 (E_FORMAT_ID_LOOKUP, the source id) / 1 (one
//    positional parameter) / 11 (E_FORMAT_INTEGER, the parameter's format).
//  * The blanking loop is `r29 = this + 0x3B0, r30 = 4, stride 0x128` -- description
//    fields 1..4, i.e. every line but the one just filled. Reached by INDEX here, so the
//    host's own TextField stride applies (the 0x128 is a console stride, comment only).
//  * The two profile tallies are INLINED array reads, not calls: `lwz r11, 0x1E0` == the
//    wins array (base 468) at index 3 and `lwz r7, 0x230` == the losses array (base 548)
//    at index 3 -- both E_MODE_ROAD_RAGE. The console adds losses + wins; the sum is
//    order-independent.
//  * The profile-event search is an inlined linear walk of maEvents (base +0x7080, stride
//    8) bounded by miEventCount (+0x278) -- the same walk the committed GetEvent's bounds
//    assert guards. When NO record matches, the X360 keeps a NULL pointer and dereferences
//    it (`mr r10, r27` with r27 == 0, then `lhz r8, 4(r10)`); that is reproduced, not
//    guarded -- adding a guard would add behaviour the binary does not have.
//  * The flag byte is `(muFlags >> 2) & 1` (srwi 2 / clrlwi 31) == the E_FLAG_RANK_WIN
//    (4) test.
//  * The post is AddEvent(record, 40, 40): channel 40 and record size 40 happen to share
//    a value here. Record = {24 payload bytes, id 464, payload at +16}.
//
// There are no float comparisons in this body, so there is no NaN-polarity decision.
// -------------------------------------------------------------------------------
void PreRaceFlyByState::SetRoadRageDescription()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1208

    char lacParameterText[128];
    CgsCore::SPrintf(lacParameterText, sizeof(lacParameterText), "%d",
                     static_cast<s32>(mpGuiCache->GetRequiredScoreForMedal(
                         BrnGameState::E_CURRENT_MEDAL_TARGET_TIME_GOLD)));
    lacParameterText[sizeof(lacParameterText) - 1] = 0;

    maEventDescriptionText[0].SetLocalisedText(
        "PRE_ROADRAGE_PART1",
        CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
        1,
        lacParameterText,
        CgsLanguage::LanguageManager::E_FORMAT_INTEGER);

    for (s32 liLine = 1; liLine < KI_MAX_LINES_DESCRIPTION_TEXT; ++liLine)
    {
        maEventDescriptionText[liLine].SetText("");
    }

    const BrnProgression::Profile* lpProfile = mpGuiCache->GetProfile();

    GuiEventPreRaceDescription lDescription;
    lDescription.mLandmarkId        = 0;                          // no destination landmark
    lDescription.miDirection        = KI_COMPASS_DIRECTION_NONE;
    lDescription.miGameMode         = GSM::E_MODE_ROAD_RAGE;
    lDescription.miTimesPlayed      = lpProfile->GetNumWinsForGameMode(GSM::E_MODE_ROAD_RAGE)
                                    + lpProfile->GetNumLossesForGameMode(GSM::E_MODE_ROAD_RAGE);
    lDescription.mu8ProgressionRank = static_cast<u8>(lpProfile->GetCurrentProgressionRank());

    const u32 luEventId = mpGuiCache->GetEventID();
    const BrnProgression::ProfileEvent* lpProfileEvent = 0;
    // Profile::GetEventCount() is u32; hoist it into an s32 so the walk keeps the
    // console's SIGNED cmpwi/ble pre-guard and cmpw/blt bound (see the banner).
    const s32 liEventCount = static_cast<s32>(lpProfile->GetEventCount());
    for (s32 liEvent = 0; liEvent < liEventCount; ++liEvent)
    {
        if (lpProfile->GetEvent(liEvent)->GetID() == luEventId)
        {
            lpProfileEvent = lpProfile->GetEvent(liEvent);
            break;
        }
    }
    // The console dereferences this unconditionally.
    // [PC GUARD RETIRED 2026-08-27, D1 profile-event-list wave] -- twin of the guard retired in
    // SetFreestyleDescription above; same DELETE-WHEN, same producer, same key. Console read
    // restored.
    lDescription.mbEventFlag = static_cast<u8>(
        (lpProfileEvent->GetFlags() & BrnProgression::ProfileEvent::E_FLAG_RANK_WIN) != 0);

    mpStateInterface->GetOutputEventQueue()->AddEvent(&lDescription, KI_CHANNEL_GUI_OUT,
                                                     static_cast<s32>(sizeof(lDescription)));
}
}
