// BrnGui::OnlineGameOptions -- static-member definitions (X360 rodata values, dumped in
// wave I: scratchpad/waveI/ogo_rodata.txt / ogo_rodata2.txt).
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineGameOptions.cpp"
namespace BrnGui
{
    // the create-match screen and the shared
    // online frame.
    const CgsGui::sResourceTuple OnlineGameOptions::maResourceTuplesToLoad[] =
        { { 176, CgsGui::E_GUI_RESOURCETYPE_APT }, { 191, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineGameOptions::miNumResourcesToLoad = 2;

    // @0x8205EFD4 -- the observed in-queue event set; @0x8205F000 == 11.
    const s32 OnlineGameOptions::maiEventToObserve[11] =
        { 14, 21, 6, 64, 244, 44, 50, 51, 213, 189, 412 };
    const s32 OnlineGameOptions::miNumEventsObserved = 11;

    // ---- apt component names (the OnEnter/SetupOptions constructor literals) ----------
    const char OnlineGameOptions::KAC_MENU_OPTIONS_COMPONENT[9]          = "MenuItem";
    const char OnlineGameOptions::KAC_CREATE_GAME_TOGGLE_COMPONENT[17]   = "CreateGameToggle";
    const char OnlineGameOptions::KAC_ROUTE_INFO_NAME[10]                = "RouteInfo";
    const char OnlineGameOptions::KAC_UP_ARROW_COMPONENT[13]             = "ArrowUp_anim";
    const char OnlineGameOptions::KAC_DOWN_ARROW_COMPONENT[15]           = "ArrowDown_anim";
    const char OnlineGameOptions::KAC_LOAD_HEADER_COMPONENT[16]          = "LoadHeader_anim";
    const char OnlineGameOptions::KAC_LOAD_HEADER_TEXT_COMPONENT[15]     = "LoadHeaderText";
    const char OnlineGameOptions::KAC_TITLE_TEXT_COMPONENT[11]           = "Title_text";
    const char OnlineGameOptions::KAC_HELP_BAR_COMPONENT[7]              = "Button";
    const char OnlineGameOptions::KAC_MAP_BORDER_ANIMATION_COMPONENT[10] = "Dirt_anim";

    // ---- localisation ids ------------------------------------------------------------
    const char OnlineGameOptions::KAC_GAME_OPTIONS_TITLE_STRING_ID[27]   = "$PAGE_HEADING_CREATE_EVENT";
    const char OnlineGameOptions::KAC_LOAD_OPTIONS_TITLE_STRING_ID[32]   = "$PAGE_HEADING_LOAD_GAME_OPTIONS";
    const char OnlineGameOptions::KAC_CREATED_OPTIONS_STRING_ID[28]      = "$ONLINE_GAME_OPTION_CREATED";
    const char OnlineGameOptions::KAC_RECENT_OPTIONS_STRING_ID[27]       = "$ONLINE_GAME_OPTION_RECENT";
    // The loc KEY (no leading '$') and the SPrintf FORMAT are two different strings.
    const char OnlineGameOptions::KPC_SLOT_STRING_FORMAT_ID[24]          = "ONLINE_GAME_OPTION_SLOT";
    const char OnlineGameOptions::KPC_SLOT_STRING_ID[28]                 = "$ONLINE_GAME_OPTION_SLOT_%d";

    // ---- animation state tables ------------------------------------------------------
    const char* const OnlineGameOptions::KPC_ARROW_ANIMATION_STATES[3] =         // @0x82F2683C
        { "invisible", "visible", "animate" };
    const char* const OnlineGameOptions::KPC_LOAD_HEADER_ANIMATION_STATES[3] =   // @0x82F26848
        { "invisible", "withoutbuttons", "withbuttons" };
    const char* const OnlineGameOptions::KPC_MAP_BORDER_ANIMATION_STATES[2] =    // @0x82F26854
        { "invisible", "visible" };

    // ---- create-match option lists (s32 ids, each E_OPTION_TERMINATOR-terminated) -----
    const CreateMatchOptions::EOption OnlineGameOptions::KAE_COMMON_OPTIONS[2] =              // @0x8205F14C
        { CreateMatchOptions::E_OPTION_GAME_MODE, CreateMatchOptions::E_OPTION_TERMINATOR };
    const CreateMatchOptions::EOption OnlineGameOptions::KAE_RACE_MODE_OPTIONS[5] =           // @0x8205F154
        { CreateMatchOptions::E_OPTION_ROUNDS, CreateMatchOptions::E_OPTION_VEHICLE_CHOICE,
          CreateMatchOptions::E_OPTION_VEHICLE_CLASS, CreateMatchOptions::E_OPTION_TRAFFIC,
          CreateMatchOptions::E_OPTION_TERMINATOR };
    const CreateMatchOptions::EOption OnlineGameOptions::KAE_ROAD_RAGE_MODE_OPTIONS[6] =      // @0x8205F168
        { CreateMatchOptions::E_OPTION_ROUNDS_EVEN, CreateMatchOptions::E_OPTION_VEHICLE_CHOICE,
          CreateMatchOptions::E_OPTION_VEHICLE_CLASS, CreateMatchOptions::E_OPTION_TRAFFIC,
          CreateMatchOptions::E_OPTION_TRAFFIC_CHECKING, CreateMatchOptions::E_OPTION_TERMINATOR };
    const CreateMatchOptions::EOption OnlineGameOptions::KAE_BURNING_HOME_RUN_MODE_OPTIONS[8] = // @0x8205F180
        { CreateMatchOptions::E_OPTION_ROUNDS, CreateMatchOptions::E_OPTION_VEHICLE_CHOICE,
          CreateMatchOptions::E_OPTION_VEHICLE_CLASS, CreateMatchOptions::E_OPTION_TRAFFIC,
          CreateMatchOptions::E_OPTION_INFINITE_BOOST, CreateMatchOptions::E_OPTION_RUNNER_CRASH_LIMIT,
          CreateMatchOptions::E_OPTION_TIME_LIMIT, CreateMatchOptions::E_OPTION_TERMINATOR };
    // FLAG: no DWARF name for the fourth list -- named for the role KAP_GAME_MODE_OPTION_DATA
    // gives it (slots 3/4/5 all point at this one array).
    const CreateMatchOptions::EOption OnlineGameOptions::KAE_DEFAULT_MODE_OPTIONS[5] =        // @0x8205F1A0
        { CreateMatchOptions::E_OPTION_ROUNDS, CreateMatchOptions::E_OPTION_VEHICLE_CHOICE,
          CreateMatchOptions::E_OPTION_VEHICLE_CLASS, CreateMatchOptions::E_OPTION_TRAFFIC,
          CreateMatchOptions::E_OPTION_TERMINATOR };

    // @0x82F26824 -- indexed by GetSelectedGameMode(). Slots 3/4/5 are the SAME pointer,
    // which is why three game modes share one option list; that is the dump, not a typo.
    const CreateMatchOptions::EOption* const OnlineGameOptions::KAP_GAME_MODE_OPTION_DATA[6] =
        { KAE_RACE_MODE_OPTIONS, KAE_ROAD_RAGE_MODE_OPTIONS, KAE_BURNING_HOME_RUN_MODE_OPTIONS,
          KAE_DEFAULT_MODE_OPTIONS, KAE_DEFAULT_MODE_OPTIONS, KAE_DEFAULT_MODE_OPTIONS };

    // @0x8205F1B4 -- DWARF-attested static (DWARF BrnOnlineGameOptions.cpp:223). The X360
    // reaches SetupHelpBar's four {text, button} pairs through folded string/immediate
    // literals rather than loading this table, so it has no xrefs in the image. NO XREFS IS
    // NOT EVIDENCE THE TABLE IS DEAD -- it is the declared source of those pairs and a later
    // sweep must not drop it.
    const OnlineGameOptions::HelpBarItem OnlineGameOptions::KA_HELPBAR_ITEMS[4] =
        { { "$CAPS_BUTTON_BACK_UP",  ButtonIconComponent::EPadButton(5) },
          { "$CAPS_BUTTON_CONTINUE", ButtonIconComponent::EPadButton(4) },
          { "$CAPS_BUTTON_LOAD",     ButtonIconComponent::EPadButton(4) },
          { "$CAPS_BUTTON_LOAD",     ButtonIconComponent::EPadButton(6) } };
}

// FOLDED FROM BrnOnlineGameOptions_wI_01.cpp
// ===================================================================================
// BrnGui::OnlineGameOptions -- wave-I partfile 01: the option-math leaves.
//   GetSelectedGameMode @0x82485A88
//   GetNumberOptions    @0x82485B08
//   GetOptionIndex      @0x8248ED50
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCreateMatchOption.h"   // CreateMatchOptions::EOption

namespace BrnGui
{

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82485B08.json, asm arbitrated over Hex-Rays).
//
// Notes taken from the asm rather than the pseudocode:
//  * The GAME-MODE list is counted first (r8, 0x82485B18 `li r8, 0`) and the COMMON list
//    second (r10), and the return is `common + mode` (0x82485B80 `add r3, r10, r8`).
//  * Both loops are terminator-driven -- they stop on E_OPTION_TERMINATOR(60)
//    (`cmpwi cr6, r10, 0x3C`), never on a count -- and both are peeled do-while shapes
//    (the first element is tested before the counter moves), so an empty list contributes
//    0. Kept as two separate loops, matching the two the X360 emits.
//  * `lwzx r11, r9, 0xA4FC` == mpCommonOptions at CONSOLE offset 42236; not reproduced --
//    the member is reached by name so the host's own layout applies. Likewise the list
//    walk advances by `addi r11, r11, 4` == the console's 4-byte enum stride; on the host
//    the pointer increment is the enum's own sizeof, which is what `++lpe...` gives.
//  * No floats, so there is no NaN-polarity decision to make.
    // @0x82485B08 -- the total number of option groups the create-match page can scroll
    // through: the selected mode's list plus the always-present common list, each counted
    // up to its E_OPTION_TERMINATOR.
    s32 OnlineGameOptions::GetNumberOptions()
    {
        s32 liNumGameModeOptions = 0;
        for (const CreateMatchOptions::EOption* lpeGameModeOption
                 = KAP_GAME_MODE_OPTION_DATA[GetSelectedGameMode()];
             *lpeGameModeOption != CreateMatchOptions::E_OPTION_TERMINATOR;
             ++lpeGameModeOption)
        {
            ++liNumGameModeOptions;
        }

        s32 liNumCommonOptions = 0;
        for (const CreateMatchOptions::EOption* lpeCommonOption = mpCommonOptions;
             *lpeCommonOption != CreateMatchOptions::E_OPTION_TERMINATOR;
             ++lpeCommonOption)
        {
            ++liNumCommonOptions;
        }

        return liNumCommonOptions + liNumGameModeOptions;
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8248ED50.json, asm arbitrated over Hex-Rays).
//
// Notes taken from the asm rather than the pseudocode:
//  * The array base is built as `addis r30, r3, 1` / `addi r30, r30, -0x5D60`, i.e.
//    this + 65536 - 23904 == this + 41632 -- the CONSOLE offset of maOptions. It is
//    deliberately NOT reproduced: the member is reached by name so the host's own layout
//    applies. Likewise the element's option word at `4(r3)` is reached as `.meOption`,
//    and the 8-byte element stride inside GetItem is the container's own sizeof.
//  * The backward scan is guarded `i >= 0` BEFORE each fetch (0x8248EDA4 `cmpwi cr6, r31,
//    0` / `blt`, and 0x8248EDC8 / `bge` at the bottom), so index -1 is never handed to
//    GetItem. When no terminator precedes the row the scan falls out at i == -1 and the
//    result is j + 1 - 2, exactly what the X360 returns.
//  * The forward scan has NO terminator guard on the console -- the list layout
//    guarantees a hit and the X360 runs off the end otherwise. Kept as-is; the committed
//    Array<T,N>::GetItem does bounds-check against the live count, so on the host an
//    absent option fires the container's own "Array index out of bounds" assert instead
//    of walking into the count word. That is the tree's container contract, not an
//    invented guard.
//  * Both comparisons are integer (`cmpw` / `cmpwi ... 0x3C`); no floats in this
//    function, so there is no NaN-polarity decision to make.
    // @0x8248ED50 -- the position of leOption within its own option group. maOptions is
    // the flat {name, option} row list BuildGameOptions produced, laid out as repeated
    // groups of "terminator, group title, value rows...". So the row's index inside its
    // group is (its own index) - (the preceding terminator's index) - 2: the -1 steps off
    // the terminator and the second -1 steps off the group's title row.
    s32 OnlineGameOptions::GetOptionIndex(CreateMatchOptions::EOption leOption)
    {
        s32 liIndex = 0;
        while (maOptions.GetItem(static_cast<u32>(liIndex)).meOption != leOption)
        {
            ++liIndex;
        }

        const s32 liOptionRow = liIndex;
        for (; liIndex >= 0; --liIndex)
        {
            if (maOptions.GetItem(static_cast<u32>(liIndex)).meOption
                    == CreateMatchOptions::E_OPTION_TERMINATOR)
            {
                break;
            }
        }

        return liOptionRow - liIndex - 2;
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82485A88.json, asm arbitrated over Hex-Rays).
//
// The X360 body is a `value - 10` jump table over eight cases:
//     lwzx r11, r3, 0xA278 ; addi r11, r11, -0xA ; cmplwi cr6, r11, 7 ; bgt -> default
// so only mode values 10..17 can reach a table arm, and table entries 0 (mode 10),
// 5 (mode 15) and 6 (mode 16) point back at the default arm. Written as the source-level
// switch over the mode values, which lowers to exactly that table.
//
// 0xA278 == 41592 is the CONSOLE offset of mGameOptions.meGameMode and is deliberately
// NOT reproduced: the field is reached by name so the host's own layout applies.
// No floats in this function, so there is no NaN-polarity decision to make.
    // @0x82485A88 -- which KAP_GAME_MODE_OPTION_DATA list the edited match's mode uses.
    // The mapping is deliberately NOT the identity. MEASURED slot contents (the six
    // pointers dumped at 0x82F26824, cross-checked against the run lengths in rodata and
    // the DWARF array bounds):
    //   0 = KAE_RACE_MODE_OPTIONS            @0x8205F154, 5 entries {22, 8, 11, 37, 60}
    //   1 = KAE_ROAD_RAGE_MODE_OPTIONS       @0x8205F168, 6 entries
    //   2 = KAE_BURNING_HOME_RUN_MODE_OPTIONS@0x8205F180, 8 entries
    //   3, 4 and 5 all hold the SAME pointer @0x8205F1A0 -- one shared 5-entry fallback
    //     list (KAE_DEFAULT_MODE_OPTIONS), not three distinct per-mode lists.
    // So the arms below read: modes 10/15/16 -> the race list, 11 road rage -> the road-rage
    // list, 13 burning home run -> its own list, and 12 fugitive / 14 free burn / 17 -> the
    // shared fallback. The mode values are left as literals with their Dec-07 DWARF
    // enumerator names in comment: the
    // GsmIO::EGameModeType enum spells 17 as E_MODE_ONLINE_MODE_END == E_MODE_COUNT (a
    // sentinel, not a mode), so naming that arm would assert a mode the recovered enum
    // does not have. FLAG: the ARTIST build clearly treats 17 as a real online mode.
    s32 OnlineGameOptions::GetSelectedGameMode() const
    {
        switch (mGameOptions.meGameMode)
        {
            case 11:                     // E_MODE_ONLINE_ROAD_RAGE
                return 1;

            case 12:                     // E_MODE_ONLINE_FUGITIVE
                return 3;

            case 13:                     // E_MODE_ONLINE_BURNING_HOME_RUN
                return 2;

            case 14:                     // E_MODE_ONLINE_FREE_BURN
                return 4;

            case 17:                     // FLAG: DWARF E_MODE_ONLINE_MODE_END / E_MODE_COUNT
                return 5;

            default:                     // 10 race, 15 free-burn lobby, 16 showtime, ...
                return 0;
        }
    }
}

// FOLDED FROM BrnOnlineGameOptions_wI_02.cpp
// ===================================================================================
// BrnGui::OnlineGameOptions -- wave-I partfile 02: the small leaves + the audio wire.
//   CheckPrivileges  @0x82485C18  cpp:1920 (assert)
//   StoreGameMode    @0x82490C68  cpp:1841 (assert)
//   TriggerSound     @0x8249CCF0  cpp:2444 (assert)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + the per-address
// assembly in .ida-exports/BURNOUT_X360_ARTIST.XEX/). These three are the create-match
// options page's leaves: the multiplayer-privilege gate the cache event consults, the
// game-mode commit the store/input paths call, and the per-move audio cue.
//
// The out-queue record is written as a typed wire view with HOST sizeof expressions --
// never the console's baked 100 / 12 / 112 immediates, which are 32-bit-console record
// sizes. Id 457 is homed only in BrnGuiDemangledEventTypes.h, which hard-collides with
// BrnGuiEventTypeDefs.h (needed here for GuiAudioTriggerEvent), so it is carried as a
// file-local wire view -- the same accommodation BrnOnlineGameRoomPlayerInfo_wH_08.cpp,
// BrnCarSelectMain_wG_02.cpp and BrnInGame.cpp make.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache::IsMultiplayerAllowed
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiAudioTriggerEvent

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineGameOptions.cpp".

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // X360 `li r5, 0x28`

        // ---- controller action ids (the in-queue payload's second word) ----------------
        // These are BrnGui's EGameInputActions values. The enum IS fully recovered
        // (references/DecFIGS/dwarfdump/GameSource/Input/GameInputActions.h:24, every
        // enumerator present) and the DWARF even types this very method
        // `void TriggerSound(EGameInputActions)` at BrnOnlineGameOptions.h:637 -- it simply
        // has no committed home under b5-decomp/src yet, which is why the parameter stays
        // s32. Same reason BrnOnlineGameRoomPlayerInfo.h:199 gives.
        //
        // The switch is `r4 - 0x25` against 14 cases, i.e. exactly the contiguous
        // EGameInputActions block 37..50; the four ids 0x2D..0x30 (45 GUI_START,
        // 46 GUI_BACK, 47 GUI_LTHUMB, 48 GUI_RTHUMB) fall into the silent default arm.
        // The two label families are the up/down pair against the left/right pair.
        const s32 KI_ACTION_GUI_DPAD_UP    = 0x25;   // 37 E_GAMEINPUTACTIONS_GUI_DPAD_UP
        const s32 KI_ACTION_GUI_DPAD_DOWN  = 0x26;   // 38 E_GAMEINPUTACTIONS_GUI_DPAD_DOWN
        const s32 KI_ACTION_GUI_DPAD_LEFT  = 0x27;   // 39 E_GAMEINPUTACTIONS_GUI_DPAD_LEFT
        const s32 KI_ACTION_GUI_DPAD_RIGHT = 0x28;   // 40 E_GAMEINPUTACTIONS_GUI_DPAD_RIGHT
        const s32 KI_ACTION_GUI_UP         = 0x29;   // 41 GUI_UP    -> SelectableGroup::HighlightPrevious
        const s32 KI_ACTION_GUI_DOWN       = 0x2A;   // 42 GUI_DOWN  -> SelectableGroup::HighlightNext
        const s32 KI_ACTION_GUI_LEFT       = 0x2B;   // 43 GUI_LEFT  -> HighlightPreviousItem
        const s32 KI_ACTION_GUI_RIGHT      = 0x2C;   // 44 GUI_RIGHT -> HighlightNextItem
        const s32 KI_ACTION_GUI_SELECT     = 0x31;   // 49 GUI_SELECT  accept the page
        const s32 KI_ACTION_GUI_CANCEL     = 0x32;   // 50 GUI_CANCEL  back out of the page

