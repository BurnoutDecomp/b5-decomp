#ifndef BRN_GUI_EFFECTS_ARBITRATOR_H
#define BRN_GUI_EFFECTS_ARBITRATOR_H

#include "types.hpp"
#include "GameSource/Gui/PFX/BrnGuiPFXHooks.h"                       // BrnGui::PFXHookBundle / PFXHook
#include "GameSource/Gui/PFX/BrnPfxHookBlender.h"                    // BrnGui::PFXHookNodeBlender / ColourCubeInfo
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"   // CgsResource::ResourcePtr<PFXHookBundle>
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"  // CgsDev::DebugUI::StringList
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"             // CgsModule::VariableEventQueue (the GUI out queue)
#include "SharedClasses/BrnSharedConstants.h"                         // BrnUpdateSet

namespace CgsGui
{
    namespace CgsGuiModuleIO { class InputBuffer; class OutputBuffer; }
    namespace ModelIO        { class InputBuffer; class OutputBuffer; }
}
namespace RendererIO { class OutputBuffer; }
class BrnEffectsFrame;

namespace BrnGui
{
    class GuiCache;
    struct GuiPFXHookEvent;
    struct GuiPFXStartBackgroundHookEvent;
    struct GuiPFXStopBackgroundHookEvent;

    // BrnGuiEffectsArbitrator.h:37..39
    const s32 KI_MAX_NUM_CONCURRENT_EFFECTS = 1;
    const s32 KI_MAX_NUM_HOOKS_PER_BUNDLE   = 20;
    const u32 KU_MAX_NUM_COLOURCUBES        = 6;

    // =========================================================================
    // BrnGui::EffectsArbitrator (DecFIGS DWARF BrnGuiEffectsArbitrator.h:54)
    //
    // THE SCREEN-FILTER SYSTEM. A "hook" is one authored post-FX effect out of PFXHOOKS.PFX
    // (Crash, Post_Crash, Damage_Crit, Rival_Hit_You, You_Hit_Rival, Wrecked, Jump_Effect,
    // Showtime, Black_White, Flash_B_W ... 67 of them). Gameplay starts and stops hooks by
    // GUI event (495 start / 496 stop / 497 stop-menu / 498 start-background / 499
    // stop-background, all posted by BrnGameModule::BridgeDirectorToGui from the director
    // camera's effect requests); this class runs them through five PFXHookNodeBlenders --
    // the current hook, the one it crossfades out of, the menu hook, and the same pair for
    // the background layer -- and every frame writes the two "FX events" BrnEffectsFrames
    // the renderer's BrnGraphics::EffectsArbitrator blends into the post-FX composite.
    //
    // The console offsets the readers inline are provenance only; the object is carved
    // inside BrnGui::GuiModule (gm + 1546880 on the X360) and never serialised, so its
    // pointer members widen on x64 and nothing here pins a console byte offset.
    // =========================================================================
    class EffectsArbitrator
    {
    public:
        // BrnGuiEffectsArbitrator.h:135
        enum EHookState
        {
            E_NORMAL         = 0,
            E_CROSSFADE      = 1,
            E_FADE_TO_MENU   = 2,
            E_FULL_MENU      = 3,
            E_FADE_FROM_MENU = 4,
            E_PAUSED         = 5,
        };
        // BrnGuiEffectsArbitrator.h:145
        enum EBackgroundHookState
        {
            E_BACKGROUND_INACTIVE    = 0,
            E_BACKGROUND_RUNNING     = 1,
            E_BACKGROUND_CROSSFADING = 2,
        };

        // @0x827E06B8 -- six ColourCubeInfo resource pointers and the bundle pointer, each
        // self-linked (BaseResourcePtr's own ctor); the members' ctors do that here.
        EffectsArbitrator() {}

        void Construct();                                             // :59  @0x825175A0
        void Destruct();                                              // :63
        // :69 @0x825177E8 -- consume the model's load notifications: 228 is the hook bundle
        // (then the colour cubes are requested), 229..234 the cubes.
        void ResourceUpdate(CgsGui::ModelIO::InputBuffer* lpGuiModelInputBuffer,
                            const CgsGui::ModelIO::OutputBuffer* lpGuiModelOutputBuffer);
        // :75 @0x825125C0 -- the GUI events (64 cache, 495..500 hooks), then UpdateHooks.
        // [FLAG PC bring-up] the console hands its CgsGuiModuleIO::OutputBuffer (AddGuiOutEvent);
        // the PC GuiModule posts its out-events on the bare 18432-byte queue the game bridge
        // drains (mGuiOutQueue), so that queue is what arrives here. Same record, same id 501.
        void EventUpdate(BrnUpdateSet leUpdateSet,
                         const CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer,
                         CgsModule::VariableEventQueue<18432, 16>* lpGuiOutQueue);
        // :81 @0x82503060 -- fill the renderer's two FX-events effects frames.
        void GenerateEffectFrameEvents(RendererIO::OutputBuffer* lpRenderOutput);
        // [FLAG PC bring-up] the same body on the two FX-events frames themselves. The console
        // reaches them through the renderer output buffer above; on this build that buffer
        // set is not live (BrnRendererModule::Update never runs with real IO buffers), so
        // BrnGameModule::DoDispatch hands the arbitrator's external FX-events frames across
        // directly, beside the world layer's (GetFXEventsEffectsFrameBringUp).
        // DELETE-WHEN the RendererIO buffers are real on PC.
        void GenerateEffectFrameEvents(BrnEffectsFrame* lpFrame1, BrnEffectsFrame* lpFrame2);
        // [FLAG PC bring-up] the GuiCache hand-over GUI event 64 performs on the console; the PC
        // GuiModule::Update synthesises that event outside the module-input queue, so it calls
        // this instead. Same store, same one-shot guard as EventUpdate's case 64.
        void SetGuiCache(GuiCache* lpGuiCache);

