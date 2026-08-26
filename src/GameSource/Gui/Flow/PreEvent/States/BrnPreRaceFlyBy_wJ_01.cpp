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
