#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"         // CgsGui::State (base) + sResourceTuple fwd
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::sResourceTuple (table element)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                     // CgsGui::GuiEvent<N> (OutputEvents record base)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface / GuiEventQueueLarge (OutputEvents)
#include "GameSource/Gui/BrnGuiCache.h"                                 // BrnGui::GuiCache (GetOptionsDataProfile)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.h"   // MenuToggleGroupVarSize<4> (EMBEDDED, this+0x40)
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpItem.h"          // HelpItem (EMBEDDED, this+0x3CF8)

// BrnGui::CrashNavOptionsData + the CrashNav "options" screen state -- owning
// header (DWARF home BrnCrashNavOptions.h). This HEADER TU owns the three
// X360-emitted header-inlines:
//   CrashNavOptions::GetResourcesToLoad          @0x82508B00
//   CrashNavOptions::SetSettingsFromProfile      @0x824B8028
//   OnlineGameRoomPlayerInfo::ShowSettingsOptions @0x82485140
// CrashNavOptionsData's class shape is verbatim DWARF (:129-:235); the two screen
// states are MINIMAL slices (their full member sets land with their own .cpp TUs).
namespace BrnGui
{
    // The persisted player-options block the GUI cache exposes @0xB878 (see
    // GuiCache::GetOptionsDataProfile). Pointer-only use here; its own home lands
    // with the profile TU.
    struct OptionsDataProfile;

    class MenuToggleGroup;                               // pointer-only (own home)
    template <s32 TI_SIZE> struct MenuToggleGroupVarSize; // pointer-only (own home:
                                                          // BrnMenuToggleGroup.h, struct tag)

    // -----------------------------------------------------------------------------
    // The GuiEvent records CrashNavOptionsData::OutputEvents pushes onto the state's
    // large output queue (mirror the committed GuiEventPlayAptMovie sibling pattern:
    // derive CgsGui::GuiEvent<N>, set muHeader0/muHeader2 via the base ctor, name the
    // payload fields). Reconstructed from the X360 OutputEvents @0x82494278 AddEvent
    // pushes (type/size/order attested).
    // -----------------------------------------------------------------------------

    // type 278 (0x116), record 16: camera-user-option word (CrashNavOptionsData+0x00).
    struct GuiEventCrashNavCameraUserOption : public CgsGui::GuiEvent<278>
    {
        s32 miCameraUserOption;   // +0x0C
        GuiEventCrashNavCameraUserOption() : CgsGui::GuiEvent<278>(4, 12) {}
    };

    // type 463 (0x1CF), record 20: { music, sfx } pair (CrashNavOptionsData+0x08/0x0C).
    struct GuiEventCrashNavAudioVolumes : public CgsGui::GuiEvent<463>
    {
        s32 miMusicVolume;        // +0x0C
        s32 miSFXVolume;          // +0x10
        GuiEventCrashNavAudioVolumes() : CgsGui::GuiEvent<463>(8, 12) {}
    };

    // type 472 (0x1D8), record 16: three controller toggles (CrashNavOptionsData+0x10..0x12).
    struct GuiEventCrashNavControllerToggles : public CgsGui::GuiEvent<472>
    {
        u8 mbSixAxisShowtime;     // +0x0C
        u8 mbSixAxisSteering;     // +0x0D
        u8 mbForceFeedback;       // +0x0E
        u8 mbPad;                 // +0x0F (X360 leaves the 4th payload byte uninitialised)
        GuiEventCrashNavControllerToggles() : CgsGui::GuiEvent<472>(3, 12) {}
    };

    // type 475 (0x1DB), record 16: default-game-camera flag word read from the PROFILE
    // block @+0x734C (NOT from CrashNavOptionsData). The X360 loads the raw 32-bit word.
    struct GuiEventCrashNavDefaultGameCamera : public CgsGui::GuiEvent<475>
    {
        s32 miDefaultGameCamera;  // +0x0C
        GuiEventCrashNavDefaultGameCamera() : CgsGui::GuiEvent<475>(4, 12) {}
    };