        // The GuiAudioTriggerEvent::meAction value every menu cue in the GUI flow uses
        // (X360 `li r4, 7`). FLAG: the action enum's name is not in the recovered DWARF
        // slice.
        const s32 KI_AUDIO_ACTION_MENU_CUE = 7;

        // The four audio labels the X360 selects between (rodata literals).

        // ---- out-queue wire record -----------------------------------------------------
        // The record the inlined OutputGuiEvent<BrnGui::GuiAudioTriggerEvent> builds:
        // { 100, 457, 12, the 100-byte payload }, channel 40, 112 bytes. The committed PC
        // GuiAudioTriggerEvent already carries that 12-byte queue header in front of its
        // 100-byte payload, so the event object IS the record -- but its GuiEvent<201> base
        // seeds the payload-size word 0 and the id 201, while the X360 wire words are the
        // payload size and the id 457. This view restores them without forking the payload
        // type (BrnOnlineGameRoomPlayerInfo_wH_08.cpp carries the identical view).
        const u32 KU_WIRE_ID_AUDIO_TRIGGER = 457;   // X360 `li r11, 0x1C9`

        typedef CgsGui::GuiEventOut<GuiAudioTriggerEvent> GuiAudioTriggerWire;

        // Record-size pin. The payload is pointer-free, so the X360's AddEvent size
        // immediate must survive unchanged on the x64 host; if a host layout ever drifts,
        // this is where it is caught rather than on the wire.
        CGS_STATIC_ASSERT((sizeof(GuiAudioTriggerWire) == 112), "audio trigger record is 112 bytes");
    }

    // ---- CheckPrivileges @ 0x82485C18 ----------------------------------------------
    // Does the signed-in profile allow online play? The page uses this to decide whether
    // to build itself at all when the cache first arrives.
    bool OnlineGameOptions::CheckPrivileges()
    {
        // cpp:1920 -- non-fatal on the X360; the cache pointer is reloaded and used either
        // way (the assert only reports it).
        CGS_ASSERT(mpGuiCache,
            CGS_ASSERT_MESSAGE_AT("mpGuiCache", __FILE__, 1920 /* ARTIST 0x82485C44 */));

        return mpGuiCache->IsMultiplayerAllowed();
    }

    // ---- StoreGameMode @ 0x82490C68 ------------------------------------------------
    // Commit the game-mode toggle row's current value into the match options. The row is
    // only present while the mode toggle exists in the group, so a missing row (index -1)
    // leaves the stored mode untouched.
    void OnlineGameOptions::StoreGameMode()
    {
        if (mCreateGameToggles.GetIndexFromId(CreateMatchOptions::E_OPTION_GAME_MODE) == -1)
        {
            return;
        }

        // Trap 5 of the wave-I spec: GetHighlightedOption returns the highlighted row's
        // EOption. The Hex-Rays `>>32` at the other call sites is decompiler noise from the
        // u64 register view -- the X360 switches on r3's low word (`addi r11, r3, -1` then
        // `cmplwi r11, 5`, i.e. the six option rows 1..6).
        const CreateMatchOptions::EOption leHighlighted =
            GetHighlightedOption(CreateMatchOptions::E_OPTION_GAME_MODE);

        // The stored values are GsmIO::EGameModeType (the same raw-s32 convention the rest
        // of the Gui slice uses for that field): 10 E_MODE_ONLINE_RACE,
        // 11 E_MODE_ONLINE_ROAD_RAGE, 12 E_MODE_ONLINE_FUGITIVE,
        // 13 E_MODE_ONLINE_BURNING_HOME_RUN, 14 E_MODE_ONLINE_FREE_BURN, 17 the last
        // online mode slot. NOTE the option rows and the modes are NOT in the same order --
        // rows 4/5/6 map to 14/17/13 -- so the mapping is written out case by case exactly
        // as the X360 jump table has it.
        switch (leHighlighted)
        {
            case CreateMatchOptions::E_OPTION_GAME_MODE_RACE:                // 1
                mGameOptions.meGameMode = 10;
                break;

            // Option row 2 is the road-rage mode row. It has no DWARF-attested enumerator
            // (the Dec-07 enum drifted by one from VEHICLE_CHOICE up and this slot is a
            // gap), so the raw ARTIST row value stands in -- wave-I spec section 2.
            case 2:
                mGameOptions.meGameMode = 11;
                break;

            case CreateMatchOptions::E_OPTION_GAME_MODE_STUNT:               // 3
                mGameOptions.meGameMode = 12;
                break;

            case CreateMatchOptions::E_OPTION_GAME_MODE_STUNT_FREE_FOR_ALL:  // 4
                mGameOptions.meGameMode = 14;
                break;

            case CreateMatchOptions::E_OPTION_GAME_MODE_STUNT_COOP:          // 5
                mGameOptions.meGameMode = 17;
                break;

            // Option row 6 is the sixth mode row (burning home run); same un-attested-slot
            // situation as row 2.
            case 6:
                mGameOptions.meGameMode = 13;
                break;

            default:
                // cpp:1841 -- non-fatal; the X360 leaves the stored mode as it was.
                CGS_ASSERT(false,
                    CGS_ASSERT_MESSAGE_AT(
                        "Invalid game mode option", __FILE__, 1841 /* ARTIST 0x82490DB8 */));
                break;
        }
    }

    // ---- TriggerSound @ 0x8249CCF0 -------------------------------------------------
    // Raise the menu audio cue for a controller action: pick the label for the action's
    // family and publish a GuiAudioTriggerEvent with it.
    void OnlineGameOptions::TriggerSound(s32 leGameInputAction)
    {
        const char* lpcLabel = 0;

        switch (leGameInputAction)
        {
            // The four UP/DOWN ids -- moving between option ROWS.
            case KI_ACTION_GUI_DPAD_UP:
            case KI_ACTION_GUI_DPAD_DOWN:
            case KI_ACTION_GUI_UP:
            case KI_ACTION_GUI_DOWN:
                lpcLabel = "MenuToggleDefault";
                break;

            // The four LEFT/RIGHT ids -- moving the highlighted row's VALUE.
            case KI_ACTION_GUI_DPAD_LEFT:
            case KI_ACTION_GUI_DPAD_RIGHT:
            case KI_ACTION_GUI_LEFT:
            case KI_ACTION_GUI_RIGHT:
                lpcLabel = "MenuItemToggleDefault";
                break;

            case KI_ACTION_GUI_SELECT:
                lpcLabel = "Accept";
                break;

            case KI_ACTION_GUI_CANCEL:
                lpcLabel = "GO_BACK";
                break;

            // The remaining four jump-table arms, 0x2D..0x30 == GUI_START / GUI_BACK /
            // GUI_LTHUMB / GUI_RTHUMB, are silent: they share the default arm.
            default:
                break;
        }

        // cpp:2444 -- non-fatal, and the X360 sinks it into the switch's default arm (the
        // other arms provably set lpcLabel). Either placement runs the same code: the event
        // is constructed with the null label on the unrecognised path.
        CGS_ASSERT(lpcLabel, "lpcLabel");

        GuiAudioTriggerEvent lAudioPayload;
        lAudioPayload.Construct(KI_AUDIO_ACTION_MENU_CUE, "", lpcLabel, "");

        mpStateInterface->OutputGuiEvent(lAudioPayload);
    }
}

// FOLDED FROM BrnOnlineGameOptions_wI_03.cpp
// ===================================================================================
// BrnGui::OnlineGameOptions -- wave-I partfile 03: the three small in-queue handlers.
//   HandleControllerInput    @0x824AD758
//   HandleInGameEvent        @0x8249C9D0
//   HandleInGameFailedEvent  @0x8249CAE0
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::Event
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiOverlayWaitFinishRequest (id 188)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache

namespace BrnGui
{

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824AD758.json, asm arbitrated over Hex-Rays).
//
// The dispatch reads ONE word -- `lwz r11, 0x38(r28)` == meSubState -- then compares it
// against 2 and 3; every other sub-state falls straight through to the epilogue with no
// action. 0x38 == 56 is the CONSOLE offset of meSubState and is deliberately NOT
// reproduced: the member is reached by name so the host's own layout applies. The
// parameter is the header-stripped in-queue payload view (const CgsModule::Event*, the
// wave-H precedent); this function never dereferences it, it only null-asserts and
// forwards. No floats here, so there is no NaN-polarity decision to make.
    // ================================================================================
    //  HandleControllerInput  @ 0x824AD758  (cpp:457)
    //
    //  Update's controller-event arm: hand the pad event to whichever sub-state owns the
    //  input. Only the two interactive sub-states take it -- while the screen is still
    //  loading, or is waiting on the game, the event is consumed with no action.
    // ================================================================================
    void OnlineGameOptions::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineGameOptions::HandleControllerInput");   // cpp:457 (non-fatal)

        if (meSubState == E_SUBSTATE_SELECTING_PARAMS)
        {
            HandleControllerInputCreateGame(lpEvent);
        }
        else if (meSubState == E_SUBSTATE_LOAD_OPTIONS)
        {
            HandleControllerInputLoadOptions(lpEvent);
        }
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8249C9D0.json, asm arbitrated over Hex-Rays).
//
// The X360 fires TWO asserts back to back off the SAME null test (lines 2226 and 2228 --
// the first carries a copy-pasted OnlineSelectRoute message), then -- unconditionally,
// unlike the game-room twin, which gates on the payload's in-game byte -- Constructs the
// overlay wait-finish request onto a stack record and publishes it before advancing the
// flow. This handler never dereferences its event pointer at all.
//
// The record the asm stack-builds (0x8249CA90..CAC4) is { 8, 188, 16 } + an 8-aligned
// 8-byte payload, queued with channel 0x28 (40) and size 0x18 (24). Those console
// immediates live in comments only: the wire type below derives every one of them from a
// host sizeof/offset (verified in a scratch compile probe -- host sizeof of the record is
// 24, matching the console record size because the payload is a pointer-free CgsID).
// No floats here, so there is no NaN-polarity decision to make.
    namespace
    {
        // The AddEvent channel selector word (the queue's "type" argument): 40 is the
        // OutputGuiEvent channel this record is published on (`li r5, 0x28`).


        // Id 188 -- the overlay wait-finish handshake. The X360 stack-builds { 8, 188, 16 }
        // and drops the 8-byte compressed overlay id at the 8-aligned payload slot, then
        // publishes 24 bytes on channel 40. Same wire the wave-H game-room screen posts
        // (BrnOnlineGameRoomPlayerInfo_wH_00.cpp), re-declared here because the two screens
        // are separate translation units.
        typedef CgsGui::GuiEvent<188> OverlayWaitFinishHeader;
        struct GuiOverlayWaitFinishWire
        {
            GuiOverlayWaitFinishRequest mRequest;   // the OutputGuiEvent payload

            GuiOverlayWaitFinishWire()
            {
            }
        };

        // Publish the id-188 record for one overlay name -- Construct the request straight
        // onto the wire record, then queue it (the X360 does exactly this inline).
        void PostOverlayWaitFinish(CgsGui::StateInterface* lpStateInterface,
                                   const char* lpacOverlayName)
        {
            GuiOverlayWaitFinishWire lWaitFinish;
            lWaitFinish.mRequest.Construct(lpacOverlayName);
            lpStateInterface->OutputGuiEvent(lWaitFinish.mRequest);   // X360 record size 24
        }
    }

    // ================================================================================
    //  HandleInGameEvent  @ 0x8249C9D0  (cpp:2226)
    //
    //  The match is going in-game: hold the flow until the "entering game" overlay has
    //  finished showing, then advance out of the options screen.
    // ================================================================================
    void OnlineGameOptions::HandleInGameEvent(const CgsModule::Event* lpInGameEvent)
    {
        // Two asserts fire back to back off the same null test; the first carries the
        // original's copy-pasted OnlineSelectRoute message.
        CGS_ASSERT(lpInGameEvent != 0,
                   "Invalid event sent to OnlineSelectRoute::HandleInGameEvent");   // cpp:2226 (non-fatal)
        CGS_ASSERT(lpInGameEvent != 0, "lpInGameEvent");                            // cpp:2228 (non-fatal)

        PostOverlayWaitFinish(mpStateInterface, "CNOnlEntGame");
        SendStateEvent("ADVANCE");
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8249CAE0.json, asm arbitrated over Hex-Rays).
//
// The cache byte is reached as `addis r31, r27, 1 / addi r31, r31, -0x5B00` == this+0xA500
// (42240, mpGuiCache) then `lbz r11, 0x4B53(r11)` == GuiCache+19283, mbOnlineStartPending.
// Both console offsets stay in this comment only -- the members are reached by name so the
// host's own layout applies. Store order follows the asm exactly: publish the record, THEN
// clear the flag, THEN send the state event.
//
// The record the asm stack-builds (0x8249CB94..CBB8) is { 4, 45, 12, 0 } queued with
// channel 0x28 (40) and size 0x10 (16) -- which is exactly the committed
// CgsGui::GuiEventNetworkSuspension(false) (CgsGuiStateInterface.h), whose host sizeof is
// 16, matching the console record size because the payload is a single pointer-free word
// (verified in a scratch compile probe). No floats here, so there is no NaN-polarity
// decision to make.
    namespace
    {
        // The AddEvent channel selector word (the queue's "type" argument): 40 is the
        // OutputGuiEvent channel this record is published on (`li r5, 0x28`).
    }

    // ================================================================================
    //  HandleInGameFailedEvent  @ 0x8249CAE0  (cpp:2250)
    //
    //  The match failed to go in-game. If this screen is the one that armed the online
    //  start, it owns the un-suspend and backs out on the quiet path; otherwise it just
    //  backs out.
    // ================================================================================
    void OnlineGameOptions::HandleInGameFailedEvent(const CgsModule::Event* lpInGameFailedEvent)
    {
        // Same copy-pasted OnlineSelectRoute message the in-game arm above fires.
        CGS_ASSERT(lpInGameFailedEvent != 0,
                   "Invalid event sent to OnlineSelectRoute::HandleInGameEvent");   // cpp:2250 (non-fatal)

        if (mpGuiCache->mbOnlineStartPending)
        {
            // Let the network layer resume before the flow drops back a screen. Posted
            // straight onto the out-queue rather than through
            // StateInterface::OutputGuiEvent, whose committed body passes the event id as
            // the channel argument where the X360 passes 40.
            CgsGui::GuiEventNetworkSuspension lNetworkSuspension;
            lNetworkSuspension.meSuspensionType = CgsGui::GuiEventNetworkSuspension::E_SUSPENSION_TYPE_SUSPEND;
            mpStateInterface->OutputGuiEvent(lNetworkSuspension);   // X360 record size 16

            mpGuiCache->mbOnlineStartPending = false;
            SendStateEvent("GO_BACK_EASY");
        }
        else
        {
            SendStateEvent("GO_BACK");
        }
    }
}

// FOLDED FROM BrnOnlineGameOptions_wI_04.cpp
// ===================================================================================
// BrnGui::OnlineGameOptions -- wave-I partfile 04: the create-game toggle builders.
//   SetupOptions                 @0x8248EB60  (the assert it fires is cpp:1387)
//   SetupCommonCreateGameOptions @0x82490598
//   SetupGameModeOptions         @0x82490630
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT + MessageAt

