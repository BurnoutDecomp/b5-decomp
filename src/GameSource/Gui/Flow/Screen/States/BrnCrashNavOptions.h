#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"         // CgsGui::State (base) + sResourceTuple fwd
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::sResourceTuple (table element)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                     // CgsGui::GuiEvent<N> (OutputEvents record base)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::StateInterface / GuiEventQueueLarge (OutputEvents)
#include "GameSource/Gui/BrnGuiCache.h"                                 // BrnGui::GuiCache (GetOptionsDataProfile)

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

        // DWARF :145-:170 -- declared-only (their own ledger functions). The camera
        // option enum's home is the network module IO (opaque-declared below the
        // namespace); the DWARF signature order is kept.
        void Construct(s32 leCameraUserOption, EOptionsVoipVolumes leVoipVolume,
                       EOptionsSoundVolumes leMusicVolume, EOptionsSoundVolumes leSFXVolume,
                       bool lbSixAxisShowtime, bool lbSixAxisSteering, bool lbForceFeedback,
                       bool lbTips, bool lbDefaultGameCamera);
        void SetFromProfile(OptionsDataProfile* lpProfile);
        void SetToProfile(OptionsDataProfile* lpProfile);
        void OutputEvents(OptionsDataProfile* lpProfile, CgsGui::StateInterface* lpStateInterface);
        void SetUpComponent(MenuToggleGroup* lpMenuToggleGroup);
        // The var-size overload the online game room drives (X360 instantiation
        // SetUpComponent<4>(MenuToggleGroupVarSize<4>*) @0x82485188's callee).
        template <s32 TI_SIZE>
        void SetUpComponent(MenuToggleGroupVarSize<TI_SIZE>* lpMenuToggleGroup);
        void SetVoipVolume(EOptionsVoipVolumes leVoipVolume);
        // (FLAG: the DWARF lists further Set*/Get* past :170 -- grown as their
        //  consumers land; SetCameraUserOptions is declared with the opaque enum's
        //  underlying s32 pending the network-IO enum home.)
        void SetCameraUserOptions(s32 leCameraUserOption);

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
    // The CrashNav options screen state. FLAG: MINIMAL slice -- only the two
    // header-inlines this TU owns plus the members they touch (the full screen
    // state -- components, menu, observers -- lands with the BrnCrashNavOptions.cpp
    // TU). X360 member offsets: mpGuiCache @+60, mOptionsData @+15584.
    // ------------------------------------------------------------------------
    class CrashNavOptions : public CgsGui::State
    {
    public:
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
        static const CgsGui::sResourceTuple maResourcesToLoad[1];   // DWARF h:296 (X360 @0x82F26F50)
        static const u32                    muNumResourcesToLoad;   // DWARF h:297 (X360 @0x82F26F58)

        GuiCache*           mpGuiCache;     // X360 this+60
        CrashNavOptionsData mOptionsData;   // X360 this+15584
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