    // type 473 (0x1D9), record 16: tips flag (CrashNavOptionsData+0x13).
    struct GuiEventCrashNavTips : public CgsGui::GuiEvent<473>
    {
        u8 mbTips;                // +0x0C
        GuiEventCrashNavTips() : CgsGui::GuiEvent<473>(1, 12) {}
    };

    // type 356 (0x164), record 16: trailing "commit / apply" marker (payload byte 0).
    struct GuiEventCrashNavCommit : public CgsGui::GuiEvent<356>
    {
        u8 mbFlag;                // +0x0C (X360 stores constant 0)
        GuiEventCrashNavCommit() : CgsGui::GuiEvent<356>(1, 12) {}
    };

    // DWARF BrnCrashNavOptions.h:~120 -- the options screen's data model (camera /
    // voip / music / sfx / sixaxis / force-feedback / tips / default-camera).
    struct CrashNavOptionsData
    {
        // DWARF :93.
        enum EOptionsVoipVolumes
        {
            E_OPTION_VOIP_VOLUMES_0 = 0,  E_OPTION_VOIP_VOLUMES_1 = 1,
            E_OPTION_VOIP_VOLUMES_2 = 2,  E_OPTION_VOIP_VOLUMES_3 = 3,
            E_OPTION_VOIP_VOLUMES_4 = 4,  E_OPTION_VOIP_VOLUMES_5 = 5,
            E_OPTION_VOIP_VOLUMES_6 = 6,  E_OPTION_VOIP_VOLUMES_7 = 7,
            E_OPTION_VOIP_VOLUMES_8 = 8,  E_OPTION_VOIP_VOLUMES_9 = 9,
            E_OPTION_VOIP_VOLUMES_10 = 10,
            E_OPTION_VOIP_VOLUMES_COUNT = 11,
        };

        // DWARF :111.
        enum EOptionsSoundVolumes
        {
            E_OPTION_SOUND_VOLUMES_0 = 0,  E_OPTION_SOUND_VOLUMES_1 = 1,
            E_OPTION_SOUND_VOLUMES_2 = 2,  E_OPTION_SOUND_VOLUMES_3 = 3,
            E_OPTION_SOUND_VOLUMES_4 = 4,  E_OPTION_SOUND_VOLUMES_5 = 5,
            E_OPTION_SOUND_VOLUMES_6 = 6,  E_OPTION_SOUND_VOLUMES_7 = 7,
            E_OPTION_SOUND_VOLUMES_8 = 8,  E_OPTION_SOUND_VOLUMES_9 = 9,
            E_OPTION_SOUND_VOLUMES_10 = 10, E_OPTION_SOUND_VOLUMES_11 = 11,
            E_OPTION_SOUND_VOLUMES_COUNT = 12,
        };

        // Construct is header-inline (the online game room's OnEnter stores the nine
        // fields in place); the rest are their own ledger functions. The camera option
        // enum's home is the network module IO (opaque-declared below the namespace);
        // the declared signature order is kept.
        void Construct(s32 leCameraUserOption, EOptionsVoipVolumes leVoipVolume,
                       EOptionsSoundVolumes leMusicVolume, EOptionsSoundVolumes leSFXVolume,
                       bool lbSixAxisShowtime, bool lbSixAxisSteering, bool lbForceFeedback,
                       bool lbTips, bool lbDefaultGameCamera)
        {
            meCameraUserOption  = leCameraUserOption;
            meVoipVolume        = leVoipVolume;
            meMusicVolume       = leMusicVolume;
            meSFXVolume         = leSFXVolume;
            mbSixAxisShowtime   = lbSixAxisShowtime;
            mbSixAxisSteering   = lbSixAxisSteering;
            mbForceFeedback     = lbForceFeedback;
            mbTips              = lbTips;
            mbDefaultGameCamera = lbDefaultGameCamera;
        }
        void SetFromProfile(OptionsDataProfile* lpProfile);
        void SetToProfile(OptionsDataProfile* lpProfile);
        void OutputEvents(OptionsDataProfile* lpProfile, CgsGui::StateInterface* lpStateInterface);
        void SetUpComponent(MenuToggleGroup* lpMenuToggleGroup);
        // The var-size overload the online game room drives (X360 instantiation
        // SetUpComponent<4>(MenuToggleGroupVarSize<4>*) @0x82485188's callee).
        template <s32 TI_SIZE>
        void SetUpComponent(MenuToggleGroupVarSize<TI_SIZE>* lpMenuToggleGroup);
        // DECLARED SINCE THIS CLASS LANDED AND DEFINED NOWHERE -- both were silent drops
        // (the X360 stores the field inline at each call site).
        void SetVoipVolume(EOptionsVoipVolumes leVoipVolume) { meVoipVolume = leVoipVolume; }