namespace BrnGui
{

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82490598.json, asm arbitrated over Hex-Rays).
//
// The five-row create-game toggle window is a scroll view over TWO option-id lists laid
// end to end: the common list (mpCommonOptions == KAE_COMMON_OPTIONS) followed by the
// current mode's list. miStartItem is the index of the first visible entry across that
// concatenation, so this half maps common entry i onto toggle row (i - miStartItem) and
// simply produces nothing once the window has scrolled past the common list entirely.
//
// HEX-RAYS MISREADING, arbitrated from the assembly: the call renders with one argument
// missing (`SetupOptions(v1, v6 - *v4)`). The asm loads r5 from the list before every `bl`
// (`lwzx r5, r30, r9` @0x824905F4 seeding the entry test, `lwzx r5, r11, r30` @0x8249061C
// re-seeding it in the loop tail), so the third argument is the option id being laid onto
// that row -- the very word the loop's terminator test just examined.
//
// X360-LITERAL AUDIT: the only console displacements this body touches are the two member
// bases (this+42236 = mpCommonOptions via `addis r28,r29,1 ; addi r28,r28,-0x5B04`, and
// this+42252 = miStartItem via `addi r27,r27,-0x5AF4`) and the option-list element stride 4
// (`slwi r30, r31, 2`), which here is the host's own `sizeof(CreateMatchOptions::EOption)`
// array step. Both members are reached BY NAME, so no console number is reproduced as code.
// No floating-point comparison occurs, so there is no NaN-polarity hazard.
    // ================================================================================
    // SetupCommonCreateGameOptions @ 0x82490598
    //
    // Lay the visible part of the common option list onto the top of the toggle window.
    // ================================================================================
    void OnlineGameOptions::SetupCommonCreateGameOptions()
    {
        // Live length of the common list (walked to its terminator). The console peels the
        // first entry's test out of the loop; kept as written so an already-empty list
        // counts zero rather than walking off the front.
        s32 liNumCommonOptions = 0;
        if (mpCommonOptions[0] != CreateMatchOptions::E_OPTION_TERMINATOR)
        {
            const CreateMatchOptions::EOption* lpeCommonOption = mpCommonOptions;
            do
            {
                ++lpeCommonOption;
                ++liNumCommonOptions;
            }
            while (*lpeCommonOption != CreateMatchOptions::E_OPTION_TERMINATOR);
        }

        // Nothing of the common list is on screen once the window has scrolled past it.
        if (miStartItem < liNumCommonOptions)
        {
            // Entry i lands on toggle row (i - miStartItem). The console re-reads
            // miStartItem from the object on every iteration (`lwz r11, 0(r27)`); it does
            // not change, so the subtraction is written directly.
            s32 liOptionIndex = miStartItem;
            while (mpCommonOptions[liOptionIndex] != CreateMatchOptions::E_OPTION_TERMINATOR)
            {
                SetupOptions(liOptionIndex - miStartItem, mpCommonOptions[liOptionIndex]);
                ++liOptionIndex;
            }
        }
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82490630.json, asm arbitrated over Hex-Rays).
//
// The mode half of the same scroll window SetupCommonCreateGameOptions fills the top of.
// The two option-id lists are laid end to end (common list, then
// KAP_GAME_MODE_OPTION_DATA[GetSelectedGameMode()]), and miStartItem is the index of the
// first visible entry across the concatenation. So either the window still shows part of
// the common list -- the mode list then starts at its own entry 0, on the row the common
// list stopped at -- or the window has scrolled past the common list entirely and row 0 is
// already a mode entry. `cmpw cr6, r9, r11 ; ble` @0x82490680 puts the equality case in the
// FIRST branch (miStartItem == liNumCommonOptions -> entry 0 on row 0, which the second
// branch would also produce).
//
// HEX-RAYS MISREADING, arbitrated from the assembly: the SetupOptions call renders with one
// argument missing (`SetupOptions(a1, v11)`). The asm loads r5 from the list before each
// `bl` (`lwzx r5, r11, r30` @0x824906B4 and @0x824906E8), so the third argument is the
// option id being laid onto that row.
//
// TRAP-10 VERIFICATION (the blank-row SetupToggle argument order), read off the asm at
// 0x8249070C..0x82490728 rather than reasoned:
//     li r9,0 ; li r8,0 ; mr r7,r30("") ; li r6,1 ; li r5,0 ; mr r4,r31(row) ; mr r3,r29
// i.e. (liIndex = row, liNumOptions = 0, lbActive = 1, lpacText = "", lppacOptions = 0,
// lpu64Ids = 0) -- exactly the corrected parameter roles in BrnMenuToggleGroup.h, and the
// same order SetupOptions' populated call uses. The prediction in spec trap 10 holds.
//
// X360-LITERAL AUDIT: the console displacements this body touches are member bases
// (this+42236 = mpCommonOptions via `ori r10,r11,0xA4FC ; lwzx r10,r26,r10`, this+42252 =
// miStartItem via `ori r10,r10,0xA50C`, this+4352 = mCreateGameToggles via
// `addi r29,r26,0x1100`) and the two element strides 4 (`slwi r27,r3,2` over the pointer
// table, `slwi r30,r10,2` over the id list), which are the host's own array steps here.
// Every member and the pointer table are reached BY NAME, so no console number is
// reproduced as code. The window height is the class's KI_MAX_CREATE_GAME_OPTIONS, not the
// bare 5 the console compares against (`cmpwi cr6, r31, 5`). No floating-point comparison
// occurs, so there is no NaN-polarity hazard.
    namespace
    {
        // unk_820046A7 -- the canonical empty string the blank-row SetupToggle passes as
        // its title text.
    }

    // ================================================================================
    // SetupGameModeOptions @ 0x82490630
    //
    // Lay the visible part of the current game mode's option list onto the rows the common
    // list left over, then blank whatever is still empty.
    // ================================================================================
    void OnlineGameOptions::SetupGameModeOptions()
    {
        const s32 liGameMode = GetSelectedGameMode();

        // The mode list is displayed immediately after the common list, so where it starts
        // in the window depends on the common list's length.
        s32 liNumCommonOptions = 0;
        for (const CreateMatchOptions::EOption* lpeCommonOption = mpCommonOptions;
             *lpeCommonOption != CreateMatchOptions::E_OPTION_TERMINATOR;
             ++lpeCommonOption)
        {
            ++liNumCommonOptions;
        }

        s32 liOptionIndex;   // first mode entry the window shows
        s32 liStartRow;      // toggle row that entry lands on
        if (miStartItem <= liNumCommonOptions)
        {
            // The window still shows some of the common list (or starts exactly where the
            // mode list does), so the mode list begins at its own first entry.
            liOptionIndex = 0;
            liStartRow    = liNumCommonOptions - miStartItem;
        }
        else
        {
            // The window has scrolled into the mode list; row 0 is already a mode entry.
            liOptionIndex = miStartItem - liNumCommonOptions;
            liStartRow    = 0;
        }

        const CreateMatchOptions::EOption* lpeGameModeOptions =
            KAP_GAME_MODE_OPTION_DATA[liGameMode];

        s32 liNumEmitted = 0;
        if (lpeGameModeOptions[liOptionIndex] != CreateMatchOptions::E_OPTION_TERMINATOR)
        {
            s32 liRow = liStartRow;
            do
            {
                // The window is only five rows tall; the console breaks out before the call
                // rather than before the terminator test, so liNumEmitted stops here too.
                if (liRow >= KI_MAX_CREATE_GAME_OPTIONS)
                {
                    break;
                }

                SetupOptions(liRow, lpeGameModeOptions[liOptionIndex]);

                ++liOptionIndex;
                ++liNumEmitted;
                ++liRow;
            }
            while (lpeGameModeOptions[liOptionIndex] != CreateMatchOptions::E_OPTION_TERMINATOR);
        }

        // Blank every row the two lists did not reach: an empty toggle with no options at
        // all. The console recomputes the first blank row as (emitted + start row) rather
        // than reusing the loop cursor; the two agree because the cursor advanced in step.
        for (s32 liEmptyRow = liNumEmitted + liStartRow;
             liEmptyRow < KI_MAX_CREATE_GAME_OPTIONS;
             ++liEmptyRow)
        {
            mCreateGameToggles.SetupToggle(liEmptyRow, 0, true, "", 0, 0);
        }
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8248EB60.json, asm arbitrated over Hex-Rays).
//
// SetupOptions turns ONE option-group id into one populated row of the five-row create-game
// toggle window. maOptions is the {name, option} table BuildGameOptions
// @0x8248CA98 fills; a group is a title row whose meOption IS the group id, followed by its
// value rows, ended by an E_OPTION_TERMINATOR row.
//
// ⭐ HEX-RAYS MISREADING, arbitrated from the assembly -- and it overturns the wave-I spec's
// trap 1. The pseudocode renders each 64-bit toggle id as a pack of the row's name pointer
// into the high word and the option into the low word:
//     HIDWORD(v16) = *Item; LODWORD(v16) = v12;
// and the spec predicted a console pointer pack whose consumers read only the low word. The
// asm @0x8248EBF8..0x8248EC14 is:
//     lwz r10, 0(r3)      ; r10 = the row's mpcName
//     extsw r11, r28      ; r11 = the row's meOption, SIGN-EXTENDED to 64 bits
//     stw r10, 0(r26)     ; -> the const char* array  (r26 += 4)
//     std r11, 0(r27)     ; -> the u64 id array       (r27 += 8)
// The name goes ONLY into the parallel text array; the id IS the option value and nothing
// else. There is no pack, so no host-pointer-width accommodation is needed at all. Hex-Rays
// fused the independent r10/r11 pair into one doubleword view. The same `extsw` appears on
// the SetSelectableId argument @0x8248EC7C.
//
// X360-LITERAL AUDIT: the only console displacements this body touches are member bases
// (this+41632 = maOptions via `addis r29,r20,1 ; addi r29,r29,-0x5D60`, this+4352 =
// mCreateGameToggles via `addi r30,r20,0x1100`) and the two stack-buffer strides 4 and 8,
// which are the host's own `sizeof(const char*)` / `sizeof(u64)` array steps here. Both
// members are reached BY NAME, so no console number is reproduced as code. maOptions'
// element stride never appears -- Array<T,N>::GetItem does the host arithmetic. No
// floating-point comparison occurs, so there is no NaN-polarity hazard.
    namespace
    {
        // The stack option-name / option-id buffers this body builds are 1024 bytes and
        // 2048 bytes on the console (`[sp+50h]` 256 words, `[sp+450h]` 256 doublewords), and
        // the guard it fires is `cmpwi cr6, r31, 0x100`.
        const s32 KI_MAX_TOGGLE_OPTIONS = 256;
    }

    // ================================================================================
    // SetupOptions @ 0x8248EB60
    //
    // Populate toggle row liToggleIndex from the option group headed by leOption: the
    // group's title row in maOptions is the entry whose meOption == leOption, and the rows
    // that follow it up to the next terminator are the group's selectable values.
    // ================================================================================
    void OnlineGameOptions::SetupOptions(s32 liToggleIndex, CreateMatchOptions::EOption leOption)
    {
        const char* lapcOptionNames[KI_MAX_TOGGLE_OPTIONS];
        u64         lau64OptionIds[KI_MAX_TOGGLE_OPTIONS];

        // Locate the title row. The console runs this scan with no terminator guard and no
        // assert -- BuildGameOptions always emits a title row for every id that can reach
        // here, so the search is guaranteed to hit.
        s32 liTitleIndex = -1;
        do
        {
            ++liTitleIndex;
        }
        while (maOptions.GetItem(static_cast<u32>(liTitleIndex)).meOption != leOption);

        // The toggle's label is the title row's own name string (`lwz r23, 0(r11)`).
        const char* lpcTitleName = maOptions.GetItem(static_cast<u32>(liTitleIndex)).mpcName;

        s32 liRowIndex   = liTitleIndex + 1;
        s32 liNumOptions = 0;

        CreateMatchOptions::EOption leRowOption =
            maOptions.GetItem(static_cast<u32>(liRowIndex)).meOption;

        if (leRowOption != CreateMatchOptions::E_OPTION_TERMINATOR)
        {
            do
            {
                const CreateMatchOptions& lrRow = maOptions.GetItem(static_cast<u32>(liRowIndex));

                lapcOptionNames[liNumOptions] = lrRow.mpcName;
                // `extsw r11, r28 ; std r11, 0(r27)` -- the id is the row's option value,
                // sign-extended to 64 bits (see the banner note).
                lau64OptionIds[liNumOptions] = static_cast<u64>(static_cast<s64>(leRowOption));
                ++liNumOptions;

                // The console tests AFTER the pair of stores, so the guard is one row late.
                // It cannot be reached in practice -- maOptions holds at most 75 entries in
                // total -- and it is kept where the console put it.
                // ARTIST passes the filename at 0x82060B60 and line 1387.
                // Keep the original diagnostic location after folding the source.
                CGS_ASSERT(liNumOptions <= KI_MAX_TOGGLE_OPTIONS,
                           CGS_ASSERT_MESSAGE_AT("Too many options specified",
                               CGS_ARTIST_FILE, 1387));

                ++liRowIndex;
                leRowOption = maOptions.GetItem(static_cast<u32>(liRowIndex)).meOption;
            }
            while (leRowOption != CreateMatchOptions::E_OPTION_TERMINATOR);

            if (liNumOptions > 0)
            {
                // r3 = this + 0x1100 (mCreateGameToggles); r4 = index, r5 = count, r6 = 1,
                // r7 = title text, r8 = the name array, r9 = the id array -- the corrected
                // parameter roles in BrnMenuToggleGroup.h.
                mCreateGameToggles.SetupToggle(liToggleIndex, liNumOptions, true, lpcTitleName,
                                               lapcOptionNames, lau64OptionIds);

                // `extsw r5, r22` -- the row's own selectable id is the group id.
                mCreateGameToggles.SetSelectableId(liToggleIndex,
                                                   static_cast<u64>(static_cast<s64>(leOption)));
            }
        }
    }
}

// FOLDED FROM BrnOnlineGameOptions_wI_05.cpp
// wave-I partfile

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::PlayAptMovie
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache / GuiFlow
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / the out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiEventActivateCrashNav (id 191)

namespace BrnGui
{

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824A7758.json, asm arbitrated over Hex-Rays).
//
// Notes taken from the asm rather than the pseudocode:
//  * EnsureResourcesAreLoaded is the THREE-ARG face, not the no-arg one the spec's triage
//    line guessed: 0x824A77B0..0x824A77BC set r4 = &maResourceTuplesToLoad (unk_8205F004)
//    and r5 = 2, matching BrnGuiCache.h:210
//    `bool EnsureResourcesAreLoaded(const CgsGui::sResourceTuple*, u32)`. The 2 is the
//    same-TU constant-fold of miNumResourcesToLoad, so the source form is the static.
//  * Its result is consumed as a BYTE (`clrlwi r11, r3, 24`), which is what the bool
//    return already gives; likewise AreAllAptComponentsInitialised.
//  * The X360 re-loads AND re-tests mpGuiCache on the substate-1 arm (0x824A77FC /
//    0x824A7800) even though the assert at the top already covered it -- CGS_ASSERT is
//    non-fatal, so that second test is real behaviour and is kept.
//  * `stwx r11, r31, 0xA0B0` (41136) is mRouteInfoDisplay.miNumComponentsLoaded
//    (32528 + 8608), and the value stored is the loaded word dword_8204C7A0 ==
//    GuiNetworkRouteInfo::KI_NUM_COMPONENTS_TO_LOAD (11). Both are public members, so this
//    is a named store, NOT an offset poke -- the console offsets are recorded only.
//  * `stwx r11, r31, 0xA4FC` (42236) is mpCommonOptions = &unk_8205F14C == KAE_COMMON_OPTIONS.
//  * The GuiFlow argument is `li r4, 0` == E_GUIFLOW_SCREEN on both cache calls.
//  * No floats anywhere in this body, so there is no NaN-polarity decision to make.
    namespace
    {
        // ---- the create-match apt movie ------------------------------------------------
        // PlayAptMovie(off_82F27BA0[0], 3) @0x824A77E4. (Shared with the OnEnter partfile --
        // keep only one copy on merge.)
        const s32  KI_APT_MOVIE_LEVEL   = 3;
    }

    // ================================================================================
    //  CheckForCompletedLoads  @ 0x824A7758
    //
    //  Driven from Update once per frame while the page is still coming up: first wait
    //  for the screen's two resources, then for the apt components the cache is watching.
    // ================================================================================
    void OnlineGameOptions::CheckForCompletedLoads()
    {
        CGS_ASSERT(mpGuiCache != 0, CGS_ASSERT_MESSAGE_AT("mpGuiCache", CGS_ARTIST_FILE, 413 /* ARTIST 0x824A779C */));   // cpp:413 (non-fatal)

        switch (meSubState)
        {
            case E_SUBSTATE_LOADING_SCREEN:
                // The screen's own resource list (the header's GetResourcesToLoad pair);
                // the X360 folds miNumResourcesToLoad to its value here.
                if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                         static_cast<u32>(miNumResourcesToLoad)))
                {
                    mpStateInterface->PlayAptMovie("ON_CREA", KI_APT_MOVIE_LEVEL);
                    meSubState = E_SUBSTATE_LOADING_COMPONENTS;
                }
                break;

            case E_SUBSTATE_LOADING_COMPONENTS:
                // The X360 re-loads and re-tests mpGuiCache on this arm (the assert above is
                // non-fatal, so the pointer really can still be null here).
                if (mpGuiCache != 0 &&
                    mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
                {
                    mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

                    // The route panel's components came up with the rest of the screen, so
                    // mark it fully loaded rather than counting them again.
                    mRouteInfoDisplay.miNumComponentsLoaded =
                        GuiNetworkRouteInfo::KI_NUM_COMPONENTS_TO_LOAD;

                    mHelpBar.SetupComponent();

                    // The always-present half of the option list (just the game-mode row).
                    mpCommonOptions = KAE_COMMON_OPTIONS;

                    ShowGameOptionsScreen();
                }
                break;

            default:
                break;
        }
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8249C188.json, asm arbitrated over Hex-Rays).
//
// Notes taken from the asm rather than the pseudocode:
//  * CONSTRUCTION ORDER is the asm's, not the DWARF declaration order: menu, toggles,
//    route info, up arrow (+0x7A90), down arrow (+0x7B1C), load-header animator (+0x7BA8),
//    load-header text (+0x7CC0), title text (+0x7DE8), THEN the help bar (+0x5C40), and
//    the map border (+0x7C34) LAST. The five middle calls are `lwz r10, <this+off>` /
//    `lwz r11, 0(r10)` / `bctrl` -- the GuiComponent Construct virtual (vtable slot 0),
//    written here as the plain named call.
//  * The two group Constructs pass `li r8, -1` + `clrldi r8, r8, 32`: a 32-bit all-ones
//    apt id ZERO-EXTENDED into the u64 parameter, i.e. 0xFFFFFFFFull -- NOT (u64)-1.
//    BrnOnlinePlay.cpp writes the same literal for the same parameter.
//  * The console member offsets in the comments (+56, +42236, ...) are recorded only.
//    Every access below is by name, so the host's own layout applies. `stbx r28, r31, r9`
//    at +42256 is the byte store into the bool mbIsCreating.
//  * The four out-queue records are stack-built at sp+0x50 and reuse the same slots, which
//    is why Hex-Rays shows the `ld`/`std` doubleword shuffle: what actually ends up on the
//    wire is decoded per record below. All four go out on channel 0x28 == 40.
//  * meOptionsToLoad (+42244) is stored AFTER the four AddEvent calls (0x8249C438) -- keep
//    that order; it is the only store in the epilogue.
//  * No floats anywhere in this body, so there is no NaN-polarity decision to make.
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) ----------------------------


        // ---- the create-match apt movie ------------------------------------------------
        // PlayAptMovie(off_82F27BA0[0], 3) @0x824A77E4 (used by CheckForCompletedLoads).

        // The apt id the two group Constructs pass. The X360 builds it with
        // `li r8, -1` + `clrldi r8, r8, 32`, i.e. the 32-bit all-ones id zero-extended
        // into the u64 parameter -- NOT a 64-bit -1.
        const u64 KU_INVALID_APT_ID = 0xFFFFFFFFull;

        // ---- out-queue wire records -----------------------------------------------------
        // Each is a CgsGui::GuiEvent<N> header { payload bytes, event id, payload offset }
        // followed by the payload, published through
        // mpStateInterface->GetOutputEventQueue()->AddEvent(record, channel, sizeof(record)).
        // The X360 record sizes quoted are the CONSOLE sizes; all three payloads below are
        // pointer-free so the host record has the same shape, but the header words are
        // still DERIVED from sizeof/offsetof rather than stamped with the console literal
        // (BrnLicenseComponent.cpp precedent).

        // Id 264 == BrnGui::GuiEventNetworkOutputPlayerTexture -- the same record
        // BrnLicenseComponent.cpp / BrnPhotoBoothComponent.cpp build: { mode, playerIndex },
        // X360 record size 20. The create-match page asks for mode 1 on player -1 (the X360
        // packs the pair as one doubleword before overwriting the header words in place).
        struct GuiEventNetworkOutputPlayerTextureRecord : public CgsGui::GuiEvent<264>
        {
            s32 meMode;          // payload +0x00
            s32 miPlayerIndex;   // payload +0x04

            GuiEventNetworkOutputPlayerTextureRecord(s32 leMode, s32 liPlayerIndex)
                : meMode(leMode), miPlayerIndex(liPlayerIndex)
            {
                // The subtraction is only safe here because the two-s32 payload leaves no
                // trailing padding, so sizeof - offsetof really is the payload byte count
                // (X360 `li r27, 8` at 0x8249C368). Do NOT copy this form onto a record
                // whose payload is narrower than the record's alignment -- see the two
                // bool records below, which take the payload size directly.
            }
            GuiEventNetworkOutputPlayerTextureRecord() {}
        };

        // Id 148 == BrnGui::GuiEventShowHideHud (payload size 1). OnEnter drops the HUD as
        // the create-match page comes up. NOTE the channel: this screen posts it on the GUI
        // OUT channel (40), not the internal channel (42) the game-room screen uses.
        // X360 record size 16.
        //
        // HEADER0 IS THE PAYLOAD BYTE COUNT, NOT sizeof(record) - offsetof(payload). On the
        // 8-byte payload above the two agree, but a lone bool pads this record out to 16,
        // so the subtraction would publish 4 where the X360 publishes 1: 0x8249C33C
        // `li r28, 1`, stored as word0 at 0x8249C3EC (this record) and 0x8249C424 (the id-65
        // twin), with word2 = r29 = 12 and the AddEvent size `li r6, 0x10` == 16. So header0
        // is taken from the payload MEMBER, the way the wave-H twin does it
        // (BrnOnlineGameRoomPlayerInfo_wH_07.cpp builds the same id-148 record as (1, 12)).
        struct GuiEventShowHideHudWire : public CgsGui::GuiEvent<148>
        {
            bool mbShowHud;   // payload +0x00

            explicit GuiEventShowHideHudWire(bool lbShowHud)
                : mbShowHud(lbShowHud)
            {
            }
            GuiEventShowHideHudWire() {}
        };

        // Id 65 == the car-control-change request (BrnGuiDemangledEventTypes.h names it
        // GuiRequestCarControlChangeEvent; that header cannot be included alongside
        // BrnGuiEventTypeDefs.h, so the record is rebuilt here). One payload byte, cleared
        // -- the create-match page takes car control away while the menu is up.
        // X360 record size 16.
        struct GuiRequestCarControlChangeWire : public CgsGui::GuiEvent<65>
        {
            bool mbCarControlEnabled;   // payload +0x00

            // Same payload-byte-count rule as the id-148 record above (X360 word0 = 1 from
            // the shared r28, AddEvent size 16 at 0x8249C40C).
            explicit GuiRequestCarControlChangeWire(bool lbCarControlEnabled)
                : mbCarControlEnabled(lbCarControlEnabled)
            {
            }
            GuiRequestCarControlChangeWire() {}
        };
    }

    // ================================================================================
    //  OnEnter  @ 0x8249C188
    //
    //  Build the create-match option table, register the screen's event set, construct
    //  every embedded component in the X360's order, reset the screen's own state and
    //  publish the four entry-time records.
    // ================================================================================
    void OnlineGameOptions::OnEnter()
    {
        // The option table ({name, option} rows) has to exist before anything reads it.
        BuildGameOptions();

        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // ---- components, in the X360's construction order ------------------------------
        // The six load slots and the five create-game toggle rows.
        mMenuOptions.Construct(KAC_MENU_OPTIONS_COMPONENT, mpStateInterface,
                               KI_MAX_LOAD_OPTIONS, 0, KU_INVALID_APT_ID);
        mCreateGameToggles.Construct(KAC_CREATE_GAME_TOGGLE_COMPONENT, mpStateInterface,
                                     KI_MAX_CREATE_GAME_OPTIONS, 0, KU_INVALID_APT_ID);

        // The route panel: (name, map type 0, state interface, no sat-nav icon base name).
        mRouteInfoDisplay.Construct(KAC_ROUTE_INFO_NAME, 0, mpStateInterface, 0);

        // The plain components go through the GuiComponent Construct virtual (vtable slot 0).
        mUpArrowAnimator.Construct(KAC_UP_ARROW_COMPONENT, mpStateInterface, 0);
        mDownArrowAnimator.Construct(KAC_DOWN_ARROW_COMPONENT, mpStateInterface, 0);
        mLoadHeaderAnimator.Construct(KAC_LOAD_HEADER_COMPONENT, mpStateInterface, 0);
        mLoadHeaderText.Construct(KAC_LOAD_HEADER_TEXT_COMPONENT, mpStateInterface, 0);
        mTitleText.Construct(KAC_TITLE_TEXT_COMPONENT, mpStateInterface, 0);

        // The help bar sits between the text fields and the map border in the X360's order.
        mHelpBar.Construct(KAC_HELP_BAR_COMPONENT, KI_MAX_HELP_BAR_ITEMS, mpStateInterface, 0);

        mMapBorderAnimator.Construct(KAC_MAP_BORDER_ANIMATION_COMPONENT, mpStateInterface, 0);

        // ---- the screen's own state ------------------------------------------------------
        meSubState      = E_SUBSTATE_LOADING_SCREEN;
        mpGuiCache      = 0;
        mpCommonOptions = 0;
        miCurrentRound  = 0;
        miStartItem     = 0;
        mbIsCreating    = true;   // X360 stbx -- a byte store

        mGameOptions.Construct();

        // ---- the four entry-time records, all on channel 40 -------------------------------
        // Start outputting the local player's gamer picture into the create-match page.
        GuiEventNetworkOutputPlayerTextureRecord lOutputPlayerTexture(1, -1);
        mpStateInterface->OutputGuiEvent(lOutputPlayerTexture);   // X360 record size 20

        // Take the front-end map (CrashNav) down while the create-match page owns the screen.
        GuiEventActivateCrashNav lActivateCrashNav;
        lActivateCrashNav.muActivate = 0u;
        lActivateCrashNav.muParam = 0;
        mpStateInterface->OutputGuiEvent(lActivateCrashNav);      // X360 record size 20

        GuiEventShowHideHudWire lHideHud(false);
        mpStateInterface->OutputGuiEvent(lHideHud);               // X360 record size 16

        GuiRequestCarControlChangeWire lDisableCarControl(false);
        mpStateInterface->OutputGuiEvent(lDisableCarControl);     // X360 record size 16

        // The X360 stores this LAST, after the four posts -- keep the order.
        meOptionsToLoad = E_OPTIONS_TO_LOAD_SAVED;
    }
}