    private:
        const PFXHook* GetHookFromName(const char* lpcName);          // :87  @0x82502C78
        const PFXHook* GetHookFromGUID(u32 luGuid);                   // :91  @0x82502D70
        void StartHook(const GuiPFXHookEvent* lpGuiPFXHookEvent);     // :96  @0x8250B618
        void StopHook(const PFXHook* lpHook);                         // :101 @0x824FB398
        void StartBackgroundHook(const GuiPFXStartBackgroundHookEvent* lpEvent);   // :106 @0x8250B1F0
        void StopBackgroundHook(const GuiPFXStopBackgroundHookEvent* lpEvent);     // :110 @0x8250B478
        void StopMenuHook();                                          // :114 @0x824FB478
        void UpdateHooks();                                           // :118 @0x82502E38
        void Acquire3dTints(CgsGui::ModelIO::InputBuffer* lpGuiModelInputBuffer);  // :123 @0x82512AA8
        ColourCubeInfo* LookupColourCube(u64 luResourceId);           // :126 @0x824F5B98

        // :186..:189 -- the four debug-menu callbacks (PFX / Start Hook ...).
        static void Debug_StartHook(void* lpThis);                    // @0x82512550
        static void Debug_StartBackgroundHook(void* lpThis);          // @0x825124F8
        static void Debug_StopHook(void* lpThis);                     // @0x824FB330
        static void Debug_StopBackgroundHook(void* lpThis);           // @0x824FB2C8

        // The hook the console resolves an event's GUID / name / "2dflash" fallback to --
        // the same three-step lookup StartHook, EventUpdate(496) and the background pair
        // share (inlined at each on the console).
        const PFXHook* ResolveHook(u32 luGuid, const char* lpcName, const char* lpcCaller);

        ColourCubeInfo                          maColourCubes[KU_MAX_NUM_COLOURCUBES];      // :128  +0x000
        u32                                     mauColourCubeRequestIds[KU_MAX_NUM_COLOURCUBES]; // :129  +0x0F0
        u32                                     muColourCubeCount;                          // :131  +0x108
        CgsResource::ResourcePtr<PFXHookBundle> mpHookInfo;                                 // :133  +0x10C
        PFXHookNodeBlender                      mBlender1;                                  // :152  +0x130
        PFXHookNodeBlender                      mBlender2;                                  // :153  +0xC40
        PFXHookNodeBlender                      mMenuBlender;                               // :154  +0x1750
        PFXHookNodeBlender                      mBackgroundBlender1;                        // :155  +0x2260
        PFXHookNodeBlender                      mBackgroundBlender2;                        // :156  +0x2D70
        PFXHookNodeBlender*                     mpCurrent;                                  // :158  +0x3880
        PFXHookNodeBlender*                     mpOutgoing;                                 // :159  +0x3884
        PFXHookNodeBlender*                     mpMenu;                                     // :160  +0x3888
        PFXHookNodeBlender*                     mpBackgroundCurrent;                        // :161  +0x388C
        PFXHookNodeBlender*                     mpBackgroundOutgoing;                       // :162  +0x3890
        EHookState                              meStateBeforeMenu;                          // :164  +0x3894
        EHookState                              meHookState;                                // :165  +0x3898
        EBackgroundHookState                    meBackgroundHookState;                      // :166  +0x389C
        f32                                     mfMenuPhaseTimeExpired;                     // :167  +0x38A0
        f32                                     mfMenuPhaseTimeToEnd;                       // :168  +0x38A4
        f32                                     mfCrossFadeTimeExpired;                     // :170  +0x38A8
        f32                                     mfCrossFadeTimeToEnd;                       // :171  +0x38AC
        f32                                     mfBackgroundFadeTimeExpired;                // :173  +0x38B0
        f32                                     mfBackgroundFadeTimeToEnd;                  // :174  +0x38B4
        GuiCache*                               mpGuiCache;                                 // :176  +0x38B8
        f32                                     mfLastTime;                                 // :177  +0x38BC
        bool                                    mbForceTime;                                // :180  +0x38C0
        f32                                     mfForcedTime;                               // :181  +0x38C4
        f32                                     mfBackgroundWeight;                         // :182  +0x38C8
        s32                                     miPfxHookIndex;                             // :183  +0x38CC
        CgsDev::DebugUI::StringList             maPfxHookList[100];                         // :184  +0x38D0
    };
}

#endif