        // DWARF :145-:170's Set*/Get* family, grown for CrashNavOptions (its consumer).
        // The X360 stores/loads these fields inline at the call sites -- OnEnter's entry
        // seeding (@0x824BCEE8) and HandleOptionChanged's four arms (@0x824D9408).
        void SetMusicVolume(EOptionsSoundVolumes leVolume)   { meMusicVolume = leVolume; }
        void SetSFXVolume(EOptionsSoundVolumes leVolume)     { meSFXVolume = leVolume; }
        void SetSixAxisShowtime(bool lbOn)                   { mbSixAxisShowtime = lbOn; }
        void SetSixAxisSteering(bool lbOn)                   { mbSixAxisSteering = lbOn; }
        void SetTips(bool lbOn)                              { mbTips = lbOn; }
        EOptionsSoundVolumes GetMusicVolume() const          { return meMusicVolume; }
        EOptionsSoundVolumes GetSFXVolume() const            { return meSFXVolume; }
        // (FLAG: the DWARF lists further Set*/Get* past :170 -- grown as their
        //  consumers land; SetCameraUserOptions is declared with the opaque enum's
        //  underlying s32 pending the network-IO enum home.)
        void SetCameraUserOptions(s32 leCameraUserOption) { meCameraUserOption = leCameraUserOption; }

        // OnlineGameRoomPlayerInfo::UpdateSoundSettings @0x8249AD00 reads meMusicVolume and
        // meSFXVolume directly (lwzx r11,r31,0x155C0 / 0x155C4 == mCrashNavOptionsData+8/+12),
        // so it needs access to the private field set rather than a raw offset cast.
        friend struct OnlineGameRoomPlayerInfo;

    private:
        // DWARF :228-:236 -- the field set, verbatim. (meCameraUserOption's real type
        // is BrnNetwork::BrnNetworkModuleIO::ECameraUserOptions; kept as its s32
        // underlying pending that home.)
        s32                  meCameraUserOption;    // :228
        EOptionsVoipVolumes  meVoipVolume;          // :229
        EOptionsSoundVolumes meMusicVolume;         // :230
        EOptionsSoundVolumes meSFXVolume;           // :231
        bool                 mbSixAxisShowtime;     // :232
        bool                 mbSixAxisSteering;     // :233
        bool                 mbForceFeedback;       // :234
        bool                 mbTips;                // :235
        bool                 mbDefaultGameCamera;   // :236
    };