// FOLDED FROM BrnOnlineGameOptions_wI_06.cpp
// ===================================================================================
// BrnGui::OnlineGameOptions -- wave-I partfile 06: the create-page presenter pair.
//   SetupHelpBar          @0x82485C78  cpp:1936 / cpp:1995 (asserts)
//   ShowGameOptionsScreen @0x8249C5C8
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper / GuiEventQueueLarge
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface (out-queue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // BrnGui::OptionsDataProfile (the saved/received counters)
#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.h"  // BrnGui::GuiNetworkRouteInfo::EState
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"          // BrnGui::ButtonIconComponent::EPadButton

namespace BrnGui
{

//
// BrnGui::OnlineGameOptions -- wave-I partfile 06: the create-page presenter pair.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   SetupHelpBar           @ 0x82485C78 (BrnOnlineGameOptions.cpp asserts at :1936 / :1995)
//   ShowGameOptionsScreen  @ 0x8249C5C8
//
// The class shape, the rodata tables and the sibling bodies live in the wave-I scaffold
// BrnOnlineGameOptions.{h,cpp} and the other BrnOnlineGameOptions_wI_XX.cpp partfiles.
//
// X360 offsets appear in comments only -- the host layout is name-based (pointers and
// component vptrs widen on x64, so no console offset, stride or record size is
// reproduced as a literal here). The one console byte count that does show up, the
// 24-byte id-213 view/internal-state record, is written as sizeof(the wrapper record):
// CgsGui::GuiEventWrapper is three s32 header words plus a pointer-free 12-byte
// payload, so the host size equals the console size.
//
// HEADER-CLASH NOTE (wave-H BrnOnlineGameRoomPlayerInfo_wH_09.cpp precedent): the id-213
// payload's real home is BrnGuiDemangledEventTypes.h, which hard-collides with
// BrnGuiEventTypeDefs.h (pulled in through BrnGuiCache.h). The two view-state records
// this file posts therefore use the same file-local payload view wH_09 uses.
//
// SPEC CORRECTION (measured, not reasoned): wave-I spec §3 says the help-bar pad-button
// ids "have no ButtonIconComponent::EPadButton home in the tree". They do --
// GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h declares the enum, and its
// values line up exactly with the three ids this function stores (4 == SELECT,
// 5 == BACK, 6 == OPTION0) and with the 15 the append call passes for its third
// argument (15 == E_PADBUTTON_INVISIBLE -- the same {text, button, INVISIBLE} shape
// BrnOnlineGameRoomPlayerInfo's HelpItem::SetItem calls use). No local id enum is
// forked; the real home is included.
// ===================================================================================
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) ---------------------------
        const s32 KI_CHANNEL_VIEW_STATE   = 41;   // OutputViewState
        const s32 KI_CHANNEL_GUI_INTERNAL = 42;   // OutputInternalState

        // The apt view-state key every animation component on this screen is driven with.

        // ---- help-bar items: the class's own KA_HELPBAR_ITEMS table is the home ---------
        // MEASURED (headless IDA): KA_HELPBAR_ITEMS @0x8205F1B4 is four
        // {const char*, s32} records --
        //   [0] {"$CAPS_BUTTON_BACK_UP",  5 == E_PADBUTTON_BACK}
        //   [1] {"$CAPS_BUTTON_CONTINUE", 4 == E_PADBUTTON_SELECT}
        //   [2] {"$CAPS_BUTTON_LOAD",     4 == E_PADBUTTON_SELECT}
        //   [3] {"$CAPS_BUTTON_LOAD",     6 == E_PADBUTTON_OPTION0}
        // -- which are precisely and only the four pairs SetupHelpBar stores at
        // 0x82485CD4..0x82485D70, in the order 2 / 1 / 3 / 0. The table itself has ZERO
        // xrefs in the whole image and SetupHelpBar reaches the strings through
        // `lis aCapsButtonLoad@ha` with the button ids as `li` immediates, so nothing is
        // read from 0x8205F1B4 at run time. The reading that fits both facts is that the
        // X360 compiler constant-folded the element reads (the array is a same-TU const
        // with a visible initialiser, defined at DWARF BrnOnlineGameOptions.cpp:223) and
        // still emitted the array because a class static has external linkage. That is an
        // inference from the fold, not something the asm states -- but it is the only
        // account of why an otherwise unreferenced rodata table carries exactly these four
        // pairs, so the pairs are sourced from the table rather than forked as file-local
        // literals. Emitted values are byte-identical either way. NOTE FOR A LATER SWEEP:
        // "no xrefs" is NOT evidence that KA_HELPBAR_ITEMS is dead -- do not drop it.

        // ---- out-queue payload view -----------------------------------------------------
        // GuiEventShowHideSatNav (wire id 213, payload 12 bytes). Its real home cannot be
        // included here (see the banner) and models the type as an empty struct, so the
        // three payload words are named as wave-H named them. FLAG: word roles not
        // recovered; the receiving CustomRendererManager keys event 213 by a sub-mode word
        // and a renderable flag.
        struct GuiEventShowHideSatNavPayload
        {
            s32  miSubMode;    // +0x00  X360 stores 0
            f32  mfValue;      // +0x04  X360 stores 0.0f (flt_82001CC0)
            bool mbFlag;       // +0x08  X360 stores 0 here (ShowLoadScreen stores 1)
            u8   maPad[3];     // +0x09  never written by the X360; modelled zeroed


            s32 GetEventType() const { return 213; }
        };
    }

    // ================================================================================
    //  SetupHelpBar  @ 0x82485C78  (BrnOnlineGameOptions.cpp, asserts at :1936 / :1995)
    //
    //  Rebuild the bottom help bar for whichever page is currently up. The load page
    //  offers only "load"; the create page offers "continue", plus a second "load"
    //  prompt when the player actually has saved or received option sets to load from.
    //  Both pages always end with the back-up prompt.
    //
    //  The DWARF signature carries a bool parameter and both call sites pass 0, but the
    //  X360 body never reads r4 -- the flag is dead on this build.
    // ================================================================================
    void OnlineGameOptions::SetupHelpBar(bool /*lbUnused*/)
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:1936 (non-fatal)

        // The X360 builds two parallel stack arrays (buttons then texts, both walked with
        // a word-scaled index) rather than an array of the class's HelpBarItem pairs.
        const char*                     lapcHelpBarText[KI_MAX_HELP_BAR_ITEMS];
        ButtonIconComponent::EPadButton laeHelpBarButton[KI_MAX_HELP_BAR_ITEMS];
        s32                             liHelpBarItemCount;

        // The X360 tests the common create-page sub-state first (`cmpwi 2 / beq`) and only
        // then the load sub-state; the two compares collapse to this single test because
        // the create arm and the fall-through arm are the same block.
        //
        // The static_cast on every meButton read is the header's doing, not the data's:
        // BrnOnlineGameOptions.h models HelpBarItem::meButton as s32 while the DWARF types
        // it BrnGui::ButtonIconComponent::EPadButton. The values are the enumerators.
        if (meSubState == E_SUBSTATE_LOAD_OPTIONS)
        {
            lapcHelpBarText[0]  = KA_HELPBAR_ITEMS[2].mpcText;    // "$CAPS_BUTTON_LOAD"
            laeHelpBarButton[0] = static_cast<ButtonIconComponent::EPadButton>(
                                      KA_HELPBAR_ITEMS[2].meButton);   // 4 SELECT
            liHelpBarItemCount  = 1;
        }
        else
        {
            lapcHelpBarText[0]  = KA_HELPBAR_ITEMS[1].mpcText;    // "$CAPS_BUTTON_CONTINUE"
            laeHelpBarButton[0] = static_cast<ButtonIconComponent::EPadButton>(
                                      KA_HELPBAR_ITEMS[1].meButton);   // 4 SELECT
            liHelpBarItemCount  = 1;

            // The two counters live in the cache's player-options profile block: the X360
            // folds both the profile fetch and the two counter reads into single
            // displacements off the cache pointer (cache+76672 / +76676 == the profile at
            // cache+0xB878 plus its +0x7308 / +0x730C). Un-inlined back onto the profile's
            // own public accessors rather than poking the (private) members.
            const OptionsDataProfile* lpOptionsDataProfile = mpGuiCache->GetMenuOptionsData();
            if (lpOptionsDataProfile->GetNumCreatedOnlineGameOptions() > 0 ||
                lpOptionsDataProfile->GetNumReceivedOnlineGameOptions() > 0)
            {
                lapcHelpBarText[1]  = KA_HELPBAR_ITEMS[3].mpcText;    // "$CAPS_BUTTON_LOAD"
                laeHelpBarButton[1] = static_cast<ButtonIconComponent::EPadButton>(
                                          KA_HELPBAR_ITEMS[3].meButton);   // 6 OPTION0
                liHelpBarItemCount  = 2;
            }
        }

        // The back-up prompt is written at the running count and only then counted in --
        // which is why the bound check below runs on the incremented count.
        lapcHelpBarText[liHelpBarItemCount]  = KA_HELPBAR_ITEMS[0].mpcText;  // "$CAPS_BUTTON_BACK_UP"
        laeHelpBarButton[liHelpBarItemCount] = static_cast<ButtonIconComponent::EPadButton>(
                                                   KA_HELPBAR_ITEMS[0].meButton);   // 5 BACK
        ++liHelpBarItemCount;

        CGS_ASSERT(liHelpBarItemCount <= KI_MAX_HELP_BAR_ITEMS,
                   "liHelpBarItemCount <= KI_MAX_HELP_BAR_ITEMS");   // cpp:1995 (non-fatal)

        mHelpBar.Clear();   // this+0x5C40
        for (s32 liItem = 0; liItem < liHelpBarItemCount; ++liItem)
        {
            // Third argument 15: the append's optional SECOND button icon, left invisible
            // (E_PADBUTTON_INVISIBLE). The callee asserts both button arguments < 0x10.
            // Both are ButtonIconComponent::EPadButton in the DWARF signature (s32 return,
            // BrnHelpBar.h:103), so the enumerators go through untouched; the s32 return is
            // the appended item's index and the X360 discards it here.
            mHelpBar.AppendHelpBarItem(lapcHelpBarText[liItem],
                                       laeHelpBarButton[liItem],
                                       ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        }
    }

    // ================================================================================
    //  ShowGameOptionsScreen  @ 0x8249C5C8
    //
    //  Bring up the create-match page: drop the load-slot menu, re-arm the five-row
    //  create-game toggle group, hide the route-info panel, rebuild the option toggles
    //  from the common option list and re-highlight them from the current match
    //  parameters, then push the page's title, its help bar and its apt view states.
    // ================================================================================
    void OnlineGameOptions::ShowGameOptionsScreen()
    {
        meSubState = E_SUBSTATE_SELECTING_PARAMS;   // this+0x38

        mMenuOptions.Clear();                                            // this+0x40, group vtable slot 6
        mCreateGameToggles.SetupGroup(KI_MAX_CREATE_GAME_OPTIONS, false); // this+0x1100
        mRouteInfoDisplay.SetState(GuiNetworkRouteInfo::E_STATE_INVISIBLE); // this+0x7F10, X360 `li r4, 1`

        SetupCommonCreateGameOptions();
        HighlightCreateGameOptions();
        SetupHelpBar(false);   // X360 `li r4, 0` -- the dead flag

        mTitleText.SetText(KAC_GAME_OPTIONS_TITLE_STRING_ID);   // this+0x7DE8
        miCurrentRound = 0;                                     // this+0xA508

        // The id-213 show/hide record { payload size 12, type 213, payload offset 12,
        // { 0, 0.0f, false } }, published on the view-state channel and then again on the
        // internal-state channel (X360 record size 24 both times).
        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue =
            mpStateInterface->GetOutputEventQueue();

        GuiEventShowHideSatNavPayload lShowHideSatNav;
        lShowHideSatNav.miSubMode = 0;
        lShowHideSatNav.mfValue   = 0.0f;
        lShowHideSatNav.mbFlag    = false;

        mpStateInterface->OutputViewState(lShowHideSatNav);

        mpStateInterface->OutputInternalState(lShowHideSatNav);

        // The load page's furniture goes away with the page (this+0x7BA8 / this+0x7C34).
        mLoadHeaderAnimator.AddOutputAptViewState("apt_Transition",
                                                  KPC_LOAD_HEADER_ANIMATION_STATES[0], false);
        mMapBorderAnimator.AddOutputAptViewState("apt_Transition",
                                                 KPC_MAP_BORDER_ANIMATION_STATES[0], false);

        // The scroll arrows only appear when there is more than one option group to scroll
        // through. The X360 drives the DOWN arrow first (this+0x7B1C), then the UP arrow
        // (this+0x7A90), and reloads the same state string for the second call.
        if (GetNumberOptions() > 1)
        {
            mDownArrowAnimator.AddOutputAptViewState("apt_Transition",
                                                     KPC_ARROW_ANIMATION_STATES[1], false);
            mUpArrowAnimator.AddOutputAptViewState("apt_Transition",
                                                   KPC_ARROW_ANIMATION_STATES[1], false);
        }
        else
        {
            mDownArrowAnimator.AddOutputAptViewState("apt_Transition",
                                                     KPC_ARROW_ANIMATION_STATES[0], false);
            mUpArrowAnimator.AddOutputAptViewState("apt_Transition",
                                                   KPC_ARROW_ANIMATION_STATES[0], false);
        }
    }
}

// FOLDED FROM BrnOnlineGameOptions_wI_07.cpp
// ===================================================================================
// BrnGui::OnlineGameOptions -- wave-I partfile 07: the store/highlight mirror pair over
// the create-match option lists.
//   StoreCreateGameOptions     @0x82492BD8  (asserts cpp:1684 / cpp:1777)
//   HighlightCreateGameOptions @0x82490740  (asserts cpp:1496 / cpp:1611)
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"              // BrnGui::GuiCache (mbOnlineMatchRanked)

namespace BrnGui
{
    namespace
    {
        // The two game-mode rows BuildGameOptions @0x8248CA98 emits that the wave-I option
        // enum leaves as unnamed gaps (spec §2: values 2 and 6 are not attested by a DWARF
        // enumerator). Their roles are fixed by StoreGameMode @0x82490C68, which maps
        // highlighted row 2 -> mode 11 and row 6 -> mode 13; HighlightCreateGameOptions is
        // that map read backwards. FLAG: role-derived names, not DWARF names.
        const CreateMatchOptions::EOption KE_OPTION_GAME_MODE_ROAD_RAGE =
            static_cast<CreateMatchOptions::EOption>(2);
        const CreateMatchOptions::EOption KE_OPTION_GAME_MODE_BURNING_HOME_RUN =
            static_cast<CreateMatchOptions::EOption>(6);
    }

