// ===================================================================================
// BrnGui::CrashNavColourCalibrate  -- implementation
//   class:BrnGui::CrashNavColourCalibrate
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + the
// per-address assembly in .ida-exports/BURNOUT_X360_ARTIST.XEX/):
//   CrashNavColourCalibrate (ctor) @0x82500120   OnEnter                @0x824B82D0
//   OnLeave                        @0x824CE850   Update                 @0x824E0228
//   UpdateInitSetup                @0x824C1B58   UpdateLoading          @0x824CE900
//   UpdateWFInit                   @0x824D98D8   UpdatePermanent        @0x824DEAD0
//   SetupComponents                @0x824CEA00   ShowCalibrationCard    @0x824CE7A8
//   HandleControllerInput          @0x824D9978   GetSettingsFromProfile @0x824B8378
//   ApplySettings                  @0x824CEB50   SaveSettings           @0x824CEBF0
// Class shape / member order / the two enums are from the DecFIGS DWARF for this exact
// header; the assert strings and their cpp: line numbers are the image's own.
//
// WHAT THIS SCREEN IS. The options "display calibration" page: it puts the grey/white
// calibration card on screen (GUI events 514 show / 515 hide, which the committed
// BrnGui::ColourCalibrationScreen::RecvEvent already consumes), runs a 100-step
// brightness toggle over it, publishes the live pair on every move (GUI event 545 --
// the event BrnGame::BrnGameModule::BridgeGuiToGame has consumed since post-fx step 10)
// and, on accept, writes the pair back into the options profile and requests a save.
// It is the PRODUCER half of the calibration feature; before this TU landed only the
// ctor existed and BrnScreenStatesDataLinkStubs.cpp carried inert OnEnter/OnLeave/Update
// stubs, which is why the options brightness/contrast sliders could not move anything.
//
// REACHABILITY (recovered from the shipped FSM, not assumed) -- build/game/FSM/
// BRNSCREENFSM.BUNDLE: state 108 is "CN_COLOUR" (BrnScreenFlow.cpp:307 registers this
// state under exactly that CgsID). Its ONLY inbound edge is
//     NextState_8CN_SETTINGS(eventId == "TO_COLOUR") -> Transition_8CN_SETTINGS_108CN_COLOUR
// and its ONLY outbound edge is
//     NextState_108CN_COLOUR(eventId == "GO_BACK")   -> Transition_108CN_COLOUR_8CN_SETTINGS
// -- which is precisely the SendStateEvent("GO_BACK") both exit paths below emit.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavColourCalibrate.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (register / out-queue / PlayAptMovie)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / the state in-queue
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (+ GetOptionsDataProfile)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiFlow / GuiAudioTriggerEvent / GuiOptionsBrightnessContrast
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // BrnGui::OptionsDataProfile
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier (the apt movie name)

