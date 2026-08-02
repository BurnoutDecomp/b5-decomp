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

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache::IsMultiplayerAllowed
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiAudioTriggerEvent

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
        const char KAC_SOUND_LABEL_MENU_TOGGLE[]      = "MenuToggleDefault";
        const char KAC_SOUND_LABEL_MENU_ITEM_TOGGLE[] = "MenuItemToggleDefault";
        const char KAC_SOUND_LABEL_ACCEPT[]           = "Accept";
        const char KAC_SOUND_LABEL_GO_BACK[]          = "GO_BACK";

        // ---- out-queue wire record -----------------------------------------------------
        // The record the inlined OutputGuiEvent<BrnGui::GuiAudioTriggerEvent> builds:
        // { 100, 457, 12, the 100-byte payload }, channel 40, 112 bytes. The committed PC
        // GuiAudioTriggerEvent already carries that 12-byte queue header in front of its
        // 100-byte payload, so the event object IS the record -- but its GuiEvent<201> base
        // seeds the payload-size word 0 and the id 201, while the X360 wire words are the
        // payload size and the id 457. This view restores them without forking the payload
        // type (BrnOnlineGameRoomPlayerInfo_wH_08.cpp carries the identical view).
        const u32 KU_WIRE_ID_AUDIO_TRIGGER = 457;   // X360 `li r11, 0x1C9`

        struct GuiAudioTriggerWire : public GuiAudioTriggerEvent
        {
            GuiAudioTriggerWire()
            {
                muHeader0 = static_cast<u32>(sizeof(GuiAudioTriggerEvent) -
                                             sizeof(CgsGui::GuiEvent<201>));
                muEventType = KU_WIRE_ID_AUDIO_TRIGGER;
            }
        };

        // Record-size pin. The payload is pointer-free, so the X360's AddEvent size
        // immediate must survive unchanged on the x64 host; if a host layout ever drifts,
        // this is where it is caught rather than on the wire.
        static_assert(sizeof(GuiAudioTriggerWire) == 112, "audio trigger record is 112 bytes");
    }

    // ---- CheckPrivileges @ 0x82485C18 ----------------------------------------------
    // Does the signed-in profile allow online play? The page uses this to decide whether
    // to build itself at all when the cache first arrives.
    bool OnlineGameOptions::CheckPrivileges()
    {
        // cpp:1920 -- non-fatal on the X360; the cache pointer is reloaded and used either
        // way (the assert only reports it).
        CGS_ASSERT(mpGuiCache, "mpGuiCache");

        return mpGuiCache->IsMultiplayerAllowed();
    }

    // ---- StoreGameMode @ 0x82490C68 ------------------------------------------------
    // Commit the game-mode toggle row's current value into the match options. The row is
    // only present while the mode toggle exists in the group, so a missing row (index -1)
    // leaves the stored mode untouched.
    void OnlineGameOptions::StoreGameMode()
    {
        if (mCreateGameToggles.GetIndexFromId(CreateMatchOption::E_OPTION_GAME_MODE) == -1)
        {
            return;
        }

        // Trap 5 of the wave-I spec: GetHighlightedOption returns the highlighted row's
        // EOption. The Hex-Rays `>>32` at the other call sites is decompiler noise from the
        // u64 register view -- the X360 switches on r3's low word (`addi r11, r3, -1` then
        // `cmplwi r11, 5`, i.e. the six option rows 1..6).
        const CreateMatchOption::EOption leHighlighted =
            GetHighlightedOption(CreateMatchOption::E_OPTION_GAME_MODE);

        // The stored values are GsmIO::EGameModeType (the same raw-s32 convention the rest
        // of the Gui slice uses for that field): 10 E_MODE_ONLINE_RACE,
        // 11 E_MODE_ONLINE_ROAD_RAGE, 12 E_MODE_ONLINE_FUGITIVE,
        // 13 E_MODE_ONLINE_BURNING_HOME_RUN, 14 E_MODE_ONLINE_FREE_BURN, 17 the last
        // online mode slot. NOTE the option rows and the modes are NOT in the same order --
        // rows 4/5/6 map to 14/17/13 -- so the mapping is written out case by case exactly
        // as the X360 jump table has it.
        switch (leHighlighted)
        {
            case CreateMatchOption::E_OPTION_GAME_MODE_RACE:                // 1
                mGameOptions.meGameMode = 10;
                break;

            // Option row 2 is the road-rage mode row. It has no DWARF-attested enumerator
            // (the Dec-07 enum drifted by one from VEHICLE_CHOICE up and this slot is a
            // gap), so the raw ARTIST row value stands in -- wave-I spec section 2.
            case 2:
                mGameOptions.meGameMode = 11;
                break;

            case CreateMatchOption::E_OPTION_GAME_MODE_STUNT:               // 3
                mGameOptions.meGameMode = 12;
                break;

            case CreateMatchOption::E_OPTION_GAME_MODE_STUNT_FREE_FOR_ALL:  // 4
                mGameOptions.meGameMode = 14;
                break;

            case CreateMatchOption::E_OPTION_GAME_MODE_STUNT_COOP:          // 5
                mGameOptions.meGameMode = 17;
                break;

            // Option row 6 is the sixth mode row (burning home run); same un-attested-slot
            // situation as row 2.
            case 6:
                mGameOptions.meGameMode = 13;
                break;

            default:
                // cpp:1841 -- non-fatal; the X360 leaves the stored mode as it was.
                CGS_ASSERT(false, "Invalid game mode option");
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
                lpcLabel = KAC_SOUND_LABEL_MENU_TOGGLE;
                break;

            // The four LEFT/RIGHT ids -- moving the highlighted row's VALUE.
            case KI_ACTION_GUI_DPAD_LEFT:
            case KI_ACTION_GUI_DPAD_RIGHT:
            case KI_ACTION_GUI_LEFT:
            case KI_ACTION_GUI_RIGHT:
                lpcLabel = KAC_SOUND_LABEL_MENU_ITEM_TOGGLE;
                break;

            case KI_ACTION_GUI_SELECT:
                lpcLabel = KAC_SOUND_LABEL_ACCEPT;
                break;

            case KI_ACTION_GUI_CANCEL:
                lpcLabel = KAC_SOUND_LABEL_GO_BACK;
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

        GuiAudioTriggerWire lAudio;
        lAudio.Construct(KI_AUDIO_ACTION_MENU_CUE, "", lpcLabel, "");

        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAudio), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lAudio)));
    }
}