    // @0x82492BD8 -- fold the create-match toggle rows back into mGameOptions.
    //
    // The toggle rows carry the option id as their selectable id, so the list entry IS the
    // lookup key: GetIndexFromId(-1) means this mode's list mentions an option that is not
    // on screen right now (the five-row scroll window), and that entry is skipped.
    void OnlineGameOptions::StoreCreateGameOptions()
    {
        StoreGameMode();

        const CreateMatchOptions::EOption* lpeGameModeOptions =
            KAP_GAME_MODE_OPTION_DATA[GetSelectedGameMode()];
        CGS_ASSERT(lpeGameModeOptions != 0, "lpeGameModeOptions");            // cpp:1684

        // The ranked flag is not a toggle row -- it comes off the cache the online menu
        // set when the player picked ranked or unranked play.
        mGameOptions.mbRanked = mpGuiCache->mbOnlineMatchRanked;

        while (*lpeGameModeOptions != CreateMatchOptions::E_OPTION_TERMINATOR)
        {
            const CreateMatchOptions::EOption leOption = *lpeGameModeOptions;

            // The console sign-extends the id word into the 64-bit id argument (`extsw`).
            if (mCreateGameToggles.GetIndexFromId(static_cast<u64>(leOption)) != -1)
            {
                const CreateMatchOptions::EOption leHighlighted = GetHighlightedOption(leOption);

                switch (leOption)
                {
                    case CreateMatchOptions::E_OPTION_VEHICLE_CHOICE:            // 8
                        mGameOptions.meVehicleChoice = GetOptionIndex(leHighlighted);
                        break;

                    case CreateMatchOptions::E_OPTION_VEHICLE_CLASS:             // 11
                        mGameOptions.miVehicleClass = GetOptionIndex(leHighlighted);
                        break;

                    // One shared arm for both rounds lists -- see the banner: the even
                    // list stores its sequence index + 1, not the round count.
                    case CreateMatchOptions::E_OPTION_ROUNDS:                    // 22
                    case CreateMatchOptions::E_OPTION_ROUNDS_EVEN:               // 23
                        mGameOptions.miNumRounds = GetOptionIndex(leHighlighted) + 1;
                        break;

                    case CreateMatchOptions::E_OPTION_INFINITE_BOOST:            // 34
                        mGameOptions.mbInfiniteBoost =
                            (leHighlighted == CreateMatchOptions::E_OPTION_INFINITE_BOOST_ON);
                        break;

                    case CreateMatchOptions::E_OPTION_TRAFFIC:                   // 37
                        mGameOptions.mbTrafficOn =
                            (leHighlighted == CreateMatchOptions::E_OPTION_TRAFFIC_ON);
                        break;

                    case CreateMatchOptions::E_OPTION_TRAFFIC_CHECKING:          // 40
                        mGameOptions.mbTrafficCheckingOn =
                            (leHighlighted == CreateMatchOptions::E_OPTION_TRAFFIC_CHECKING_ON);
                        break;

                    case CreateMatchOptions::E_OPTION_BOOST_TYPE:                // 43
                        mGameOptions.meBoostType = GetOptionIndex(leHighlighted);
                        break;

                    case CreateMatchOptions::E_OPTION_RUNNER_CRASH_LIMIT:        // 49
                        mGameOptions.miNumRunnerCrashes = GetOptionIndex(leHighlighted) + 1;
                        break;

                    case CreateMatchOptions::E_OPTION_TIME_LIMIT:                // 56
                        mGameOptions.miTimeLimit = GetOptionIndex(leHighlighted);
                        break;

                    default:
                        CGS_ASSERT(false, "Unknown game mode option");          // cpp:1777
                        break;
                }
            }

            ++lpeGameModeOptions;
        }
    }

    // @0x82490740 -- drive every visible toggle row from the stored match options.
    //
    // Two halves: the game-mode row itself (which also selects WHICH option list the page
    // shows, and rebuilds the toggle rows for it), then that list's rows.
    void OnlineGameOptions::HighlightCreateGameOptions()
    {
        const s32 liGameModeRow = mCreateGameToggles.GetIndexFromId(
            static_cast<u64>(CreateMatchOptions::E_OPTION_GAME_MODE));

        const CreateMatchOptions::EOption* lpeGameModeOptions;

        // Mode values are GsmIO::EGameModeType; left as literals with the enumerator name
        // in comment, matching the sibling wave-I partfiles (the recovered enum spells 17
        // E_MODE_ONLINE_MODE_END == E_MODE_COUNT, a sentinel the ARTIST build nonetheless
        // uses as a real online mode).
        switch (mGameOptions.meGameMode)
        {
            case 10:                                    // E_MODE_ONLINE_RACE
                if (liGameModeRow > -1)
                {
                    mCreateGameToggles.HighlightItem(
                        liGameModeRow,
                        GetOptionIndex(CreateMatchOptions::E_OPTION_GAME_MODE_RACE));
                }
                SetupGameModeOptions();
                lpeGameModeOptions = KAE_RACE_MODE_OPTIONS;
                break;

            case 11:                                    // E_MODE_ONLINE_ROAD_RAGE
                if (liGameModeRow > -1)
                {
                    mCreateGameToggles.HighlightItem(
                        liGameModeRow, GetOptionIndex(KE_OPTION_GAME_MODE_ROAD_RAGE));
                }
                SetupGameModeOptions();
                lpeGameModeOptions = KAE_ROAD_RAGE_MODE_OPTIONS;
                break;

            case 12:                                    // E_MODE_ONLINE_FUGITIVE
                if (liGameModeRow > -1)
                {
                    mCreateGameToggles.HighlightItem(
                        liGameModeRow,
                        GetOptionIndex(CreateMatchOptions::E_OPTION_GAME_MODE_STUNT));
                }
                SetupGameModeOptions();
                lpeGameModeOptions = KAE_DEFAULT_MODE_OPTIONS;
                break;

            case 13:                                    // E_MODE_ONLINE_BURNING_HOME_RUN
                if (liGameModeRow > -1)
                {
                    mCreateGameToggles.HighlightItem(
                        liGameModeRow, GetOptionIndex(KE_OPTION_GAME_MODE_BURNING_HOME_RUN));
                }
                SetupGameModeOptions();
                lpeGameModeOptions = KAE_BURNING_HOME_RUN_MODE_OPTIONS;
                break;

            case 14:                                    // E_MODE_ONLINE_FREE_BURN
                if (liGameModeRow > -1)
                {
                    mCreateGameToggles.HighlightItem(
                        liGameModeRow,
                        GetOptionIndex(CreateMatchOptions::E_OPTION_GAME_MODE_STUNT_FREE_FOR_ALL));
                }
                SetupGameModeOptions();
                lpeGameModeOptions = KAE_DEFAULT_MODE_OPTIONS;
                break;

            case 15:                                    // E_MODE_ONLINE_FREE_BURN_LOBBY
                if (liGameModeRow > -1)
                {
                    mCreateGameToggles.HighlightItem(
                        liGameModeRow,
                        GetOptionIndex(CreateMatchOptions::E_OPTION_GAME_MODE_RACE));
                }
                SetupGameModeOptions();
                lpeGameModeOptions = KAE_RACE_MODE_OPTIONS;
                break;

            case 17:                                    // FLAG: DWARF E_MODE_ONLINE_MODE_END
                if (liGameModeRow > -1)
                {
                    mCreateGameToggles.HighlightItem(
                        liGameModeRow,
                        GetOptionIndex(CreateMatchOptions::E_OPTION_GAME_MODE_STUNT_COOP));
                }
                SetupGameModeOptions();
                lpeGameModeOptions = KAE_DEFAULT_MODE_OPTIONS;
                break;

            default:                                    // 16 E_MODE_ONLINE_SHOWTIME, ...
                // The list is taken BEFORE the assert fires, and this arm alone skips
                // SetupGameModeOptions -- so the rows below are highlighted against
                // whatever the group already holds.
                lpeGameModeOptions = KAE_RACE_MODE_OPTIONS;
                CGS_ASSERT(false, "Invalid game mode option");                  // cpp:1496
                break;
        }

        // Post-tested walk: the first entry is always processed (see the banner).
        do
        {
            const CreateMatchOptions::EOption leOption = *lpeGameModeOptions;
            const s32 liRow = mCreateGameToggles.GetIndexFromId(static_cast<u64>(leOption));

            if (liRow != -1)
            {
                switch (leOption)
                {
                    case CreateMatchOptions::E_OPTION_VEHICLE_CHOICE:            // 8
                        mCreateGameToggles.HighlightItem(liRow, mGameOptions.meVehicleChoice);
                        break;

                    case CreateMatchOptions::E_OPTION_VEHICLE_CLASS:             // 11
                        mCreateGameToggles.HighlightItem(liRow, mGameOptions.miVehicleClass);
                        break;

                    case CreateMatchOptions::E_OPTION_ROUNDS:                    // 22
                        mCreateGameToggles.HighlightItem(liRow, mGameOptions.miNumRounds - 1);
                        break;

                    case CreateMatchOptions::E_OPTION_ROUNDS_EVEN:               // 23
                        mCreateGameToggles.HighlightItem(liRow, mGameOptions.miNumRounds / 2 - 1);
                        break;

                    case CreateMatchOptions::E_OPTION_INFINITE_BOOST:            // 34
                        mCreateGameToggles.HighlightItem(
                            liRow,
                            GetOptionIndex(mGameOptions.mbInfiniteBoost
                                               ? CreateMatchOptions::E_OPTION_INFINITE_BOOST_ON
                                               : CreateMatchOptions::E_OPTION_INFINITE_BOOST_OFF));
                        break;

                    case CreateMatchOptions::E_OPTION_TRAFFIC:                   // 37
                        mCreateGameToggles.HighlightItem(
                            liRow,
                            GetOptionIndex(mGameOptions.mbTrafficOn
                                               ? CreateMatchOptions::E_OPTION_TRAFFIC_ON
                                               : CreateMatchOptions::E_OPTION_TRAFFIC_OFF));
                        break;

                    case CreateMatchOptions::E_OPTION_TRAFFIC_CHECKING:          // 40
                        mCreateGameToggles.HighlightItem(
                            liRow,
                            GetOptionIndex(mGameOptions.mbTrafficCheckingOn
                                               ? CreateMatchOptions::E_OPTION_TRAFFIC_CHECKING_ON
                                               : CreateMatchOptions::E_OPTION_TRAFFIC_CHECKING_OFF));
                        break;

                    case CreateMatchOptions::E_OPTION_BOOST_TYPE:                // 43
                        mCreateGameToggles.HighlightItem(liRow, mGameOptions.meBoostType);
                        break;

                    case CreateMatchOptions::E_OPTION_RUNNER_CRASH_LIMIT:        // 49
                        mCreateGameToggles.HighlightItem(liRow,
                                                         mGameOptions.miNumRunnerCrashes - 1);
                        break;

                    case CreateMatchOptions::E_OPTION_TIME_LIMIT:                // 56
                        mCreateGameToggles.HighlightItem(liRow, mGameOptions.miTimeLimit);
                        break;

                    default:
                        CGS_ASSERT(false, "Unknown game mode option");          // cpp:1611
                        break;
                }
            }

            ++lpeGameModeOptions;
        }
        while (*lpeGameModeOptions != CreateMatchOptions::E_OPTION_TERMINATOR);
    }
}

// FOLDED FROM BrnOnlineGameOptions_wI_08.cpp
// ===================================================================================
// BrnGui::OnlineGameOptions -- wave-I partfile 08: the load-page pair.
//   ShowLoadScreen   @0x8249C760  cpp:2078 (assert)
//   ShowLoadOptions  @0x8248C878  cpp:2151 (assert)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode arbitrated by
// the raw asm). ShowLoadScreen commits whatever the create page was editing, flips the
// state machine into the load sub-state and dresses the 6-slot save-slot page; it then
// hands the per-slot text/enable work to ShowLoadOptions, which also primes the route
// preview panel from save-route [0].
//
// VTABLE SLOT -> METHOD, DUMPED not inferred. Both functions dispatch through the
// MenuComponent component vtable @0x82074068 (this partfile's dump, via idat on
// BURNOUT_X360_ARTIST.XEX -- scratchpad/waveI/ogo_vt.txt):
//     +0x18 (slot  6)  MenuComponent::Clear            (the toggle-group face on <5>)
//     +0x20 (slot  8)  SelectableGroup::Enable   @0x824E3490
//     +0x24 (slot  9)  SelectableGroup::Disable  @0x824E3620
//     +0x30 (slot 12)  SelectableGroup::HighlightIndex @0x824E3CE0
// The wave-I spec hedged the empty-slot call toward MenuComponent::DisableSelectable
// (@0x824E2E38); the dumped table says slot 9 is SelectableGroup::Disable, which is a
// DIFFERENT function (Disable clears the Active flag and asserts the row is not the last
// enabled one; DisableSelectable clears Highlightable+Selectable). The dump is the
// authority, so ShowLoadOptions calls Enable/Disable.
//
// HOST-vs-CONSOLE. Every X360 immediate in these two bodies is either a *count* (6 slots,
// 128-byte text buffers, the 41/42 channel selectors, the localisation format words 0/9/11)
// or a *member offset the console inlined* (0x12B80/0x12B84 == the created/received option
// counters at GuiCache+0xB878+0x7308/+0x730C; +0x8 / +0x3988 == maCreated/maReceived[0]).
// None of the offsets are written here: the inlined GuiCache::GetOptionsDataProfile() and
// the four OptionsDataProfile accessors are restored as real calls (inlining reversal --
// their "lpGuiCache" asserts at BrnGuiOptionsDataProfile.h:611/632 are exactly what the
// X360 inlined into ShowLoadOptions). The one AddEvent size is the host sizeof of the
// record type, not the console's 24.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue / language manager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // LanguageManager::FormatAndAddText
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache::GetOptionsDataProfile
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // OptionsDataProfile online-save-route accessors
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"           // the 480-byte params the route fills

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------



        // The apt view-state animation channel every screen component drives.

        // The "%d" the slot-number parameter is printed through before the localisation
        // manager formats it into the slot label.

        // The source text an EMPTY save slot is given: a single space, formatted raw.

        // Both SPrintf targets are 128-byte stack buffers on the X360 (`li r4, 0x80`).
        const u32 KU_SLOT_TEXT_BUFFER_SIZE = 128;

        // Id 213 == BrnGui::GuiEventShowHideSatNav (BrnGuiDemangledEventTypes.h:371), which
        // hard-collides with BrnGuiEventTypeDefs.h, so it is carried here as a file-local
        // wire view -- the same 24-byte { 12, 213, 12, s32, f32, u8 } record
        // BrnFBurnMainHudState.cpp's GuiShowHideEvent24 and BrnOnlineGameRoomPlayerInfo_wH_01
        // post. ShowLoadScreen publishes it with the trailing flag SET (X360 `stb r25(1)`),
        // where the create page publishes the same record with the flag clear.
        typedef CgsGui::GuiEvent<213> ShowHideHeader;
        struct GuiEventShowHideWire : public ShowHideHeader
        {
            s32 miShow;      // +0x0C
            f32 mfValue;     // +0x10
            u8  mu8Flag;     // +0x14
            u8  maPad[3];    // +0x15 (the record is 24 bytes on the wire)

            GuiEventShowHideWire(s32 liShow, f32 lfValue, u8 lu8Flag)
                : miShow(liShow)
                , mfValue(lfValue)
                , mu8Flag(lu8Flag)
            {
                maPad[0] = 0;
                maPad[1] = 0;
                maPad[2] = 0;
            }
            GuiEventShowHideWire() {}
        };

        // Record-size pin. The payload is pointer-free, so the X360's AddEvent size
        // immediate must survive unchanged on the x64 host.
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiEventShowHideWire>) == 24), "show/hide record is 24 bytes");
    }

    // ---- ShowLoadScreen @ 0x8249C760 -----------------------------------------------
    // Leave the create page (committing its toggles) and bring the load page up: pick the
    // save-slot family that actually has entries -- the player's own saved options when
    // there are any, otherwise the received/recent ones -- title the list accordingly,
    // rebuild the 6-slot menu through ShowLoadOptions, and put the page's help bar,
    // scroll arrows and view-state up.
    void OnlineGameOptions::ShowLoadScreen()
    {
        StoreCreateGameOptions();

        meSubState = E_SUBSTATE_LOAD_OPTIONS;

        // The X360 inlines the cache's profile fetch and reads the two counters straight
        // out of the far block; restored as the real accessor chain.
        OptionsDataProfile* lpOptionsData = mpGuiCache->GetMenuOptionsData();

        s32         liNumRoutes;
        const char* lpacHeaderTextId;

        if (lpOptionsData->GetNumCreatedOnlineGameOptions() > 0)
        {
            liNumRoutes      = lpOptionsData->GetNumCreatedOnlineGameOptions();
            lpacHeaderTextId = KAC_CREATED_OPTIONS_STRING_ID;
            meOptionsToLoad  = E_OPTIONS_TO_LOAD_SAVED;
        }
        else
        {
            // cpp:2078 -- non-fatal; the X360 falls straight through into the recent list.
            CGS_ASSERT(lpOptionsData->GetNumReceivedOnlineGameOptions() > 0,
                       "mpGuiCache->GetMenuOptionsData()->GetNumReceivedOnlineGameOptions() > 0");

            liNumRoutes      = lpOptionsData->GetNumReceivedOnlineGameOptions();
            lpacHeaderTextId = KAC_RECENT_OPTIONS_STRING_ID;
            meOptionsToLoad  = E_OPTIONS_TO_LOAD_RECENT;
        }

        mLoadHeaderText.SetText(lpacHeaderTextId);

        // The header only offers its "switch list" buttons when BOTH families have
        // entries to switch between.
        const char* lpacLoadHeaderState =
            ((lpOptionsData->GetNumCreatedOnlineGameOptions() <= 0) ||
             (lpOptionsData->GetNumReceivedOnlineGameOptions() <= 0))
                ? KPC_LOAD_HEADER_ANIMATION_STATES[1]    // "withoutbuttons"
                : KPC_LOAD_HEADER_ANIMATION_STATES[2];   // "withbuttons"

        mLoadHeaderAnimator.AddOutputAptViewState("apt_Transition", lpacLoadHeaderState, false);
        mMapBorderAnimator.AddOutputAptViewState("apt_Transition",
                                                 KPC_MAP_BORDER_ANIMATION_STATES[1], false);   // "visible"

        miStartItem = 0;
        mCreateGameToggles.Clear();

        mMenuOptions.SetupMenu(KI_MAX_LOAD_OPTIONS, false);
        ShowLoadOptions();
        mMenuOptions.HighlightIndex(0);

        mTitleText.SetText(KAC_LOAD_OPTIONS_TITLE_STRING_ID);
        SetupHelpBar(false);

        // A single-entry list cannot scroll, so both arrows go away; otherwise both show.
        // (X360 order: the DOWN animator first, then the UP one.)
        const char* lpacArrowState = (liNumRoutes <= 1) ? KPC_ARROW_ANIMATION_STATES[0]    // "invisible"
                                                        : KPC_ARROW_ANIMATION_STATES[1];   // "visible"
        mDownArrowAnimator.AddOutputAptViewState("apt_Transition", lpacArrowState, false);
        mUpArrowAnimator.AddOutputAptViewState("apt_Transition", lpacArrowState, false);

        // The view-state pair: the same record on the view-state channel and then on the
        // internal one, with the trailing flag set (the load page's form of the record).
        GuiEventShowHideWire lShowHide(0, 0.0f, 1);

        mpStateInterface->OutputViewState(lShowHide);
        mpStateInterface->OutputInternalState(lShowHide);
    }

    // ---- ShowLoadOptions @ 0x8248C878 ----------------------------------------------
    // Fill the six visible save-slot rows from the currently selected save-route family,
    // starting at the scroll window's first item: a slot backed by a route gets the
    // localised "Slot <n>" label and is enabled, a slot past the end gets a blank label
    // and is disabled. Then reset the round cursor and preview route [0] in the route
    // info panel.
    void OnlineGameOptions::ShowLoadOptions()
    {
        OptionsDataProfile* lpOptionsData = mpGuiCache->GetMenuOptionsData();

        s32 liNumRoutes;
        if (meOptionsToLoad == E_OPTIONS_TO_LOAD_SAVED)
        {
            liNumRoutes = lpOptionsData->GetNumCreatedOnlineGameOptions();
        }
        else
        {
            // cpp:2151 -- non-fatal; the X360 reads the received counter either way.
            CGS_ASSERT(meOptionsToLoad == E_OPTIONS_TO_LOAD_RECENT,
                       "meOptionsToLoad == E_OPTIONS_TO_LOAD_RECENT");

            liNumRoutes = lpOptionsData->GetNumReceivedOnlineGameOptions();
        }

        char lacSlotId[KU_SLOT_TEXT_BUFFER_SIZE];
        char lacNumber[KU_SLOT_TEXT_BUFFER_SIZE];

        for (s32 liSlot = 0; liSlot < KI_MAX_LOAD_OPTIONS; ++liSlot)
        {
            const s32 liRouteIndex = miStartItem + liSlot;

            // "$ONLINE_GAME_OPTION_SLOT_<slot>" -- keyed by the VISIBLE row, not the route.
            CgsCore::SPrintf(lacSlotId, KU_SLOT_TEXT_BUFFER_SIZE, KPC_SLOT_STRING_ID, liSlot);

            // The label is registered under lacSlotId + 1: dropping the leading '$' makes
            // the added string's id match the '$'-prefixed lookup MenuComponent::SetText
            // performs below.
            if (liRouteIndex < liNumRoutes)
            {
                CgsCore::SPrintf(lacNumber, KU_SLOT_TEXT_BUFFER_SIZE, "%d",
                                 liRouteIndex + 1);

                mpStateInterface->GetLanguageManager()->FormatAndAddText(
                    lacSlotId + 1, KPC_SLOT_STRING_FORMAT_ID,
                    CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 1,
                    lacNumber, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
            }
            else
            {
                mpStateInterface->GetLanguageManager()->FormatAndAddText(
                    lacSlotId + 1, " ",
                    CgsLanguage::LanguageManager::E_FORMAT_TEXT);
            }

            mMenuOptions.SetText(liSlot, lacSlotId);

            // Component vtable slot 8 / slot 9 (dumped -- see the banner).
            if (liRouteIndex < liNumRoutes)
            {
                mMenuOptions.Enable(liSlot);
            }
            else
            {
                mMenuOptions.Disable(liSlot);
            }
        }

        miCurrentRound = 0;
        mRouteInfoDisplay.SetState(GuiNetworkRouteInfo::E_STATE_VISIBLE);

        // Preview the first route of the selected family. The X360 inlined both profile
        // accessors (their "lpGuiCache" asserts are what survives at
        // BrnGuiOptionsDataProfile.h:611 and :632); restored as the real calls.
        GuiEventNetworkGameParams lParams;
        if (meOptionsToLoad == E_OPTIONS_TO_LOAD_SAVED)
        {
            lpOptionsData->GetCreatedOnlineGameOptions(0, mpGuiCache, &lParams);
        }
        else
        {
            lpOptionsData->GetReceivedOnlineGameOptions(0, mpGuiCache, &lParams);
        }

        mRouteInfoDisplay.SetInfo(miCurrentRound, &lParams);
    }
}

