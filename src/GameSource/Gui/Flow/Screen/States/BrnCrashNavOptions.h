#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"         // CgsGui::State (base) + sResourceTuple fwd
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::sResourceTuple (table element)
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

    class MenuToggleGroup;                              // pointer-only (own home)
    template <s32 TI_SIZE> class MenuToggleGroupVarSize; // pointer-only (own home)

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
    // FLAG: foreign screen state -- OnlineGameRoomPlayerInfo's own home is its
    // screen-state TU; ONLY ShowSettingsOptions is homed here (this header is the
    // symbol's DWARF primary file). Minimal slice: the three members the inline
    // touches, by name (X360: mOptionsData @+87480, mpGuiCache @+87516,
    // mOptionsComponent @+55584); never instantiated from this slice.
    // ------------------------------------------------------------------------
    class OnlineGameRoomPlayerInfo
    {
    public:
        // @0x82485140 (this TU) -- refresh the options model from the profile and
        // bind it onto the 4-slot toggle group.
        void ShowSettingsOptions()
        {
            mOptionsData.SetFromProfile(mpGuiCache->GetOptionsDataProfile());
            mOptionsData.SetUpComponent<4>(mpOptionsComponentSlice());
        }

    private:
        // The 4-slot toggle group lives BY VALUE @+55584 in the real state; its type
        // is incomplete here (own home), so the slice keeps opaque storage and hands
        // out the typed address (FLAG: adopt the real member when the home lands).
        MenuToggleGroupVarSize<4>* mpOptionsComponentSlice()
        {
            return reinterpret_cast<MenuToggleGroupVarSize<4>*>(maOptionsComponentStorage);
        }

        u8                  maOptionsComponentStorage[1];   // X360 +55584 (opaque; see FLAG)
        CrashNavOptionsData mOptionsData;                    // X360 +87480
        GuiCache*           mpGuiCache;                      // X360 +87516
    };
}