    // ------------------------------------------------------------------------
    // CrashNavOptionsData::SetUpComponent<4>  @0x824C0210
    //
    // Fill a four-row toggle group from this model. Defined in the header because the
    // template has two instantiating TUs (CrashNavOptions::UpdateWFInit and
    // OnlineGameRoomPlayerInfo::ShowSettingsOptions) and had NO definition at all, which
    // made every call a silent drop: the rows were never configured, so the options tab
    // showed nothing and could not be moved.
    //
    // Recovered from the RAW ASM: the Hex-Rays output for 0x824C0210 is prefixed "local
    // variable allocation has failed, the output may be wrong!". The console runs rows
    // 0..3 through a jump table inside a loop; the four arms are spelled out here in row
    // order, which is the same store sequence.
    //
    // ⚠️ ROWS 1 AND 2 ARE SET INACTIVE ON THE CONSOLE (`li r6, 0` at 0x824C0394 and
    // 0x824C03F4, against `li r6, 1` for rows 0 and 3). That is the X360's own argument and
    // it is reproduced as-is -- HandleOptionChanged still has live arms for both volume
    // rows, so "active" here is not "reachable".
    // ------------------------------------------------------------------------
    template <s32 TI_SIZE>
    inline void CrashNavOptionsData::SetUpComponent(MenuToggleGroupVarSize<TI_SIZE>* lpMenuToggleGroup)
    {
        CGS_ASSERT(lpMenuToggleGroup != 0, "lpMenuToggleGroup");   // cpp:1063
        if (lpMenuToggleGroup == 0)
        {
            return;
        }

        // .rdata 0x82F26F5C -- the four row captions.
        static const char* const KAPC_ROW_TEXT[4] =
        {
            "$OPTIONS_MENU_CAMERA",         // 0x820649E8
            "$OPTIONS_MENU_MUSIC_VOLUME",   // 0x820649CC
            "$OPTIONS_MENU_SFX_VOLUME",     // 0x820649B0
            "$OPTIONS_MENU_TIPS",           // 0x8206499C
        };

        // .rdata 0x82F26F74 -- row 0's three camera options.
        static const char* const KAPC_CAMERA_OPTIONS[3] =
        {
            "$GENERAL_OPTION_OFF",
            "$GENERAL_OPTION_ON",
            "$CAMERA_OPTION_FRIENDS_ONLY",
        };

        // .rdata 0x82F26FAC -- the twelve volume steps shared by rows 1 and 2. The first
        // entry is the letter "O", not a zero: it is the authored glyph for the off step.
        static const char* const KAPC_VOLUME_OPTIONS[12] =
        {
            "O", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11",
        };

        // .rdata 0x82F26F6C -- row 3's two tips options, ON first.
        static const char* const KAPC_TIPS_OPTIONS[2] =
        {
            "$GENERAL_OPTION_ON",
            "$GENERAL_OPTION_OFF",
        };

        // The id arrays the console builds on the stack alongside each call.
        static u64 KAU_IDS_3[3]   = { 0, 1, 2 };
        static u64 KAU_IDS_12[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
        static u64 KAU_IDS_2[2]   = { 0, 1 };

        // row 0 -- camera (active)
        lpMenuToggleGroup->SetupToggle(0, 3, true, KAPC_ROW_TEXT[0],
                                       const_cast<const char**>(KAPC_CAMERA_OPTIONS), KAU_IDS_3);
        lpMenuToggleGroup->HighlightItem(0, meCameraUserOption);

        // row 1 -- music volume (inactive; see the banner)
        lpMenuToggleGroup->SetupToggle(1, 12, false, KAPC_ROW_TEXT[1],
                                       const_cast<const char**>(KAPC_VOLUME_OPTIONS), KAU_IDS_12);
        lpMenuToggleGroup->HighlightItem(1, static_cast<s32>(meMusicVolume));

        // row 2 -- sfx volume (inactive; see the banner)
        lpMenuToggleGroup->SetupToggle(2, 12, false, KAPC_ROW_TEXT[2],
                                       const_cast<const char**>(KAPC_VOLUME_OPTIONS), KAU_IDS_12);
        lpMenuToggleGroup->HighlightItem(2, static_cast<s32>(meSFXVolume));

        // row 3 -- tips (active). The options are { ON, OFF }, so tips ON highlights 0.
        lpMenuToggleGroup->SetupToggle(3, 2, true, KAPC_ROW_TEXT[3],
                                       const_cast<const char**>(KAPC_TIPS_OPTIONS), KAU_IDS_2);
        lpMenuToggleGroup->HighlightItem(3, mbTips ? 0 : 1);
    }

    // ------------------------------------------------------------------------
    // The CrashNav options screen state. FLAG: MINIMAL slice -- only the two
    // header-inlines this TU owns plus the members they touch (the full screen
    // state -- components, menu, observers -- lands with the BrnCrashNavOptions.cpp
    // TU). X360 member offsets: mpGuiCache @+60, mOptionsData @+15584.
    // ------------------------------------------------------------------------
    class CrashNavOptions : public CgsGui::State
    {
    public:
        // Update's switch subject (X360 this+0x38). The default arm asserts "Invalid
        // internal state (" at cpp:241, so only these five values are ever stored.
        enum EState
        {
            E_STATE_INIT_SETUP = 0,   // drain the cache event, latch mpGuiCache
            E_STATE_LOADING    = 1,   // wait for the screen's resources + declare the apt components
            E_STATE_WF_INIT    = 2,   // wait for those components, then build the rows
            E_STATE_MAIN       = 3,   // interactive
            E_STATE_LEAVING    = 4,   // stop running the per-state legs
        };

        // The four option rows SetUpComponent<4> builds, in the order HandleOptionChanged
        // switches on (X360 @0x824C0210 / @0x824D9408).
        enum EOptionRow
        {
            E_OPTIONROW_CAMERA       = 0,
            E_OPTIONROW_MUSIC_VOLUME = 1,
            E_OPTIONROW_SFX_VOLUME   = 2,
            E_OPTIONROW_TIPS         = 3,
            E_OPTIONROW_COUNT        = 4,
        };

        CrashNavOptions();          // @0x82508AA0

        virtual void OnEnter();     // @0x824BCEE8
        virtual void OnLeave();     // @0x824CF5A0
        virtual void Update();      // @0x824E0BD8

        // @0x82508B00 (this TU) -- hand back the static load table (X360 .data
        // @0x82F26F50: one tuple {id 0x8D, type 4}; count @0x82F26F58 == 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples   = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

        // @0x824B8028 (this TU, cpp:853) -- pull the options model from the profile
        // block the GUI cache exposes.
        void SetSettingsFromProfile()
        {
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // :853 (non-gating)
            mOptionsData.SetFromProfile(mpGuiCache->GetOptionsDataProfile());
        }

    private:
        bool UpdateInitSetup();                                       // @0x824C18B8
        bool UpdateLoading();                                         // @0x824CDC00
        bool UpdateWFInit();                                          // @0x824C19B8
        void UpdatePermanent();                                       // @0x824DFF90
        bool HandleControllerInput(const CgsModule::Event* lpEvent);   // @0x824DE418
        void HandleTriggers(const CgsModule::Event* lpEvent);          // @0x824B7F90
        void HandleOverlayCompleteEvent(const CgsModule::Event* lpEvent); // @0x824D95F0
        void HandleOptionChanged();                                   // @0x824D9408
        void ApplyAndSaveSettings();                                  // @0x824CDD00
        void UpdateSoundSettings();                                   // @0x824CDF10
        void RestoreSoundSettings();                                  // @0x824CDDA0
        void RestoreVoipSettings();                                   // @0x824CDE68
        void StateCancelFlow();                                       // @0x824CE0C0
        void TriggerSound(s32 liAction);                              // @0x824CDFB0
        // The shared 112-byte audio-trigger post the X360 inlines at three sites here.
        void PostAudioTrigger(s32 liAction, const char* lpacMovie);

        static const CgsGui::sResourceTuple maResourcesToLoad[1];   // DWARF h:296 (X360 @0x82F26F50)
        static const u32                    muNumResourcesToLoad;   // DWARF h:297 (X360 @0x82F26F58)

        EState                     meState;            // X360 this+56
        GuiCache*                  mpGuiCache;         // X360 this+60
        MenuToggleGroupVarSize<E_OPTIONROW_COUNT> mMenuToggleGroup;   // X360 this+64
        CrashNavOptionsData        mOptionsData;       // X360 this+15584
        HelpItem                   mHelpItem;          // X360 this+15608
        bool                       mbSettingsChanged;  // X360 this+16036
        bool                       mbInputHandled;     // X360 this+16037
    };

    // ------------------------------------------------------------------------
    // OnlineGameRoomPlayerInfo::ShowSettingsOptions @0x82485140 -- this header TU
    // owns the symbol, but the class's REAL home landed (wave H):
    // GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h, which keeps
    // the inline body verbatim on the real members (mCrashNavOptionsData @+87480 /
    // mpGuiCache @+87516 / mSettingToggle @+55584). The former file-local "slice"
    // class that lived here is retired -- it was flagged "adopt the real member
    // when the home lands", and defining the class in both headers would be an ODR
    // fork now that the real home exists.
    // ------------------------------------------------------------------------
}