// FOLDED FROM BrnOnlineGameOptions_wI_09.cpp
// ===================================================================================
// BrnGui::OnlineGameOptions -- wave-I partfile 09: the two big handlers.
//   HandleGuiCacheEvent
//   HandleControllerInputCreateGame
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include <cstring>                                                       // memcpy
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (we are a friend)
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // BrnGui::OptionsDataProfile
#include <cstring>                                                       // memcpy / memset / strncpy
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiFlow

namespace BrnGui
{

//
// WHAT THE FUNCTION DOES
// ----------------------
// The create-match page's controller handler: a jump-table switch over the input action id
// (`r28 - 0x29`, 11 slots covering 0x29..0x33; 0x2D..0x30 fall into the default and are
// ignored). Two arms scroll the five-row option WINDOW, two move the highlighted row's
// VALUE, one accepts the page, one backs out, one opens the load-options page.
//
// ⭐⭐ MEASURED CORRECTION TO THE WAVE-I SPEC (spec §5 TRAP 2). The spec says the '+' / ','
// arms compare a ZERO-extended miHighlightedIndex against GetIndexFromId(0), so a -1 (no row
// highlighted) reads back as 255 and can never match a -1 index. THE CONSOLE SAYS OTHERWISE:
// both arms read the s8 SelectableGroup::miHighlightedIndex, SIGN-extend it, and compare it
// as a signed word against GetIndexFromId's s32, so -1 == -1 IS taken. Reproduced verbatim
// below by reading the s8 member straight into an s32; do NOT re-introduce a u8 cast.
// (The member's reach is +0x11A5 == 4517 == mCreateGameToggles(4352) +
// SelectableGroup::miHighlightedIndex(+0xA5) -- the offset corroborates the member exactly.)
//
// ⭐ VTABLE SLOTS MEASURED, NOT ASSUMED. The MenuToggleGroupVarSize<5> ctor stores its own
// dispatch table at +0x00, so the dispatch displacements resolve as +0x18 Clear,
// +0x28 SelectableGroup::HighlightNext, +0x2C
// SelectableGroup::HighlightPrevious, +0x30 SelectableGroup::HighlightIndex, +0x34
// HighlightNextItem, +0x38 HighlightPreviousItem. All are called by name below.
//
// NOTES TAKEN FROM THE CONSOLE'S OWN CODE, NOT A DECOMPILER'S PRINT-OUT
// --------------------------------------------------------------------
//  * A decompiler loses the live range of the miStartItem base register in BOTH scroll arms
//    and prints `*v20 = v19;` / `*v27 = v28 + 1;` with v20/v27 never assigned. The console
//    forms the address as this + 0xA50C == miStartItem in each arm.
//  * The five-call window rebuild (StoreCreateGameOptions / Clear / SetupGroup(5,false) /
//    SetupCommonCreateGameOptions / HighlightCreateGameOptions) appears FOUR times, written
//    out each time -- there is no `bl` to a shared helper and no such helper in the ledger,
//    so it is written out here too rather than inventing one.
//  * The arrow states come from KPC_ARROW_ANIMATION_STATES, whose entries are
//    [0] "invisible", [1] "visible", [2] "animate". Scrolling UP animates the
//    up arrow and leaves the down arrow merely visible; scrolling DOWN is the mirror; with
//    one option or fewer BOTH go invisible. The up arrow is written first, the down arrow
//    second, in both arms (this+0x7A90 then this+0x7B1C).
//  * The suspension event on the '2' arm is posted onto the out-queue directly rather than
//    through CgsGui::StateInterface::OutputGuiEvent, whose committed body passes the event id
//    (45) as the AddEvent channel where the console passes 40. Same accommodation the
//    group-03 partfile makes.
//  * The '1' arm copies mGameOptions OUT to the cache mirror (Dst = cache + 0xA800, Src =
//    this + 0xA0C0) -- the opposite direction to the copy HandleGuiCacheEvent makes.
//  * The '3' arm reads the two profile counters the console inlines as cache + 0x12B80 /
//    0x12B84; those are GetOptionsDataProfile() (+0xB878) plus miNumCreated/
//    ReceivedOnlineGameOptions (+0x7308/+0x730C), reached through the profile's accessors.
//  * No float compares anywhere in this body, so there is no NaN-polarity decision.
    namespace
    {
        // ---- AddEvent channel (the out-queue selector word) ---------------------------


        // ---- controller action ids (the in-queue payload's second word) ----------------
        // BrnGui's EGameInputActions values. The enum IS fully recovered
        // from the original GameSource/Input/GameInputActions.h -- it just
        // has no committed home under b5-decomp/src yet, which is why these stay s32. Same
        // names and values the group-02 partfile's TriggerSound uses.






        const s32 KI_ACTION_GUI_OPTION0 = 0x33;   // 51 GUI_OPTION0 open the saved-options page

        // ---- KPC_ARROW_ANIMATION_STATES indices (measured) -----------------------------
        const s32 KI_ARROW_STATE_INVISIBLE = 0;   // "invisible"
        const s32 KI_ARROW_STATE_VISIBLE   = 1;   // "visible"
        const s32 KI_ARROW_STATE_ANIMATE   = 2;   // "animate"

        // The apt view whose state the two arrow animators are driven through.


        // ---- in-queue payload view -----------------------------------------------------
        // The state in-queue hands handlers the HEADER-STRIPPED payload; this handler reads
        // only the payload's second word. Same view the sibling
        // screens carry for CgsGui::GuiEventControllerInput*.
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (the input action id)
        };
    }

    // ----------------------------------------------------- HandleControllerInputCreateGame
    void OnlineGameOptions::HandleControllerInputCreateGame(const CgsModule::Event* lpEvent)
    {
        // Non-fatal (BeginAssert / FireAssert / EndAssert, no early-out).
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineGameOptions::HandleControllerInputCreateGame");

        const ControllerButtonPayload* lpInput =
            reinterpret_cast<const ControllerButtonPayload*>(lpEvent);
        const s32 leAction = lpInput->miButtonId;

        switch (leAction)
        {
        // ---- scroll the five-row option window UP -------------------------------------
        case KI_ACTION_GUI_UP:
            {
                bool lbHandled = false;

                if (mCreateGameToggles.HighlightPrevious(false))
                {
                    // There was another row above: the highlight moved, nothing scrolls.
                    lbHandled = true;
                }
                else if (miStartItem > 0)
                {
                    // At the top of the window but not the top of the list: scroll one row.
                    lbHandled = true;
                    --miStartItem;

                    StoreCreateGameOptions();
                    mCreateGameToggles.Clear();
                    mCreateGameToggles.SetupGroup(KI_MAX_CREATE_GAME_OPTIONS, false);
                    SetupCommonCreateGameOptions();
                    HighlightCreateGameOptions();
                }
                else if (GetNumberOptions() > 1)
                {
                    // At the very top: wrap round to the last window and highlight its
                    // bottom row.
                    lbHandled = true;

                    const s32 liLastWindowStart = GetNumberOptions() - KI_MAX_CREATE_GAME_OPTIONS;
                    miStartItem = (liLastWindowStart > 0) ? liLastWindowStart : 0;

                    StoreCreateGameOptions();
                    mCreateGameToggles.Clear();
                    mCreateGameToggles.SetupGroup(KI_MAX_CREATE_GAME_OPTIONS, false);
                    SetupCommonCreateGameOptions();
                    HighlightCreateGameOptions();

                    s32 liVisibleRows = GetNumberOptions();
                    if (liVisibleRows >= KI_MAX_CREATE_GAME_OPTIONS)
                    {
                        liVisibleRows = KI_MAX_CREATE_GAME_OPTIONS;
                    }
                    mCreateGameToggles.HighlightIndex(liVisibleRows - 1);
                }

                if (lbHandled)
                {
                    // Scrolling up: the up arrow plays its "there is more above" animation.
                    if (GetNumberOptions() <= 1)
                    {
                        mUpArrowAnimator.AddOutputAptViewState(
                            "apt_Transition", KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_INVISIBLE], false);
                        mDownArrowAnimator.AddOutputAptViewState(
                            "apt_Transition", KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_INVISIBLE], false);
                    }
                    else
                    {
                        mUpArrowAnimator.AddOutputAptViewState(
                            "apt_Transition", KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_ANIMATE], false);
                        mDownArrowAnimator.AddOutputAptViewState(
                            "apt_Transition", KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_VISIBLE], false);
                    }

                    TriggerSound(leAction);
                }
            }
            break;

        // ---- scroll the five-row option window DOWN -----------------------------------
        case KI_ACTION_GUI_DOWN:
            {
                bool lbHandled = false;

                if (mCreateGameToggles.HighlightNext(false))
                {
                    lbHandled = true;
                }
                else
                {
                    const s32 liStartItem = miStartItem;

                    if (liStartItem + KI_MAX_CREATE_GAME_OPTIONS < GetNumberOptions())
                    {
                        // More list below the window: scroll one row and stay on the bottom.
                        lbHandled = true;
                        miStartItem = liStartItem + 1;

                        StoreCreateGameOptions();
                        mCreateGameToggles.Clear();
                        mCreateGameToggles.SetupGroup(KI_MAX_CREATE_GAME_OPTIONS, false);
                        SetupCommonCreateGameOptions();
                        HighlightCreateGameOptions();

                        s32 liVisibleRows = GetNumberOptions();
                        if (liVisibleRows >= KI_MAX_CREATE_GAME_OPTIONS)
                        {
                            liVisibleRows = KI_MAX_CREATE_GAME_OPTIONS;
                        }
                        mCreateGameToggles.HighlightIndex(liVisibleRows - 1);
                    }
                    else if (GetNumberOptions() > 1)
                    {
                        // At the very bottom: wrap round to the first window, top row.
                        lbHandled = true;
                        miStartItem = 0;

                        StoreCreateGameOptions();
                        mCreateGameToggles.Clear();
                        mCreateGameToggles.SetupGroup(KI_MAX_CREATE_GAME_OPTIONS, false);
                        SetupCommonCreateGameOptions();
                        HighlightCreateGameOptions();

                        mCreateGameToggles.HighlightIndex(0);
                    }
                }

                if (lbHandled)
                {
                    // Scrolling down: the DOWN arrow is the one that animates.
                    if (GetNumberOptions() <= 1)
                    {
                        mUpArrowAnimator.AddOutputAptViewState(
                            "apt_Transition", KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_INVISIBLE], false);
                        mDownArrowAnimator.AddOutputAptViewState(
                            "apt_Transition", KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_INVISIBLE], false);
                    }
                    else
                    {
                        mUpArrowAnimator.AddOutputAptViewState(
                            "apt_Transition", KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_VISIBLE], false);
                        mDownArrowAnimator.AddOutputAptViewState(
                            "apt_Transition", KPC_ARROW_ANIMATION_STATES[KI_ARROW_STATE_ANIMATE], false);
                    }

                    TriggerSound(leAction);
                }
            }
            break;

        // ---- previous value on the highlighted row ------------------------------------
        case KI_ACTION_GUI_LEFT:
            if (mCreateGameToggles.HighlightPreviousItem())
            {
                // Changing the GAME MODE row rebuilds the whole option set below it.
                // See the banner: the console SIGN-extends the s8 highlight, so a
                // -1 highlight does match a -1 game-mode row.
                const s32 liHighlightedRow = mCreateGameToggles.miHighlightedIndex;
                const s32 liGameModeRow = mCreateGameToggles.GetIndexFromId(
                    static_cast<u64>(static_cast<u32>(CreateMatchOptions::E_OPTION_GAME_MODE)));

                if (liHighlightedRow == liGameModeRow)
                {
                    StoreGameMode();
                    ResetGameOptions();
                    HighlightCreateGameOptions();
                    RequestPresetEvents();
                }

                TriggerSound(leAction);
            }
            break;

        // ---- next value on the highlighted row ----------------------------------------
        case KI_ACTION_GUI_RIGHT:
            if (mCreateGameToggles.HighlightNextItem())
            {
                const s32 liHighlightedRow = mCreateGameToggles.miHighlightedIndex;
                const s32 liGameModeRow = mCreateGameToggles.GetIndexFromId(
                    static_cast<u64>(static_cast<u32>(CreateMatchOptions::E_OPTION_GAME_MODE)));

                if (liHighlightedRow == liGameModeRow)
                {
                    StoreGameMode();
                    ResetGameOptions();
                    HighlightCreateGameOptions();
                    RequestPresetEvents();
                }

                TriggerSound(leAction);
            }
            break;

        // ---- accept the page ----------------------------------------------------------
        case KI_ACTION_GUI_SELECT:
            StoreCreateGameOptions();

            // Publish the edited options back into the cache's mirror and move on.
            memcpy(reinterpret_cast<GuiEventNetworkGameParams*>(
                       &mpGuiCache->maOnlineGameModeOptionsStorage[0]),
                   &mGameOptions,
                   sizeof(mGameOptions));   // size 0x1E0 == the whole cache mirror

            TriggerSound(leAction);
            SendStateEvent("ADVANCE");
            break;

        // ---- back out of the page ------------------------------------------------------
        case KI_ACTION_GUI_CANCEL:
            {
                const char* lpacStateEvent;

                if (mpGuiCache->mbOnlineStartPending)   // GuiCache +0x4B53
                {
                    // This screen armed the online start: lift the network suspension it
                    // put in place, then back out on the quiet path.
                    CgsGui::GuiEventNetworkSuspension lNetworkSuspension;
                    lNetworkSuspension.meSuspensionType = CgsGui::GuiEventNetworkSuspension::E_SUSPENSION_TYPE_SUSPEND;
                    mpStateInterface->OutputGuiEvent(lNetworkSuspension);   // record size 16

                    mpGuiCache->mbOnlineStartPending = false;
                    lpacStateEvent = "GO_BACK_EASY";
                }
                else
                {
                    lpacStateEvent = "GO_BACK";
                }

                SendStateEvent(lpacStateEvent);
                TriggerSound(leAction);
            }
            break;

        // ---- open the saved / recent options page --------------------------------------
        case KI_ACTION_GUI_OPTION0:
            {
                // Only worth opening when the profile actually holds a saved or a received
                // set of options.
                OptionsDataProfile* lpProfile = mpGuiCache->GetMenuOptionsData();
                if (lpProfile->GetNumCreatedOnlineGameOptions() > 0 ||
                    lpProfile->GetNumReceivedOnlineGameOptions() > 0)
                {
                    ShowLoadScreen();
                }
            }
            break;

        default:
            // 0x2D..0x30 land here through the jump table; every other id misses it.
            break;
        }
    }