#include <cstddef>   // offsetof (the record-layout pins below)

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels ---------------------------------------------------
        const s32 KI_CHANNEL_GUI_OUT    = 40;   // X360 `li r5, 0x28` -- GuiEventOut
        const s32 KI_CHANNEL_VIEW_STATE = 41;   // X360 `li r5, 0x29` -- GuiOutViewState

        // ---- observed / consumed event ids ---------------------------------------
        const s32 KI_EVENT_CONTROLLER_INPUT = 6;    // controller action (sub-id @ payload +4)
        const s32 KI_EVENT_GUI_CACHE        = 64;   // per-frame cache event (GuiCache* payload)

        // ---- the ids this screen POSTS (every one read off the asm) ---------------
        // ShowCalibrationCard @0x824CE7A8: `li r11, 0x202` (show) / `li r11, 0x203` (hide)
        // -- exactly the 514/515 pair BrnGuiColourCalibrationScreen::RecvEvent expects.
        // DWARF names the payload structs BrnGui::GuiEventColourCalibrationScreenShow /
        // ...Hide (BrnGuiEventTypeDefs.h:10359-10366, PS3 ids 504/505 -- the usual
        // merge-window drift; the X360 image is authority and says 514/515). Those payload
        // types have no committed home yet (group `guiwire` owns homing them in
        // BrnGuiEventTypeDefs.h); until they land this TU posts the wire record directly,
        // the standing family accommodation (BrnCarSelectUnlock.cpp,
        // BrnOnlineGameOptions_wI_02.cpp, BrnCarSelectMain_wG_02.cpp).
        const s32 KI_EVENT_COLOUR_CALIBRATION_SHOW = 514;   // 0x202
        const s32 KI_EVENT_COLOUR_CALIBRATION_HIDE = 515;   // 0x203
        // 0x164 -- the trailing "commit / apply" marker the settings pages post after a
        // change. Named GuiAutosaveRequestEvent by the mangled-name table
        // (BrnGuiDemangledEventTypes.h:47, "id 356 size 1") and GuiEventCrashNavCommit by
        // BrnCrashNavOptions.h:79. CrashNavOptions posts it with payload byte 0; the X360
        // posts it here with payload byte 1 (`li r11, 1 ; stb r11, var_24`).
        const s32 KI_EVENT_COMMIT_SETTINGS = 356;
        // 0x229 -- SaveSettings' trailing record, posted right after the two profile
        // writes. FLAG: no recovered NAME. It is absent from BrnGuiDemangledEventTypes.h
        // (so no AddGuiOutEvent<T> instantiation carries it) and no committed TU produces
        // or consumes it; the role is read off this one producer -- "the profile changed,
        // save it". Reproduced as the exact wire record, not renamed.
        const s32 KI_EVENT_SAVE_PROFILE_REQUEST = 553;
        // The wire id the inlined OutputGuiEvent<BrnGui::GuiAudioTriggerEvent> stamps
        // (X360 `li r11, 0x1C9`), as opposed to the payload type's own GuiEvent<201>.
        // Same file-local wire view BrnOnlineGameOptions_wI_02.cpp / BrnCarSelectLivery.cpp
        // carry; id 457 is homed only in BrnGuiDemangledEventTypes.h, which hard-collides
        // with the BrnGuiEventTypeDefs.h this TU needs.
        const u32 KU_WIRE_ID_AUDIO_TRIGGER = 457;

        // ---- controller action ids (the action event's payload word +4) ------------
        // BrnGui::EGameInputActions (DWARF GameSource/Input/GameInputActions.h:24; no
        // committed home yet, so they stay s32 -- the same reason
        // BrnOnlineGameOptions_wI_02.cpp:44 gives). The X360 switch is `r4 - 0x2B`
        // against 8 cases, i.e. the contiguous block 43..50; 45..48 fall to the silent
        // default arm.
        const s32 KI_ACTION_GUI_LEFT   = 0x2B;   // 43 GUI_LEFT   -> HighlightPrevious
        const s32 KI_ACTION_GUI_RIGHT  = 0x2C;   // 44 GUI_RIGHT  -> HighlightNext
        const s32 KI_ACTION_GUI_SELECT = 0x31;   // 49 GUI_SELECT -> accept (save + leave)
        const s32 KI_ACTION_GUI_CANCEL = 0x32;   // 50 GUI_CANCEL -> revert + leave

        // The GuiAudioTriggerEvent::meAction value every menu cue in the GUI flow uses
        // (X360 `li r4, 7`); the label is the shared menu-toggle cue.
        const s32  KI_AUDIO_ACTION_MENU_CUE     = 7;
        const char KAC_SOUND_LABEL_MENU_TOGGLE[] = "MenuToggleDefault";
        const char KAC_EMPTY[]                   = "";   // X360 unk_820046A7

        // The brightness row's bounds. The X360 bakes the pair as `cmpwi r11, 1` on the
        // GUI_RIGHT arm and `cmpwi r11, 0x64` on the GUI_LEFT arm -- which are exactly the
        // profile's own validated range, already homed as BrnGui::KI_MIN_BRIGHTNESS (1) /
        // BrnGui::KI_MAX_BRIGHTNESS (100) in BrnGuiOptionsDataProfile.h:27/:33. No local
        // fork: this TU uses those (and KI_DEFAULT_BRIGHTNESS / KI_DEFAULT_CONTRAST, both
        // 50, for OnEnter's `li r10, 0x32` seeds).
        // SetupMenuToggle is handed exactly 100 options with ids 0..99, so the setting is
        // the 1-based row index.
        const s32 KI_NUM_BRIGHTNESS_OPTIONS = 100;   // X360 `li r4, 0x64`

        // The apt movie UpdateLoading plays on display level 3 and OnLeave unloads: the
        // screen's own resource NAME, taken from the shared id->name table rather than
        // re-spelled (the X360 loads `off_82F27B1C`, which is
        // `off_82F278E0 + 143*4` == gGuiResourceIdentifier[143] == "BrnCrashNavColourCalibrate";
        // 143 is also maResourcesToLoad[0].muId below).
        const s32 KI_RESOURCE_ID_COLOUR_CALIBRATE = 143;
        const s32 KI_APT_DISPLAY_LEVEL            = 3;    // X360 `li r11, 3`

        // THE CARD'S TWO COLOURS -- recorded, deliberately NOT defined as code. The DWARF
        // for this TU declares two namespace-scope constants that no recovered X360 body
        // in this class references (references/DecFIGS/dwarfdump/.../BrnCrashNavColourCalibrate.cpp:
        //   BrnGui::KU_GREY  = 1077952576 == 0x40404040
        //   BrnGui::KU_WHITE = 4294967295 == 0xFFFFFFFF
        // i.e. the grey and white halves of the calibration card. On the X360 the card
        // itself is drawn by the post-fx composite from the calibration RAMP TEXTURE, not
        // by this state, so the pair is dead here and adding it as unreferenced code would
        // be noise. It is written down because whoever wires the composite's
        // calibration-card consumer (post-fx BLOCKED-3) will want exactly these two values.

        // The state's in-event queue (CgsGui::State +0x18) is the DWARF
        // InputBuffer::GuiEventQueue, an incomplete alias for the concrete queue
        // instantiation the X360 walks (`VariableEventQueue<18432,16>`).
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // The event-64 payload view (the queue delivers the header-stripped payload; the
        // member name is the X360 assert's).
        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpCachePointer;
        };

        // The event-6 payload view: the action sub-id rides in the payload's +4 word
        // (`lwz r11, 4(r27)`) -- the same idiom BrnPauseScreen.cpp / BrnBootLegal.cpp use.
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (EGameInputActions)
        };

        // ---- out-queue wire records ------------------------------------------------
        // { 1, N, 12, <one payload byte> } -- 16 bytes on channel 40. The shared
        // channel-command record (BrnScreenLoading.cpp's GuiCommandEvent16, same shape).
        // NOTE the console leaves the payload byte UNWRITTEN for 514/515 (payload size 1,
        // no `stb`); zeroed here rather than shipping stack garbage on the wire -- the
        // same call BrnCarSelectUnlock.cpp's GuiCarSelectTickerClosedWire77 makes.
        // The 514/515 PAYLOAD TYPES are homed as BrnGui::GuiEventColourCalibrationScreenShow /
        // ...Hide (BrnGuiEventTypeDefs.h, DWARF-named, EMPTY payloads); this is the RECORD form
        // the console's ShowCalibrationCard @0x824CE7E8/@0x824CE7FC actually builds around them
        // (`li r11, 0x202` / `0x203`), consumed by GuiModule's inbound dispatch -> BrnGui::
        // ColourCalibrationScreen::RecvEvent by id. Same ids at both ends, verified twice.
        template <s32 N>
        struct GuiCommandWire16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;    // +0x0C
            u8 maPad[3];   // +0x0D
            explicit GuiCommandWire16(u8 lu8Flag = 0)
                : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
            { maPad[0] = 0; maPad[1] = 0; maPad[2] = 0; }
        };

        // { 8, 545, 12, brightness, contrast } -- 20 bytes on channel 40. The payload is
        // the HOMED BrnGui::GuiOptionsBrightnessContrast (BrnGuiEventTypeDefs.h:602), so
        // the two words are reached BY NAME and the 545 consumer
        // (BrnGameModule::BridgeGuiToGame) reads the identical type it was written from.
        struct GuiBrightnessContrastWire20 : public CgsGui::GuiEvent<545>
        {
            GuiOptionsBrightnessContrast mPayload;   // +0x0C
            GuiBrightnessContrastWire20(s32 liBrightness, s32 liContrast)
                : CgsGui::GuiEvent<545>(static_cast<u32>(sizeof(GuiOptionsBrightnessContrast)), 12)
            {
                mPayload.mBrightness = liBrightness;
                mPayload.mContrast   = liContrast;
            }
        };

        // The audio cue record: { 100, 457, 12, <the 100-byte payload> }, 112 bytes on
        // channel 40 -- the record the inlined OutputGuiEvent<GuiAudioTriggerEvent>
        // builds. Identical view to BrnOnlineGameOptions_wI_02.cpp's.
        struct GuiAudioTriggerWire : public GuiAudioTriggerEvent
        {
            GuiAudioTriggerWire()
            {
                muHeader0 = static_cast<u32>(sizeof(GuiAudioTriggerEvent) -
                                             sizeof(CgsGui::GuiEvent<201>));
                muEventType = KU_WIRE_ID_AUDIO_TRIGGER;
            }
        };

        // Record-size pins. Every payload here is pointer-free, so the X360's AddEvent
        // size immediates (16 / 20 / 112) must survive unchanged on the x64 host; if a
        // host layout ever drifts this is where it is caught rather than on the wire.
        static_assert(sizeof(GuiCommandWire16<514>) == 16, "card show/hide record is 16 bytes");
        static_assert(sizeof(GuiBrightnessContrastWire20) == 20, "brightness/contrast record is 20 bytes");
        static_assert(sizeof(GuiAudioTriggerWire) == 112, "audio trigger record is 112 bytes");
        static_assert(offsetof(GuiBrightnessContrastWire20, mPayload) == 12,
                      "the 545 payload sits at the record's own header offset word (12)");

        // The per-move menu cue. NOT a recovered function: the X360 emits this exact
        // two-call pair INLINE at both HandleControllerInput sites
        // (`bl GuiAudioTriggerEvent::Construct(&rec, 7, "", "MenuToggleDefault", "")`
        // then `bl CgsGui::StateInterface::OutputGuiEvent<GuiAudioTriggerEvent>`), so it
        // is factored here as a file-local helper rather than invented as a class method
        // the DWARF does not declare. Posted as the wire record the console's inlined
        // OutputGuiEvent builds (id 457, channel 40, 112 bytes) -- see the note on
        // GuiAudioTriggerWire above.
        void PostMenuToggleSound(CgsGui::StateInterface* lpStateInterface)
        {
            GuiAudioTriggerWire lAudio;
            lAudio.Construct(KI_AUDIO_ACTION_MENU_CUE, KAC_EMPTY,
                             KAC_SOUND_LABEL_MENU_TOGGLE, KAC_EMPTY);
            lAudio.muHeader0   = static_cast<u32>(sizeof(GuiAudioTriggerEvent) -
                                                  sizeof(CgsGui::GuiEvent<201>));
            lAudio.muEventType = KU_WIRE_ID_AUDIO_TRIGGER;
            lAudio.muHeader2   = 12u;

            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lAudio), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lAudio)));
        }
    }

    // ---- statics -------------------------------------------------------------------

    // .rdata @0x82F27008 / count @0x82F27018 -- DEFINED IN BrnScreenStatesDataLinkStubs.cpp
    // (that file's measured-table block), not here; the declaration lives in the header.

    // @0x820664C0 (.rdata, 2 entries) + the count word @0x820664C8. The two ids this
    // state registers for.
    //
    // [FLAG INFERRED -- not dumped] The .rdata BYTES at 0x820664C0 have not been read;
    // the pair is derived from the state's own consumers, which is exhaustive: OnEnter
    // passes count 2 (`li r5, 2`), UpdateInitSetup @0x824C1B58 dispatches ONLY
    // `cmpwi r3, 0x40` (64, the GuiCache event) and UpdatePermanent @0x824DEAD0
    // dispatches ONLY `cmpwi r3, 6` (the controller action) -- so the two registered ids
    // are 6 and 64 and nothing else. The ORDER is not attested and is behaviourally
    // irrelevant (RegisterForEvents walks the array registering each id); {6, 64} follows
    // the MEASURED order of the nearest sibling table, BrnPauseScreen's
    // maiEventToObserve @0x82066660 == {6, 64} (BrnPauseScreen.h:49), which registers the
    // same two ids. See CONDUCTOR DUMP REQUEST in the report.
    const s32 CrashNavColourCalibrate::maiEventToObserve[2] =
        { KI_EVENT_CONTROLLER_INPUT, KI_EVENT_GUI_CACHE };
    const s32 CrashNavColourCalibrate::miNumEventsObserved = 2;   // X360 `li r5, 2`

    // The apt component names the X360 passes to MenuToggle::Construct / HelpItem::Construct
    // (string-pool literals aOptiontoggle0_1 / aHelpitemaccept_0). DWARF cpp:52 / cpp:50
    // give the array bounds 15 / 18, which these exactly fill.
    const char CrashNavColourCalibrate::KAC_OPTION_NAME[15]     = "optionToggle_0";
    const char CrashNavColourCalibrate::KAC_HELPITEM_ACCEPT[18] = "HelpItemAccept_mc";

    // off_82F27028 (.rdata, 100 pointers) -- the toggle row's option TEXTS, handed to
    // MenuToggle::SetupMenuToggle together with the id array 0..99.
    //
    // [FLAG BLOCKED / PARTIAL -- 1 of 100 entries attested]
    //   RECOVERED: element [0]. The X360 loads the table base as `addi r7, r11,
    //   off_82F27028@l` and IDA resolves that first pointer's target to the string "1"
    //   (annotated `# "1"` at 0x824CEA7C in SetupComponents' asm).
    //   INFERRED: elements [1..99] as the consecutive decimal strings "2".."100". The
    //   inference is bounded on all sides by attested facts -- the row has exactly 100
    //   options (`li r4, 0x64`), its ids are 0..99 (the `std` fill loop), the setting is
    //   the 1-based row index (`miHighlightedIndex + 1` in HandleControllerInput) and the
    //   X360 clamps it to [1,100] (`cmpwi 1` / `cmpwi 0x64`) -- and it is what a
    //   1..100 brightness scale displays. It is NOT read from the image.
    // The exact bytes are one dump away: see CONDUCTOR DUMP REQUEST in the report
    // (0x82F27028, 400 bytes + the 100 targets). Nothing else in this TU depends on the
    // strings' content; a wrong entry shows as a wrong NUMBER under the calibration card
    // and changes no behaviour.
    const char* CrashNavColourCalibrate::KAPC_BRIGHTNESS_OPTION_VALUES[100] =
    {
        "1",   "2",   "3",   "4",   "5",   "6",   "7",   "8",   "9",   "10",
        "11",  "12",  "13",  "14",  "15",  "16",  "17",  "18",  "19",  "20",
        "21",  "22",  "23",  "24",  "25",  "26",  "27",  "28",  "29",  "30",
        "31",  "32",  "33",  "34",  "35",  "36",  "37",  "38",  "39",  "40",
        "41",  "42",  "43",  "44",  "45",  "46",  "47",  "48",  "49",  "50",
        "51",  "52",  "53",  "54",  "55",  "56",  "57",  "58",  "59",  "60",
        "61",  "62",  "63",  "64",  "65",  "66",  "67",  "68",  "69",  "70",
        "71",  "72",  "73",  "74",  "75",  "76",  "77",  "78",  "79",  "80",
        "81",  "82",  "83",  "84",  "85",  "86",  "87",  "88",  "89",  "90",
        "91",  "92",  "93",  "94",  "95",  "96",  "97",  "98",  "99",  "100",
    };

    // ---- ctor @ 0x82500120 ---------------------------------------------------------
    // Compiler-emitted construction of the colour-calibrate flow state and its embedded
    // GUI sub-objects. The X360 writes the state's own vtable (+0x000), the MenuToggle's
    // Selectable/GuiComponent sub-vtables (+0x038 / +0x050), constructs the toggle's
    // embedded TextSelection (r3 = this+0xE0) and TextField (+0xDA0), then the HelpItem
    // (+0xEE4) and its two ButtonIconComponent children (+0xF70 / +0x1000). Constructing
    // the two embedded widgets by name reproduces every one of those stores; the scalars
    // are deliberately NOT seeded here, exactly as on the console -- OnEnter writes all
    // six of them before anything reads one.
    CrashNavColourCalibrate::CrashNavColourCalibrate()
        : CgsGui::State()         // state vtable + base bookkeeping (X360 +0x000)
        , mBrightnessToggle()     // X360 +0x038 (and the widgets nested inside it)
        , mHelpItemAccept()       // X360 +0xEE4 (and its two button icons)
    {
    }

    // ---- OnEnter @ 0x824B82D0 ------------------------------------------------------
    void CrashNavColourCalibrate::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The X360 stores the four scalars in the shadow of the HelpItem's vtable load
        // (0xED8 / 0xEE0 / 0xEDC / 0xED0), then dispatches the help item's slot 0.
        mBrightnessSetting       = KI_DEFAULT_BRIGHTNESS;   // `li r10, 0x32` (50) -> 0xED8
        mCurrentlySelectedOption = E_CCOPTION_BRIGHTNESS;   // `stw r30(=0), 0xEE0`
        mContrastSetting         = KI_DEFAULT_CONTRAST;     // `stw r10, 0xEDC` (the same 50)
        meCurrentState           = E_INTERNALSCREENSTATE_SETUP;   // `stw r30(=0), 0xED0`

        mHelpItemAccept.Construct(KAC_HELPITEM_ACCEPT, mpStateInterface, 0);

        mbSettingsChanged = false;   // `stb r30, 0x1090`

        // X360 `li r7, -1 ; clrldi r7, r7, 32` -- the 32-bit -1 zero-extended into the
        // 64-bit apt-id slot, i.e. Selectable::K_INVALID_ID.
        mBrightnessToggle.Construct(KAC_OPTION_NAME, mpStateInterface, 0,
                                    Selectable::K_INVALID_ID);
        mBrightnessToggle.Clear();   // component vtable slot 6 (`lwz r11, 0x18(vtbl)`)

        mpGuiCache = 0;   // `stw r30, 0xED4` -- last store in the body
    }

    // ---- OnLeave @ 0x824CE850 ------------------------------------------------------
    void CrashNavColourCalibrate::OnLeave()
    {
        mBrightnessToggle.Clear();          // component vtable slot 6
        ShowCalibrationCard(false);         // posts 515 + the commit marker
        meCurrentState = E_INTERNALSCREENSTATE_LEAVING;   // `li r10, 4 ; stw r10, 0xED0`
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // X360: an inlined bare GuiEventPlayAptMovie (GuiEvent<18>(8,12) { name, level })
        // pushed onto the view-state channel with the EMPTY name (unk_820046A7) and level
        // 3 -- the unload sentinel. StateInterface::PlayAptMovie posts exactly that record.
        mpStateInterface->PlayAptMovie(KAC_EMPTY, KI_APT_DISPLAY_LEVEL);
    }

    // ---- Update @ 0x824E0228 -------------------------------------------------------
    // The family's fall-through sub-state ladder: each rung re-stamps meCurrentState and,
    // when its Update* returns true, falls straight into the next rung in the SAME frame.
    // Only the RUNNING rung runs UpdatePermanent. The in-queue is cleared unconditionally
    // at the tail (so an event consumed by no rung is still dropped).
    void CrashNavColourCalibrate::Update()
    {
        bool lbRunPermanent = true;   // `li r27, 1` before the switch

        switch (meCurrentState)
        {
        case E_INTERNALSCREENSTATE_SETUP:
            lbRunPermanent = false;
            meCurrentState = E_INTERNALSCREENSTATE_SETUP;
            if (!UpdateInitSetup())
            {
                break;
            }
            // fall through

        case E_INTERNALSCREENSTATE_LOADING:
            lbRunPermanent = false;
            meCurrentState = E_INTERNALSCREENSTATE_LOADING;
            if (!UpdateLoading())
            {
                break;
            }
            // fall through

        case E_INTERNALSCREENSTATE_INITIALISING:
            lbRunPermanent = false;
            meCurrentState = E_INTERNALSCREENSTATE_INITIALISING;
            if (!UpdateWFInit())
            {
                break;
            }
            // fall through

        case E_INTERNALSCREENSTATE_RUNNING:
            // The DWARF's UpdateRunning (cpp:449) is inlined to exactly this: stay in
            // RUNNING and let UpdatePermanent run.
            lbRunPermanent = true;
            meCurrentState = E_INTERNALSCREENSTATE_RUNNING;
            break;

        case E_INTERNALSCREENSTATE_LEAVING:
            lbRunPermanent = false;
            meCurrentState = E_INTERNALSCREENSTATE_LEAVING;
            break;

        default:
            // X360 cpp:233 -- the streamed form, "Invalid internal state (" << state << ") \n".
            CGS_ASSERT(false, "Invalid internal state");
            break;
        }

        if (lbRunPermanent)
        {
            UpdatePermanent();
        }

        reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue)->Clear();
    }

    // ---- UpdateInitSetup @ 0x824C1B58 ----------------------------------------------
    // Walk the whole in-queue; every GuiCache event (64) latches its carried pointer.
    // The rung completes on the first one seen (the X360 sets its result flag inside the
    // arm and keeps walking).
    bool CrashNavColourCalibrate::UpdateInitSetup()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        bool lbGotCache = false;
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId != KI_EVENT_GUI_CACHE)
            {
                continue;
            }

            const GuiEventCache* lpCacheEvent = reinterpret_cast<const GuiEventCache*>(lpEvent);
            // X360 cpp:304 -- the streamed form; note the message names OnEnter, not this
            // function (the console's own wording, kept verbatim).
            CGS_ASSERT(lpCacheEvent->mpCachePointer != 0,
                       "Invalid cache in CrashNavColourCalibrate::OnEnter");
            lbGotCache = true;
            mpGuiCache = lpCacheEvent->mpCachePointer;
        }

        return lbGotCache;
    }

    // ---- UpdateLoading @ 0x824CE900 ------------------------------------------------
    // Wait for the two apt resources; once they are in, arm the expected apt components
    // for the toggle row and play this screen's own movie on display level 3.
    bool CrashNavColourCalibrate::UpdateLoading()
    {
        CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // cpp:332

        if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        {
            return false;
        }

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        // X360 (this+0x38, flow 0, cache, withTitle 1).
        mBrightnessToggle.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, true);

        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KI_RESOURCE_ID_COLOUR_CALIBRATE],
                                       KI_APT_DISPLAY_LEVEL);
        return true;
    }

    // ---- UpdateWFInit @ 0x824D98D8 -------------------------------------------------
    // Wait for the flow's apt components to finish initialising, then read the profile
    // and build the row.
    bool CrashNavColourCalibrate::UpdateWFInit()
    {
        CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // cpp:371

        if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            return false;
        }

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
        GetSettingsFromProfile();
        SetupComponents();
        return true;
    }

    // ---- UpdatePermanent @ 0x824DEAD0 ----------------------------------------------
    // Per-frame while RUNNING: hand every controller-action event to the input handler,
    // then tick the toggle row (component vtable slot 5). The queue is NOT cleared here --
    // Update's tail does that.
    void CrashNavColourCalibrate::UpdatePermanent()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_CONTROLLER_INPUT)
            {
                HandleControllerInput(lpEvent);
            }
        }

        mBrightnessToggle.Update();
    }

    // ---- SetupComponents @ 0x824CEA00 ----------------------------------------------
    // Put the calibration card up and (re)build the 100-step brightness row, then move
    // its highlight to the profile's current value.
    void CrashNavColourCalibrate::SetupComponents()
    {
        // The one part of the FLAGged option table that IS machine-checked: its length
        // must equal the option count the X360 hands SetupMenuToggle (`li r4, 0x64`).
        // (Inside the member so it may name the private static.)
        static_assert(sizeof(KAPC_BRIGHTNESS_OPTION_VALUES) /
                          sizeof(KAPC_BRIGHTNESS_OPTION_VALUES[0]) ==
                      static_cast<size_t>(KI_NUM_BRIGHTNESS_OPTIONS),
                      "the brightness option table must hold exactly the 100 rows SetupMenuToggle builds");

        ShowCalibrationCard(true);

        // X360: a 100-entry, 8-byte-stride stack array filled with 0..99 (`std r10, 0(r9)`
        // -- a 64-bit store, so these really are u64 ids, not packed words).
        u64 laOptionIds[KI_NUM_BRIGHTNESS_OPTIONS];
        for (s32 liOption = 0; liOption < KI_NUM_BRIGHTNESS_OPTIONS; ++liOption)
        {
            laOptionIds[liOption] = static_cast<u64>(liOption);
        }

        mBrightnessToggle.Clear();          // slot 6
        mBrightnessToggle.SetActive(true);  // slot 0

        // X360 (this+0x38, 100, 0, "$BRIGHTNESS", off_82F27028, laOptionIds): the row is
        // NOT wrapped -- moving past either end is what the [1,100] clamps below catch.
        mBrightnessToggle.SetupMenuToggle(KI_NUM_BRIGHTNESS_OPTIONS, false, "$BRIGHTNESS",
                                          KAPC_BRIGHTNESS_OPTION_VALUES, laOptionIds);

        mBrightnessToggle.SetHighlightable(true);   // slot 1
        mBrightnessToggle.SetHighlighted(true);     // slot 3
        mBrightnessToggle.SetSelectable(true);      // slot 2

        // Slot 12 on the row's embedded option selection == SelectableGroup::HighlightIndex.
        // The setting is 1-based, the row index 0-based.
        if (mBrightnessToggle.mItemText.HighlightIndex(mBrightnessSetting - 1))
        {
            mBrightnessToggle.SetDirty();
        }
        mBrightnessToggle.SetDirty();       // the X360 dirties the row unconditionally too

        mBrightnessToggle.SetActive(true);  // slot 0, again -- the body's tail call
    }

    // ---- ShowCalibrationCard @ 0x824CE7A8 ------------------------------------------
    // Put the grey/white calibration card on screen, or take it down. Both records go on
    // the state's OWN output queue (`*(this+0x1C) + 0xC` == StateInterface::mOutEventQueue)
    // under channel 40 -- i.e. into the GUI module's out-event stream, which is the same
    // stream BrnGameModule::BridgeGuiToGame drains; the screen-side consumer
    // (BrnGui::ColourCalibrationScreen::RecvEvent) is reached through the GUI module's own
    // observer fan-out, not by a direct hand-off from here.
    void CrashNavColourCalibrate::ShowCalibrationCard(bool lbShow)
    {
        if (lbShow)
        {
            GuiCommandWire16<KI_EVENT_COLOUR_CALIBRATION_SHOW> lShow;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lShow), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lShow)));
            return;
        }

        GuiCommandWire16<KI_EVENT_COLOUR_CALIBRATION_HIDE> lHide;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lHide), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lHide)));

        // The hide path also posts the commit marker with its payload byte SET
        // (`li r11, 1 ; stb r11, var_24`).
        GuiCommandWire16<KI_EVENT_COMMIT_SETTINGS> lCommit(1);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lCommit), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lCommit)));
    }

    // ---- HandleControllerInput @ 0x824D9978 ----------------------------------------
    // The four live actions. LEFT/RIGHT move the toggle (only while RUNNING and only for
    // the brightness option row), re-arm the ACCEPT help prompt, re-read the row's
    // highlight into mBrightnessSetting, clamp it and publish. SELECT saves and leaves --
    // but ONLY if something changed; CANCEL always reverts to the profile and leaves.
    void CrashNavColourCalibrate::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        // X360 cpp:516 -- the streamed form.
        CGS_ASSERT(lpEvent != 0, "Invalid event in CrashNavColourCalibrate::HandleControllerInput");

        const ControllerButtonPayload* lpInput =
            reinterpret_cast<const ControllerButtonPayload*>(lpEvent);

        switch (lpInput->miButtonId)
        {
        case KI_ACTION_GUI_LEFT:
        {
            if (meCurrentState != E_INTERNALSCREENSTATE_RUNNING ||
                mCurrentlySelectedOption != E_CCOPTION_BRIGHTNESS)
            {
                break;
            }
            if (!mBrightnessToggle.HighlightPrevious())
            {
                break;
            }
            mHelpItemAccept.SetItem("$CAPS_BUTTON_ACCEPT",
                                    ButtonIconComponent::E_PADBUTTON_SELECT,
                                    ButtonIconComponent::E_PADBUTTON_INVISIBLE);

            // `lbz 0x185 ; extsb ; addi 1` -- the row's SIGNED highlight index, 1-based.
            mBrightnessSetting = mBrightnessToggle.mItemText.miHighlightedIndex + 1;
            if (mBrightnessSetting > KI_MAX_BRIGHTNESS)
            {
                mBrightnessSetting = KI_MAX_BRIGHTNESS;
            }
            else
            {
                PostMenuToggleSound(mpStateInterface);
            }
            ApplySettings();
            mbSettingsChanged = true;
            break;
        }

        case KI_ACTION_GUI_RIGHT:
        {
            if (meCurrentState != E_INTERNALSCREENSTATE_RUNNING ||
                mCurrentlySelectedOption != E_CCOPTION_BRIGHTNESS)
            {
                break;
            }
            if (!mBrightnessToggle.HighlightNext())
            {
                break;
            }
            mHelpItemAccept.SetItem("$CAPS_BUTTON_ACCEPT",
                                    ButtonIconComponent::E_PADBUTTON_SELECT,
                                    ButtonIconComponent::E_PADBUTTON_INVISIBLE);

            mBrightnessSetting = mBrightnessToggle.mItemText.miHighlightedIndex + 1;
            if (mBrightnessSetting < KI_MIN_BRIGHTNESS)
            {
                mBrightnessSetting = KI_MIN_BRIGHTNESS;
            }
            else
            {
                PostMenuToggleSound(mpStateInterface);
            }
            ApplySettings();
            mbSettingsChanged = true;
            break;
        }

        case KI_ACTION_GUI_SELECT:
            // The X360 does NOTHING at all when nothing has changed -- it does not even
            // leave the screen (the arm branches to the function epilogue).
            if (!mbSettingsChanged)
            {
                break;
            }
            SaveSettings();
            ShowCalibrationCard(false);
            SendStateEvent("GO_BACK");
            break;

        case KI_ACTION_GUI_CANCEL:
            GetSettingsFromProfile();
            ApplySettings();
            ShowCalibrationCard(false);
            SendStateEvent("GO_BACK");
            break;

        default:
            break;
        }
    }

    // ---- GetSettingsFromProfile @ 0x824B8378 ---------------------------------------
    // NOTE the console reads BRIGHTNESS ONLY -- mContrastSetting keeps whatever OnEnter
    // seeded (50) for the whole visit. Faithful, and it is why the CANCEL arm can revert
    // brightness but never contrast.
    void CrashNavColourCalibrate::GetSettingsFromProfile()
    {
        CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // cpp:688

        // X360 `addis r3, r11, 1 ; addi r3, r3, -0x4788` == cache + 0xB878 (47224) ==
        // GuiCache::GetOptionsDataProfile().
        mBrightnessSetting = mpGuiCache->GetOptionsDataProfile()->GetBrightness();
    }

    // ---- ApplySettings @ 0x824CEB50 ------------------------------------------------
    // Publish the live pair as GUI event 545 on channel 40 -- the record
    // BrnGameModule::BridgeGuiToGame's 545 arm reads into miBrightness / miContrast and
    // republishes through the dispatch buffer every frame.
    void CrashNavColourCalibrate::ApplySettings()
    {
        CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // cpp:708

        // X360 `ld` of the ADJACENT member pair 0xED8/0xEDC into one 8-byte payload; on
        // the host the two words are copied by name (same bytes, no guest-offset read).
        GuiBrightnessContrastWire20 lCalibration(mBrightnessSetting, mContrastSetting);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lCalibration), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lCalibration)));
    }

    // ---- SaveSettings @ 0x824CEBF0 -------------------------------------------------
    // Commit the pair into the options profile, then request the profile save. No assert
    // on this path (the console has none here).
    void CrashNavColourCalibrate::SaveSettings()
    {
        OptionsDataProfile* lpProfile = mpGuiCache->GetOptionsDataProfile();
        lpProfile->SetBrightness(mBrightnessSetting);
        lpProfile->SetContrast(mContrastSetting);

        GuiCommandWire16<KI_EVENT_SAVE_PROFILE_REQUEST> lSaveRequest(1);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lSaveRequest), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lSaveRequest)));
    }
}
