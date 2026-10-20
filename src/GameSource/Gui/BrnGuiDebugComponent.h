// BrnGuiDebugComponent.h
// Home of BrnGui::GuiDebugComponent -- the GUI debug-menu component (render/speedo/minimap
// toggles, safe-area overlays, font tests, post-FX hook triggers, the debug finish screen, and
// the per-element race-HUD toggles). Derives from CgsDev::DebugComponent.
//
// Members in declaration order; the console offsets that pin them (the base takes +0x00..+0x0B):
//   mpGuiModule +0x0C  mbRenderGui +0x10  mbSpeedoVisible +0x11  mSpeedoInfo +0x14
//   mbMiniMapVisible +0x20  mbShow43Area +0x21  mbShowSafeArea +0x22  mfSafePercent +0x24
//   mbPriEventOverride +0x28  mfFontSpacing +0x2C  miScreenFlowStreamingMode +0x30
//   maStreamingOptions[3] +0x34  mPFXHookEnumeration +0x4C (hook names +0x50)
//   miCurrentPFXHookIndex +0x1E0  macCurrentPFXHookName +0x1E4  miDebugPlayerRank +0x264
//   miDebugFinishPos +0x268  the six finish/instant-result bools +0x26C..+0x271
//   mbWorstCaseHudActive +0x272  mbWorstCaseHudAlreadyActive +0x273  mfWorstCaseUpdateTime +0x274
//   mbToggleableRaceHUDEnabled +0x278  mabTogglableRaceHUDElementsState[22] +0x279
//   mInputQueue +0x290
// Only the methods bodied in BrnGuiDebugComponent.cpp are declared.

#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"       // CgsDev::DebugUI::StringList
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                  // CgsModule::VariableEventQueue
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                                    // GuiEventUpdateHud
#include "GameSource/Gui/Events/BrnGuiPFXEvents.h"                                 // GuiPFXHookEnumeration

#include "GameSource/Gui/Flow/BrnBaseFlow.h"                              // original streaming enum

namespace CgsGui { namespace CgsGuiModuleIO { struct InputBuffer; } }

namespace BrnGui
{
    class GuiModule;

    struct GuiDebugComponent : public CgsDev::DebugComponent
    {
    public:
        static const s32 KI_NUM_STREAMING_OPTIONS        = 3;
        static const s32 KI_PFX_HOOK_NAME_LENGTH         = 128;
        static const s32 KI_NUM_TOGGLEABLE_RACE_HUD_ITEMS = 22;

        // ARTIST82505488; real two-phase provider, unselected by the
        // current module which still lacks this contained debug owner.
        void Construct(GuiModule* lpGuiModule);
        // ARTIST825076D0..82507710 inlines the original derived teardown.
        void Destruct();

        bool IsGuiRenderingEnabled() const { return mbRenderGui; }
        void SetScreenFlowStreamingMode(BrnBaseFlow::EStreamingMode leMode)
        {
            miScreenFlowStreamingMode = leMode;
        }
        BrnBaseFlow::EStreamingMode GetScreenFlowStreamingMode() const
        {
            return static_cast<BrnBaseFlow::EStreamingMode>(miScreenFlowStreamingMode);
        }

        // ARTIST8250C998; original typed GUI input reader/publisher.
        void Update(CgsGui::CgsGuiModuleIO::InputBuffer* lpInputBuffer);

        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) override;

        // Return the singleton instance pointer, asserting it is non-null (the assert is
        // non-fatal; the pointer is returned either way).
        static GuiDebugComponent* GetSingletonPtr();

    protected:
        const char* GetName() const override;
        const char* GetPath() const override;

        void DrawDebugFrame1280(CgsDev::Debug2DImmediateRender* lpRender,
            Vector4 lvFrame, CgsDev::RGBA lColour, f32 lfBorderSize);