//
// WHAT THE FUNCTION DOES
// ----------------------
// The screen's GuiCache-arrival sink, and its one-shot page setup. The GUI module
// broadcasts the cache pointer to every state; this screen acts only on the FIRST one
// (`if (!v3[10560])` == mpGuiCache still null -- everything below sits inside that guard).
// On that first arrival it: latches the cache, publishes the online ticker line, and then
// either registers every apt component the page waits on (when the player is allowed to
// play multiplayer) or backs straight out again (when they are not). Either way it then
// pulls the cache's 480-byte network-game-params mirror into its own editable copy and, if
// the options changed while this screen was away -- or the lobby is a free-burn lobby --
// publishes the id-409 refresh record and clears the changed flag. Finally it asks for the
// preset events that populate the option rows.
//
// NOTES TAKEN FROM THE ASM RATHER THAN HEX-RAYS
// ---------------------------------------------
//  * The ticker payload is built in a stack scratch at sp+0x80 and memcpy'd into the record
//    payload at sp+0x8AC; built directly in the record here (the CarSelectVehicle_Input
//    precedent), which is the same bytes on the wire.
//  * The payload seeds differ from CarSelectVehicle::SetTicker's. Measured (the offsets
//    are payload-relative):
//        +0x810 = 0   mi8NumStrings
//        +0x811 = 1   maFlags[0]      <-- SetTicker leaves this 0
//        +0x812 = 0   maFlags[1]
//        +0x813 = 1   maFlags[2]      <-- the seed SetTicker also sets
//        +0x814 = 0   maFlags[3]
//    with maiStringTypes (+0x00..+0x0F) zeroed by two wide stores and the 0x800-byte string
//    block zeroed by the memset. A whole-struct memset plus the two flag stores is identical.
//  * The six name-registrations pass (cache, 0, component + 4). +4 is
//    CgsGui::GuiComponent::macName, i.e. the component's GetName(); and the callee is the
//    name-taking entry of GuiCache::AppendExpectedAptComponent already declared in
//    BrnGuiCache.h. Their order is up / down / load-header ANIM / load-header TEXT /
//    title TEXT / map-border (this+0x7A94, 0x7B20, 0x7BAC, 0x7CC4, 0x7DEC, 0x7C38) -- the
//    map border is registered LAST, out of declaration order; kept as the console has it.
//  * The suspension event is posted onto the out-queue directly rather than through
//    CgsGui::StateInterface::OutputGuiEvent, whose committed body passes the event id (45)
//    as the AddEvent channel where the console passes 40. Same accommodation the group-03
//    partfile makes.
//  * The id-409 record's two payload bytes are built as a halfword in a scratch slot and
//    stored as one halfword; modelled as the two bytes they are.
//    Id 409 has no declared event type anywhere in the recovered declarations, so the record
//    is a file-local wire with a FLAG role-name taken from what the call site does with it.
    namespace
    {
        // ---- AddEvent channel (the out-queue selector word) ---------------------------

        // The ticker string's format/kind selector (the console passes the constant 2) -- the
        // same word BrnGui::CarSelectVehicle::SetTicker passes to AddString.
        const s32 KI_TICKER_STRING_TYPE = 2;

        // ---- the three ticker lines (rodata literals) ----------------------------------

        // ---- in-queue payload view -----------------------------------------------------
        // The state in-queue hands handlers the HEADER-STRIPPED payload, so the incoming
        // cache pointer is the payload's first word (read twice by the console). The
        // original types the parameter const GuiEventCache*, whose
        // home header hard-collides with BrnGuiEventTypeDefs.h -- the same file-local view
        // the wave-H twin (BrnOnlineGameRoomPlayerInfo_wH_18.cpp) carries.
        struct GuiEventCachePayload : public CgsModule::Event
        {
            GuiCache* mpGuiCache;   // +0x00
        };

        // ---- out-queue wire records ----------------------------------------------------

        // The custom ticker message payload (0x818 bytes). Layout recovered store-for-store
        // from BrnGui::GuiEventTickerCustomMessage::AddString, whose asserts name
        // "GameSource/Gui/BrnGuiEventTypeDefs.h" as the type's home:
        //   +0x000  s32  maiStringTypes[4]
        //   +0x010  char maacStrings[4][512]
        //   +0x810  s8   mi8NumStrings          (read as a SIGNED byte, bounded < 4)
        //   +0x811..+0x814  four flag bytes
        // Kept TU-LOCAL rather than promoted into BrnGuiEventTypeDefs.h: the type already has
        // an opaque twin in BrnGuiDemangledEventTypes.h (GuiEvent<537> + a 2060-byte blob) and
        // the two headers are mutually exclusive by construction, so a second definition would
        // be a live ODR fork. Identical to the view BrnCarSelectVehicle_Input.cpp carries.
        struct GuiTickerCustomMessagePayload
        {
            static const s32 KI_MAX_NUM_STRINGS   = 4;     // AddString's bound
            static const s32 KI_MAX_STRING_LENGTH = 512;   // AddString's strncpy count

            s32  maiStringTypes[KI_MAX_NUM_STRINGS];                       // +0x000
            char maacStrings[KI_MAX_NUM_STRINGS][KI_MAX_STRING_LENGTH];    // +0x010
            s8   mi8NumStrings;                                            // +0x810
            // FLAG: four flag bytes at +0x811..+0x814 whose roles are not recovered. This
            // producer seeds them { 1, 0, 1, 0 }; CarSelectVehicle::SetTicker seeds
            // { 0, 0, 1, 0 }, so flag 0 is what distinguishes the two ticker kinds.
            u8   maFlags[4];                                               // +0x811
            u8   maPad815[3];                                              // +0x815 (sizeof == 0x818)

            // Copy lpString into the next free 512-byte slot and record its format type.
            // The count is read as a SIGNED byte.
            void AddString(const char* lpString, s32 liType)
            {
                CGS_ASSERT(mi8NumStrings >= 0, "mi8NumStrings >= 0");
                CGS_ASSERT(mi8NumStrings < KI_MAX_NUM_STRINGS,
                           "mi8NumStrings < KI_MAX_NUM_STRINGS");
                CGS_ASSERT(lpString != 0, "lpString");                                  // h:392

                std::strncpy(maacStrings[mi8NumStrings], lpString,
                             static_cast<size_t>(KI_MAX_STRING_LENGTH));
                maiStringTypes[mi8NumStrings] = liType;
                ++mi8NumStrings;
            }
        };

        // { 0x818, 537, 12, <the message> }, channel 40, 0x824 bytes -- all three written as
        // host expressions.
        struct GuiTickerCustomMessageWire : public CgsGui::GuiEvent<537>
        {
            GuiTickerCustomMessagePayload mMessage;   // +0x0C

            GuiTickerCustomMessageWire()
            {
                std::memset(&mMessage, 0, sizeof(mMessage));
                mMessage.maFlags[0] = 1;   // +0x811 (this producer's distinguishing seed)
                mMessage.maFlags[2] = 1;   // +0x813
            }
        };

        // Id 409 -- the "the online game options changed, re-publish them" refresh record.
        // FLAG: id 409 carries no declared event struct anywhere in the recovered
        // declarations, so the payload shape and the role-name both come from this single
        // call site: the console stack-builds { 2, 409, 12 } + the two payload bytes { 1, 0 }
        // and publishes 16 bytes on channel 40. The two flag names are
        // deliberately role-free -- nothing in the binary names them.
        struct GuiEventOnlineGameOptionsRefresh
        {
            u8 mu8FlagA;   // +0x00 (set)
            u8 mu8FlagB;   // +0x01 (clear)
        };

        typedef CgsGui::GuiEvent<409> OnlineGameOptionsRefreshHeader;

        struct GuiEventOnlineGameOptionsRefreshWire : public OnlineGameOptionsRefreshHeader
        {
            GuiEventOnlineGameOptionsRefresh mRefresh;   // +0x0C

            GuiEventOnlineGameOptionsRefreshWire()
            {
                mRefresh.mu8FlagA = 1;
                mRefresh.mu8FlagB = 0;
            }
        };

        // Layout pins for the two records above: the console posts 0x824 and 0x10 bytes with
        // payload-size words 0x818 and 2 and payload-offset words 12 and 12.
        typedef char KAC_ASSERT_TICKER_PAYLOAD_SIZE[
            sizeof(GuiTickerCustomMessagePayload) == 0x818 ? 1 : -1];
        typedef char KAC_ASSERT_TICKER_WIRE_SIZE[
            sizeof(CgsGui::GuiEventOut<GuiTickerCustomMessageWire>) == 0x824 ? 1 : -1];
        typedef char KAC_ASSERT_REFRESH_PAYLOAD_SIZE[
            sizeof(GuiEventOnlineGameOptionsRefresh) == 2 ? 1 : -1];
        typedef char KAC_ASSERT_REFRESH_WIRE_SIZE[
            sizeof(CgsGui::GuiEventOut<GuiEventOnlineGameOptionsRefreshWire>) == 16 ? 1 : -1];
    }

    // ------------------------------------------------------------------ HandleGuiCacheEvent
    void OnlineGameOptions::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiEventCachePayload* lpCacheEvent =
            reinterpret_cast<const GuiEventCachePayload*>(lpEvent);

        // Non-fatal (BeginAssert / FireAssert / EndAssert, no early-out) -- the console falls
        // straight through into the body, so a null cache would be latched as-is.
        CGS_ASSERT(lpCacheEvent->mpGuiCache != 0,
                   "Invalid cache in HandleGuiCacheEvent::Update");

        // ---- first arrival only: the whole page setup is a one-shot -------------------
        if (mpGuiCache != 0)
        {
            return;
        }

        mpGuiCache = lpCacheEvent->mpGuiCache;

        // ---- the ticker line ----------------------------------------------------------
        // Which of the three online blurbs runs along the ticker depends on how the match
        // was entered: ranked, free-burn, or plain unranked.
        {
            const char* lpacTickerText;
            if (mpGuiCache->mbOnlineMatchRanked)             // GuiCache +0x4B51
            {
                lpacTickerText = "ONLINE_RANKED_TICKER_TEXT";
            }
            else if (mpGuiCache->mbOnlineMatchUnranked)      // GuiCache +0x4B52
            {
                lpacTickerText = "ONLINE_FREEBURN_TICKER_TEXT";
            }
            else
            {
                lpacTickerText = "ONLINE_UNRANKED_TICKER_TEXT";
            }

            GuiTickerCustomMessageWire lTicker;
            lTicker.mMessage.AddString(lpacTickerText, KI_TICKER_STRING_TYPE);

            mpStateInterface->OutputGuiEvent(lTicker);   // record size 0x824
        }

        // ---- either arm the page, or back straight out --------------------------------
        if (CheckPrivileges())
        {
            // Tell the cache which apt components this page has to wait on before it can
            // report the screen flow ready. The four components with their own helper go
            // through it; the four animators and two text fields are registered by name.
            mMenuOptions.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
            mCreateGameToggles.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, true);
            mRouteInfoDisplay.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
            mHelpBar.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);

            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mUpArrowAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mDownArrowAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mLoadHeaderAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mLoadHeaderText.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTitleText.GetName());
            // The map border comes LAST on the console, out of declaration order.
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMapBorderAnimator.GetName());
        }
        else if (mpGuiCache->mbOnlineStartPending)          // GuiCache +0x4B53
        {
            // No multiplayer privilege, and this screen owns the pending online start:
            // lift the network suspension it armed, then back out on the quiet path.
            CgsGui::GuiEventNetworkSuspension lNetworkSuspension;
            lNetworkSuspension.meSuspensionType = CgsGui::GuiEventNetworkSuspension::E_SUSPENSION_TYPE_SUSPEND;
            mpStateInterface->OutputGuiEvent(lNetworkSuspension);   // record size 16

            mpGuiCache->mbOnlineStartPending = false;
            SendStateEvent("GO_BACK_EASY");
        }
        else
        {
            SendStateEvent("GO_BACK");
        }

        // ---- take a working copy of the cache's params mirror -------------------------
        memcpy(&mGameOptions,
               reinterpret_cast<const GuiEventNetworkGameParams*>(
                   &mpGuiCache->maOnlineGameModeOptionsStorage[0]),
               sizeof(mGameOptions));   // size 0x1E0 == the whole cache mirror

        // ---- ask for a fresh publish when the options went stale ----------------------
        // Either something changed the options while this screen was away, or the lobby is
        // a free-burn lobby (whose options this page always re-publishes on entry).
        if (mpGuiCache->mbOnlineGameOptionsChanged ||          // GuiCache +0xA9E0
            mGameOptions.meGameMode ==
                BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY)
        {
            GuiEventOnlineGameOptionsRefreshWire lRefresh;
            mpStateInterface->OutputGuiEvent(lRefresh);   // record size 16

            mpGuiCache->mbOnlineGameOptionsChanged = false;
        }

        // Populate the option rows from the preset the current mode selects.
        RequestPresetEvents();
    }
}

// FOLDED FROM BrnOnlineGameOptions_wS34_00.cpp
// ===================================================================================
// BrnGui::OnlineGameOptions -- stub-wave partfile 00: the lifecycle pair, the option
// table, the load-list input and the request/response handlers.
//   BuildGameOptions
//   ResetGameOptions
//   GetHighlightedOption
//   HandleCarInfoResponseEvent
//   OnLeave
//   RequestPresetEvents
//   HandleControllerInputLoadOptions
//   Update
// Reconstructed from the console image; the assembly arbitrates over the decompiler.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include <cstddef>                                                        // offsetof
#include "GameShared/GameClasses/Containers/CgsArray.h"                   // Array<CgsID, N>
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GameStateModuleIO::EGameModeType
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // OptionsDataProfile
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // WorldDataController::GetVehicleList
#include "GameSource/Input/GameInputActions.h"                            // E_GAMEINPUTACTIONS_GUI_*
#include "GameSource/Resource/BrnDLCManager.h"                            // g_DLCFeatureAvailability
#include "SharedClasses/DataLists/VehicleList.h"                          // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                     // VehicleListEntry::GetUnlockRank

namespace BrnGui
{
    namespace
    {
        namespace GSM = BrnGameState::GameStateModuleIO;

        // The state in-queue concrete type (GuiEventQueue is its pointer-only face).
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // Out-queue channels: GUI events, view state, internal state.
        // KI_CHANNEL_GUI_OUT is declared above in this translation unit.

        const s32 KI_CHANNEL_INTERNAL_STATE = 42;

        // The in-queue event ids Update routes.
        const s32 KI_EVENT_CONTROLLER        = 6;
        const s32 KI_EVENT_DISCONNECTED      = 44;
        const s32 KI_EVENT_IN_GAME           = 50;
        const s32 KI_EVENT_IN_GAME_FAILED    = 51;
        const s32 KI_EVENT_GUI_CACHE         = 64;
        const s32 KI_EVENT_OVERLAY_COMPLETE  = 189;
        const s32 KI_EVENT_LOBBY_PLAYER_LIST = 244;
        const s32 KI_EVENT_CAR_INFO_RESPONSE = 412;

        // The apt movie the screen mounts, and the level it is cleared on.
        // KI_APT_MOVIE_LEVEL is declared above in this translation unit.

        // The DLC feature that unlocks the stunt game modes (no recovered name).
        const s32 KI_DLC_FEATURE_ONLINE_STUNT_MODES = 19;   // FLAG name

        // The load list's arrow states (KPC_ARROW_ANIMATION_STATES indices).
        const s32 KI_ARROW_INVISIBLE = 0;
        const s32 KI_ARROW_VISIBLE   = 1;
        const s32 KI_ARROW_ANIMATE   = 2;



        // Controller event payload: the action id rides in the second word.
        struct OptionsButtonPayload : public CgsModule::Event
        {
            s32 miDevice;     // +0x00
            s32 miButtonId;   // +0x04
        };

        // Event 244 payload view: the lobby roster rows, then the live player count.
        struct GuiEventNetworkLobbyPlayerListPayload : public CgsModule::Event
        {
            u8  maRows[0x1C0];   // +0x000 (eight lobby rows, not read here)
            s32 miNumPlayers;    // +0x1C0
        };

        // Event 412 payload view: the car ids of the lobby's players.
        struct GuiEventCarInfoResponsePayload : public CgsModule::Event
        {
            CgsContainers::Array<CgsID, 128> maCarIds;   // +0x000 (count at +0x400)
        };

        // { 4, 193, 12, <list> }, channel 40, 16 bytes: ask for a preset-race list.
        struct GuiEventRequestSpecificPreSetRacesWire : public CgsGui::GuiEvent<193>
        {
            s32 miPresetRaceType;   // +0x0C

            explicit GuiEventRequestSpecificPreSetRacesWire(s32 liPresetRaceType)
                : miPresetRaceType(liPresetRaceType)
            {
            }
            GuiEventRequestSpecificPreSetRacesWire() {}
        };

        // { 2, 536, 12, <two zero bytes> }, channel 40, 16 bytes: clear the news ticker.
        struct GuiEventTickerClearMessagesWire : public CgsGui::GuiEvent<536>
        {
            u8 maPayload[2];   // +0x0C

            GuiEventTickerClearMessagesWire()
            {
                maPayload[0] = 0;
                maPayload[1] = 0;
            }
        };

        // { 12, 213, 12, map type, fade time, show }, 24 bytes, posted on the view-state and
        // the internal-state channels: show or hide the sat-nav.
        struct GuiEventShowHideSatNavWire : public CgsGui::GuiEvent<213>
        {
            s32 meMapType;    // +0x0C (0 == the main map)
            f32 mfFadeTime;   // +0x10
            u8  mbShow;       // +0x14
            u8  maPad[3];

            GuiEventShowHideSatNavWire(s32 leMapType, f32 lfFadeTime, bool lbShow)
                : meMapType(leMapType)
                , mfFadeTime(lfFadeTime)
                , mbShow(lbShow ? 1 : 0)
            {
                maPad[0] = maPad[1] = maPad[2] = 0;
            }
            GuiEventShowHideSatNavWire() {}
        };
    }

    // ================================================================================
    //  BuildGameOptions
    //
    //  The create-match option table: one {title} row per option group, its value rows,
    //  and an E_OPTION_TERMINATOR row after each group. The stunt modes are only offered
    //  when their DLC feature is enabled.
    // ================================================================================
    void OnlineGameOptions::BuildGameOptions()
    {
        typedef CreateMatchOptions O;

        // The console clears the table and appends the two game-mode rows twice.
        for (s32 liPass = 0; liPass < 2; ++liPass)
        {
            maOptions.Clear();
            const CreateMatchOptions lMode     = { "$ONLINE_GAME_OPTION_MODE",      O::E_OPTION_GAME_MODE };
            const CreateMatchOptions lModeRace = { "$ONLINE_GAME_OPTION_MODE_RACE", O::E_OPTION_GAME_MODE_RACE };
            maOptions.Append(lMode);
            maOptions.Append(lModeRace);
        }

        if (BrnResource::g_DLCFeatureAvailability.IsFeatureEnabled(KI_DLC_FEATURE_ONLINE_STUNT_MODES))
        {
            const CreateMatchOptions laStuntModes[] =
            {
                { "$ONLINE_GAME_OPTION_MODE_STUNT",              O::E_OPTION_GAME_MODE_STUNT },
                { "$ONLINE_GAME_OPTION_MODE_STUNT_FREE_FOR_ALL", O::E_OPTION_GAME_MODE_STUNT_FREE_FOR_ALL },
                { "$ONLINE_GAME_OPTION_MODE_STUNT_COOP",         O::E_OPTION_GAME_MODE_STUNT_COOP },
            };
            for (u32 luRow = 0; luRow < sizeof(laStuntModes) / sizeof(laStuntModes[0]); ++luRow)
                maOptions.Append(laStuntModes[luRow]);
        }

        const CreateMatchOptions laOptions[] =
        {
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_VEHICLE_CHOICE",      O::E_OPTION_VEHICLE_CHOICE },
            { "$ONLINE_GAME_OPTION_VEHICLE_CHOICE_FREE", O::E_OPTION_VEHICLE_CHOICE_FREE },
            { "$ONLINE_GAME_OPTION_VEHICLE_CHOICE_HOST", O::E_OPTION_VEHICLE_CHOICE_HOST },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_VEHICLE_CLASS",   O::E_OPTION_VEHICLE_CLASS },
            { "$ONLINE_GAME_OPTION_VEHICLE_CLASS_1", O::E_OPTION_VEHICLE_CLASS_1 },
            { "$ONLINE_GAME_OPTION_VEHICLE_CLASS_5", O::E_OPTION_VEHICLE_CLASS_2 },
            { "$ONLINE_GAME_OPTION_VEHICLE_CLASS_4", O::E_OPTION_VEHICLE_CLASS_3 },
            { "$ONLINE_GAME_OPTION_VEHICLE_CLASS_3", O::E_OPTION_VEHICLE_CLASS_4 },
            { "$ONLINE_GAME_OPTION_VEHICLE_CLASS_2", O::E_OPTION_VEHICLE_CLASS_5 },
            { "$ONLINE_GAME_OPTION_VEHICLE_CLASS_6", O::E_OPTION_VEHICLE_CLASS_6 },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_ROUNDS", O::E_OPTION_ROUNDS },
            { "1", O::E_OPTION_ROUNDS_1 },
            { "2", O::E_OPTION_ROUNDS_2 },
            { "3", O::E_OPTION_ROUNDS_3 },
            { "4", O::E_OPTION_ROUNDS_4 },
            { "5", O::E_OPTION_ROUNDS_5 },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_ROUNDS", O::E_OPTION_ROUNDS_EVEN },
            { "2",  O::E_OPTION_ROUNDS_2 },
            { "4",  O::E_OPTION_ROUNDS_4 },
            { "6",  O::E_OPTION_ROUNDS_6 },
            { "8",  O::E_OPTION_ROUNDS_8 },
            { "10", O::E_OPTION_ROUNDS_10 },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_RED_TEAM",       O::E_OPTION_INFINITE_BOOST },
            { "$ONLINE_GAME_OPTION_INFINITE_BOOST", O::E_OPTION_INFINITE_BOOST_ON },
            { "$ONLINE_GAME_OPTION_NORMAL_BOOST",   O::E_OPTION_INFINITE_BOOST_OFF },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_TRAFFIC", O::E_OPTION_TRAFFIC },
            { "$GENERAL_OPTION_ON",          O::E_OPTION_TRAFFIC_ON },
            { "$GENERAL_OPTION_OFF",         O::E_OPTION_TRAFFIC_OFF },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_TRAF_CHK", O::E_OPTION_TRAFFIC_CHECKING },
            { "$GENERAL_OPTION_ON",           O::E_OPTION_TRAFFIC_CHECKING_ON },
            { "$GENERAL_OPTION_OFF",          O::E_OPTION_TRAFFIC_CHECKING_OFF },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_BOOST",    O::E_OPTION_BOOST_TYPE },
            { "$ONLINE_GAME_OPTION_BOOST_B1", O::E_OPTION_BOOST_TYPE_NORMAL },
            { "$ONLINE_GAME_OPTION_BOOST_B2", O::E_OPTION_BOOST_TYPE_DANAGER },
            { "$ONLINE_GAME_OPTION_BOOST_B3", O::E_OPTION_BOOST_TYPE_AGGRESSION },
            { "$ONLINE_GAME_OPTION_BOOST_B4", O::E_OPTION_BOOST_TYPE_STUNT },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_CR_LIMIT",   O::E_OPTION_RUNNER_CRASH_LIMIT },
            { "$ONLINE_GAME_OPTION_CR_LIMIT_1", O::E_OPTION_RUNNER_CRASH_LIMIT_1 },
            { "$ONLINE_GAME_OPTION_CR_LIMIT_2", O::E_OPTION_RUNNER_CRASH_LIMIT_2 },
            { "$ONLINE_GAME_OPTION_CR_LIMIT_3", O::E_OPTION_RUNNER_CRASH_LIMIT_3 },
            { "$ONLINE_GAME_OPTION_CR_LIMIT_4", O::E_OPTION_RUNNER_CRASH_LIMIT_4 },
            { "$ONLINE_GAME_OPTION_CR_LIMIT_5", O::E_OPTION_RUNNER_CRASH_LIMIT_5 },
            { "$ONLINE_GAME_OPTION_CR_LIMIT_N", O::E_OPTION_RUNNER_CRASH_LIMIT_NEVER },
            { "", O::E_OPTION_TERMINATOR },
            { "$ONLINE_GAME_OPTION_TIME_LIMIT",    O::E_OPTION_TIME_LIMIT },
            { "$ONLINE_GAME_OPTION_TIME_LIMIT_10", O::E_OPTION_TIME_LIMIT_10 },
            { "$ONLINE_GAME_OPTION_TIME_LIMIT_20", O::E_OPTION_TIME_LIMIT_20 },
            { "$ONLINE_GAME_OPTION_TIME_LIMIT_30", O::E_OPTION_TIME_LIMIT_30 },
            { "", O::E_OPTION_TERMINATOR },
        };
        for (u32 luRow = 0; luRow < sizeof(laOptions) / sizeof(laOptions[0]); ++luRow)
            maOptions.Append(laOptions[luRow]);
    }