        // Actual nonvirtual members inlined into the original debug callbacks.
        void TriggerIntro();
        void RotateThroughFonts();
        void RotateThroughFontEffects();
        void ChangeFontSpacing();
        void ToggleGenericHUDPriorityOverride();
        void MiniMapSwitch();
        void PlayMovie();
        void ToggleShow43Area();
        void ToggleShowSafeArea();
        void OpenCrashNav();
        void OpenCarSelect();
        void TriggerClosePFX();
        void TriggerEATraxComponent();
        void TriggerOnlineInviteComponent();
        void BootupProfile();
        void AutosaveProfile();
        void SaveProfile();
        void LoadProfile();

        // Post the selected post-FX hook (by name, every maximum weight 1.0) to the GUI.
        void TriggerPFX();
        // Post a made-up offline post-event result built from the debug finish settings.
        void TriggerPostEvent();

    private:
        // The second argument is an encoded item index, including valid index0.
        static void ToggleRaceHudComponentsCallback(void* lpValue, void* lpUserData);
        static void TriggerEATraxComponentCallback(void* lpUserData);
        static void TriggerOnlineInviteCallback(void* lpUserData);
        static void TriggerPaybackComponent(void* lpUserData);
        static void BootupProfileCallback(void* lpUserData);
        static void AutosaveProfileCallback(void* lpUserData);
        static void SaveProfileCallback(void* lpUserData);
        static void LoadProfileCallback(void* lpUserData);
        static void EnterCarSelectCallback(void* lpUserData);
        static void TriggerIntroCallback(void* lpUserData);
        static void TriggerPostEventCallback(void* lpUserData);
        static void OpenCrashNavCallback(void* lpUserData);
        static void MiniMapSwitchCallback(void* lpUserData);
        static void ToggleShow43AreaCallback(void* lpUserData);
        static void ToggleShowSafeAreaCallback(void* lpUserData);
        static void ChangeFontSpacingCallback(void* lpUserData);
        static void PlayMovieCallback(void* lpUserData);
        static void RotateThroughFontsCallback(void* lpUserData);
        static void RotateThroughFontEffectsCallback(void* lpUserData);
        static void TriggerPFXCallback(void* lpUserData);
        static void TriggerClosePFXCallback(void* lpUserData);
        static void ToggleGenericHUDPriorityOverrideCallback(void* lpUserData);

        // The single live GuiDebugComponent instance, set when the debug component is created.
        static GuiDebugComponent* mspSingletonThis;

        GuiModule*                  mpGuiModule;
        bool                        mbRenderGui;
        bool                        mbSpeedoVisible;
        GuiEventUpdateHud           mSpeedoInfo;
        bool                        mbMiniMapVisible;
        bool                        mbShow43Area;
        bool                        mbShowSafeArea;
        f32                         mfSafePercent;
        bool                        mbPriEventOverride;
        f32                         mfFontSpacing;
        s32                         miScreenFlowStreamingMode;
        CgsDev::DebugUI::StringList maStreamingOptions[KI_NUM_STREAMING_OPTIONS];
        GuiPFXHookEnumeration       mPFXHookEnumeration;
        s32                         miCurrentPFXHookIndex;
        char                        macCurrentPFXHookName[KI_PFX_HOOK_NAME_LENGTH];
        s32                         miDebugPlayerRank;
        s32                         miDebugFinishPos;
        bool                        mbDebugFinishRankup;
        bool                        mbDebugFinishUnlockFreeCar;
        bool                        mbDebugFinishUnlockNormalCar;
        bool                        mbDebugFinishUnlockRival;
        bool                        mbDebugOfflineInstantResults;
        bool                        mbDebugOnlineInstantResults;
        bool                        mbWorstCaseHudActive;
        bool                        mbWorstCaseHudAlreadyActive;
        f32                         mfWorstCaseUpdateTime;
        bool                        mbToggleableRaceHUDEnabled;
        bool                        mabTogglableRaceHUDElementsState[KI_NUM_TOGGLEABLE_RACE_HUD_ITEMS];
        CgsModule::VariableEventQueue<18432, 16> mInputQueue;   // InputBuffer::GuiEventQueue
    };
}