    // ================================================================================
    //  ResetGameOptions
    //
    //  Back to the default match parameters, keeping the chosen mode and vehicle class.
    //  Road rage starts on two rounds.
    // ================================================================================
    void OnlineGameOptions::ResetGameOptions()
    {
        const s32 leGameMode     = mGameOptions.meGameMode;
        const s32 liVehicleClass = mGameOptions.miVehicleClass;

        mGameOptions.Construct();
        mGameOptions.meGameMode     = leGameMode;
        mGameOptions.miVehicleClass = liVehicleClass;
        mGameOptions.miTimeLimit    = 0;

        if (leGameMode == GSM::E_MODE_ONLINE_ROAD_RAGE)
            mGameOptions.miNumRounds = 2;
        else if (leGameMode == GSM::E_MODE_ONLINE_BURNING_HOME_RUN)
            mGameOptions.miTimeLimit = 0;
    }

    // ================================================================================
    //  GetHighlightedOption
    //
    //  The value the toggle row for leOption currently shows.
    // ================================================================================
    CreateMatchOptions::EOption OnlineGameOptions::GetHighlightedOption(CreateMatchOptions::EOption leOption)
    {
        const s32 liToggle = mCreateGameToggles.GetIndexFromId(static_cast<u64>(static_cast<s64>(leOption)));
        CGS_ASSERT(liToggle >= 0, "Required toggle not found!");

        return static_cast<CreateMatchOptions::EOption>(
            mCreateGameToggles.GetSelectable(liToggle)->GetHighlightedId());
    }

    // ================================================================================
    //  HandleCarInfoResponseEvent
    //
    //  The lobby's cars are known: reset the match parameters (keeping the security
    //  setting) and set the vehicle class to the highest unlock rank among them.
    // ================================================================================
    void OnlineGameOptions::HandleCarInfoResponseEvent(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        CGS_ASSERT(mpGuiCache->GetWorldDataController() != 0, "mpWorldDataController");
        const BrnResource::VehicleList* lpVehicleList =
            mpGuiCache->GetWorldDataController()->GetVehicleList();
        CGS_ASSERT(lpVehicleList != 0, "No vehicle list\n");

        const GuiEventCarInfoResponsePayload* lpResponse =
            static_cast<const GuiEventCarInfoResponsePayload*>(lpEvent);

        s32 liHighestRank = 0;
        for (u32 luCar = 0; luCar < lpResponse->maCarIds.GetLength(); ++luCar)
        {
            const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lpResponse->maCarIds.GetItem(luCar));
            const BrnResource::VehicleListEntry* lpVehicleData =
                (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);
            // The console streams the car id after both texts.
            CGS_ASSERT(lpVehicleData != 0, "No vehicle data for ");

            const s32 liRank = lpVehicleData->GetUnlockRank();
            if (!(liHighestRank > liRank))
                liHighestRank = liRank;
        }

        const s32 leSecurity = mGameOptions.meSecurity;
        mGameOptions.Construct();
        mGameOptions.meSecurity     = leSecurity;
        mGameOptions.miVehicleClass = liHighestRank;
    }

    // ================================================================================
    //  OnLeave
    // ================================================================================
    void OnlineGameOptions::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The console inlines StateInterface::PlayAptMovie: clear the screen's apt level.
        mpStateInterface->PlayAptMovie("", KI_APT_MOVIE_LEVEL);

        mMenuOptions.Clear();
        mCreateGameToggles.Clear();
        mRouteInfoDisplay.Destruct();

        GuiEventTickerClearMessagesWire lClearTicker;
        mpStateInterface->OutputGuiEvent(lClearTicker);

        // Hide the sat-nav: one record on the view-state and the internal-state channels.
        GuiEventShowHideSatNavWire lHideSatNav(0, 0.0f, false);
        mpStateInterface->OutputViewState(lHideSatNav);
        mpStateInterface->OutputInternalState(lHideSatNav);

        mCreateGameToggles.Unloaded();
        mHelpBar.Destruct();
    }

    // ================================================================================
    //  RequestPresetEvents
    //
    //  Ask the network module for the preset-race list that fits the chosen mode.
    // ================================================================================
    void OnlineGameOptions::RequestPresetEvents()
    {
        s32 liPresetRaceType;
        switch (mGameOptions.meGameMode)
        {
            case GSM::E_MODE_ONLINE_ROAD_RAGE:
                liPresetRaceType = GSM::E_MODE_ONLINE_RACE;
                break;
            case GSM::E_MODE_ONLINE_BURNING_HOME_RUN:
                liPresetRaceType = GSM::E_MODE_ONLINE_BURNING_HOME_RUN;
                break;
            case GSM::E_MODE_ONLINE_FUGITIVE:
            case GSM::E_MODE_ONLINE_FREE_BURN:
            case GSM::E_MODE_ONLINE_MODE_END:
                liPresetRaceType = GSM::E_MODE_STUNT_ATTACK;
                break;
            default:
                liPresetRaceType = GSM::E_MODE_ONLINE_RACE;
                break;
        }

        GuiEventRequestSpecificPreSetRacesWire lRequest(liPresetRaceType);
        mpStateInterface->OutputGuiEvent(lRequest);
    }

    // ================================================================================
    //  HandleControllerInputLoadOptions
    //
    //  The load page: up/down scroll the six visible slots over the saved (created) or
    //  the recent (received) list, left/right step through the highlighted slot's rounds,
    //  select loads the slot, cancel goes back, and the shoulders swap the two lists.
    // ================================================================================
    void OnlineGameOptions::HandleControllerInputLoadOptions(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event sent to OnlineGameOptions::HandleControllerInputLoadOptions");

        const s32 leAction = static_cast<const OptionsButtonPayload*>(lpEvent)->miButtonId;
        s32 liOption = mMenuOptions.GetHighlightedIndex() + miStartItem;

        OptionsDataProfile* lpProfile = mpGuiCache->GetMenuOptionsData();
        GuiEventNetworkGameParams lParams;

        switch (leAction)
        {
            case E_GAMEINPUTACTIONS_GUI_UP:
            case E_GAMEINPUTACTIONS_GUI_DOWN:
            {
                bool lbChanged = false;
                s32 liNumOptions;
                if (meOptionsToLoad == E_OPTIONS_TO_LOAD_SAVED)
                {
                    liNumOptions = lpProfile->GetNumCreatedOnlineGameOptions();
                }
                else
                {
                    CGS_ASSERT(meOptionsToLoad == E_OPTIONS_TO_LOAD_RECENT,
                               "meOptionsToLoad == E_OPTIONS_TO_LOAD_RECENT");
                    liNumOptions = lpProfile->GetNumReceivedOnlineGameOptions();
                }

                if (leAction == E_GAMEINPUTACTIONS_GUI_UP)
                {
                    if (mMenuOptions.HighlightPrevious())
                    {
                        lbChanged      = true;
                        miCurrentRound = 0;
                        liOption       = mMenuOptions.GetHighlightedIndex() + miStartItem;
                    }
                    else if (miStartItem > 0)
                    {
                        // Scroll the window up one slot.
                        --miStartItem;
                        lbChanged = true;
                        liOption  = mMenuOptions.GetHighlightedIndex() + miStartItem;
                        ShowLoadOptions();
                    }
                    else if (liNumOptions > 1)
                    {
                        // Wrap to the bottom of the list.
                        miStartItem = liNumOptions - KI_MAX_LOAD_OPTIONS;
                        if (miStartItem < 0)
                            miStartItem = 0;
                        liOption  = liNumOptions - 1;
                        lbChanged = true;
                        ShowLoadOptions();
                        mMenuOptions.HighlightIndex(liNumOptions - 1 - miStartItem);
                    }
                }
                else
                {
                    if (mMenuOptions.HighlightNext())
                    {
                        lbChanged      = true;
                        miCurrentRound = 0;
                        liOption       = mMenuOptions.GetHighlightedIndex() + miStartItem;
                    }
                    else if (miStartItem + KI_MAX_LOAD_OPTIONS >= liNumOptions)
                    {
                        // Wrap to the top of the list.
                        if (liNumOptions > 1)
                        {
                            miStartItem = 0;
                            lbChanged   = true;
                            liOption    = 0;
                            ShowLoadOptions();
                            mMenuOptions.HighlightIndex(0);
                        }
                    }
                    else
                    {
                        // Scroll the window down one slot.
                        ++miStartItem;
                        lbChanged = true;
                        liOption  = mMenuOptions.GetHighlightedIndex() + miStartItem;
                        ShowLoadOptions();
                    }
                }

                if (!lbChanged)
                    break;

                TriggerSound(leAction);
                if (meOptionsToLoad != E_OPTIONS_TO_LOAD_SAVED)
                    lpProfile->GetReceivedOnlineGameOptions(liOption, mpGuiCache, &lParams);
                else
                    lpProfile->GetCreatedOnlineGameOptions(liOption, mpGuiCache, &lParams);
                mRouteInfoDisplay.SetInfo(miCurrentRound, &lParams);

                // The arrow on the side the list moved animates; both hide for a single slot.
                const char* lpacUpState;
                const char* lpacDownState;
                if (liNumOptions > 1)
                {
                    const bool lbUp = (leAction == E_GAMEINPUTACTIONS_GUI_UP);
                    lpacUpState   = KPC_ARROW_ANIMATION_STATES[lbUp ? KI_ARROW_ANIMATE : KI_ARROW_VISIBLE];
                    lpacDownState = KPC_ARROW_ANIMATION_STATES[lbUp ? KI_ARROW_VISIBLE : KI_ARROW_ANIMATE];
                }
                else
                {
                    lpacUpState   = KPC_ARROW_ANIMATION_STATES[KI_ARROW_INVISIBLE];
                    lpacDownState = KPC_ARROW_ANIMATION_STATES[KI_ARROW_INVISIBLE];
                }
                mUpArrowAnimator.AddOutputAptViewState("apt_Transition", lpacUpState, false);
                mDownArrowAnimator.AddOutputAptViewState("apt_Transition", lpacDownState, false);
                break;
            }

            case E_GAMEINPUTACTIONS_GUI_LEFT:
                // Previous round of the highlighted slot (road rage steps in pairs).
                if (miCurrentRound > 0)
                {
                    if (meOptionsToLoad != E_OPTIONS_TO_LOAD_SAVED)
                        lpProfile->GetReceivedOnlineGameOptions(liOption, mpGuiCache, &lParams);
                    else
                        lpProfile->GetCreatedOnlineGameOptions(liOption, mpGuiCache, &lParams);
                    miCurrentRound -= (lParams.meGameMode == GSM::E_MODE_ONLINE_ROAD_RAGE) ? 2 : 1;
                    mRouteInfoDisplay.SetInfo(miCurrentRound, &lParams);
                }
                break;

            case E_GAMEINPUTACTIONS_GUI_RIGHT:
            {
                // Next round of the highlighted slot (road rage steps in pairs).
                if (meOptionsToLoad != E_OPTIONS_TO_LOAD_SAVED)
                    lpProfile->GetReceivedOnlineGameOptions(liOption, mpGuiCache, &lParams);
                else
                    lpProfile->GetCreatedOnlineGameOptions(liOption, mpGuiCache, &lParams);
                const s32 liStep = (lParams.meGameMode == GSM::E_MODE_ONLINE_ROAD_RAGE) ? 2 : 1;
                if (miCurrentRound < lParams.miNumRounds - liStep)
                {
                    miCurrentRound += liStep;
                    mRouteInfoDisplay.SetInfo(miCurrentRound, &lParams);
                }
                break;
            }

            case E_GAMEINPUTACTIONS_GUI_SELECT:
                // Load the highlighted slot into the screen's parameters.
                if (meOptionsToLoad != E_OPTIONS_TO_LOAD_SAVED)
                    lpProfile->GetReceivedOnlineGameOptions(liOption, mpGuiCache, &mGameOptions);
                else
                    lpProfile->GetCreatedOnlineGameOptions(liOption, mpGuiCache, &mGameOptions);
                ShowGameOptionsScreen();
                TriggerSound(leAction);
                break;

            case E_GAMEINPUTACTIONS_GUI_CANCEL:
                ShowGameOptionsScreen();
                TriggerSound(leAction);
                break;

            case E_GAMEINPUTACTIONS_GUI_LSHOULDER:
            case E_GAMEINPUTACTIONS_GUI_RSHOULDER:
                // Swap to the other list when it has entries.
                if (meOptionsToLoad == E_OPTIONS_TO_LOAD_SAVED &&
                    lpProfile->GetNumReceivedOnlineGameOptions() > 0)
                {
                    miStartItem     = 0;
                    meOptionsToLoad = E_OPTIONS_TO_LOAD_RECENT;
                    ShowLoadOptions();
                    mMenuOptions.HighlightIndex(0);
                    mDownArrowAnimator.AddOutputAptViewState(
                        "apt_Transition",
                        KPC_ARROW_ANIMATION_STATES[(lpProfile->GetNumReceivedOnlineGameOptions() <= 1)
                                                       ? KI_ARROW_INVISIBLE : KI_ARROW_VISIBLE],
                        false);
                    mUpArrowAnimator.AddOutputAptViewState(
                        "apt_Transition", KPC_ARROW_ANIMATION_STATES[KI_ARROW_INVISIBLE], false);
                    mLoadHeaderText.SetText(KAC_RECENT_OPTIONS_STRING_ID);
                }
                else if (meOptionsToLoad == E_OPTIONS_TO_LOAD_RECENT &&
                         lpProfile->GetNumCreatedOnlineGameOptions() > 0)
                {
                    meOptionsToLoad = E_OPTIONS_TO_LOAD_SAVED;
                    miStartItem     = 0;
                    ShowLoadOptions();
                    mMenuOptions.HighlightIndex(0);
                    mDownArrowAnimator.AddOutputAptViewState(
                        "apt_Transition",
                        KPC_ARROW_ANIMATION_STATES[(lpProfile->GetNumCreatedOnlineGameOptions() <= 1)
                                                       ? KI_ARROW_INVISIBLE : KI_ARROW_VISIBLE],
                        false);
                    mUpArrowAnimator.AddOutputAptViewState(
                        "apt_Transition", KPC_ARROW_ANIMATION_STATES[KI_ARROW_INVISIBLE], false);
                    mLoadHeaderText.SetText(KAC_CREATED_OPTIONS_STRING_ID);
                }
                break;

            default:
                break;
        }
    }

    // ================================================================================
    //  Update
    // ================================================================================
    void OnlineGameOptions::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liEventSize); lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
        {
            switch (liEventId)
            {
                case KI_EVENT_CONTROLLER:
                    HandleControllerInput(lpEvent);
                    break;

                case 14:
                case 21:
                    break;

                case KI_EVENT_DISCONNECTED:
                    // Latch the disconnect reason for the popup and leave the screen.
                    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
                    mpGuiCache->SetDoDisconnectPopup(lpEvent);
                    SendStateEvent("DISCONNECT");
                    break;

                case KI_EVENT_IN_GAME:
                    HandleInGameEvent(lpEvent);
                    break;

                case KI_EVENT_IN_GAME_FAILED:
                    HandleInGameFailedEvent(lpEvent);
                    break;

                case KI_EVENT_GUI_CACHE:
                    HandleGuiCacheEvent(lpEvent);
                    break;

                case KI_EVENT_OVERLAY_COMPLETE:
                    CGS_ASSERT(lpEvent != 0, "lpOverlayCompleteEvent");
                    break;

                case KI_EVENT_LOBBY_PLAYER_LIST:
                    // An empty lobby means this player is creating the match.
                    CGS_ASSERT(lpEvent != 0, "lpLobbyList");
                    mbIsCreating =
                        static_cast<const GuiEventNetworkLobbyPlayerListPayload*>(lpEvent)->miNumPlayers == 0;
                    break;

                case KI_EVENT_CAR_INFO_RESPONSE:
                    HandleCarInfoResponseEvent(lpEvent);
                    break;

                default:
                    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "Unexpected event received : " << liEventId
                            << " in "
                            << "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineGameOptions.cpp"
                            << " at line " << 333 << "\n";
                    }
                    break;
            }
        }

        mRouteInfoDisplay.Update(mpInGuiEventQueue);
        lpInQueue->Clear();

        CheckForCompletedLoads();
        mCreateGameToggles.Update();
        mMenuOptions.Update();

        if (meSubState > E_SUBSTATE_LOADING_COMPONENTS)
        {
            mHelpBar.Update(mpGuiCache->GetTime());
        }
    }
}

#undef CGS_ARTIST_FILE
