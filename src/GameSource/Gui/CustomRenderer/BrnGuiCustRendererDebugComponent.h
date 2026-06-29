#ifndef BRN_GUI_CUST_RENDERER_DEBUG_COMPONENT_H
#define BRN_GUI_CUST_RENDERER_DEBUG_COMPONENT_H

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent (base), Debug2D/3DImmediateRender fwd
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostType.h" // BrnWorld::EBoostType

// BrnGui::GuiCustRendererDebugComponent -- the in-game "Gui Custom Renderer" debug-menu component.
// It lets a developer toggle each custom-render layer (sat-nav, main-map, above-car, ...) on/off,
// override what the game asks to render, and live-edit the boost-bar inner/outer flame colours. It
// is a CgsDev::DebugComponent subclass: OnActivate registers all the menu controls (the per-layer
// bool toggles, the five boost-colour action buttons and the six boost-colour sliders), RenderHUD
// draws the boost-colour debug screen + the active-renderer/texture-count read-out, and the
// SetBoostColour* / ToggleBoostBarDebugScreen callbacks drive the live boost-bar editing.
//
// SOURCE-OF-TRUTH:
//   - Member LAYOUT is pinned to the X360 ARTIST asm (the store offsets in Construct @0x824EC850,
//     the load offsets in OnActivate @0x824FCBC8 / RenderHUD @0x82505108 / SetBoostColourToCustom
//     @0x824FC868 / UpdateBoostColours @0x824F7C48): the base CgsDev::DebugComponent occupies
//     [0x00..0x0B], then mpCustRenderManager (+0x0C), the seven bool flags (+0x10..+0x16) and the
//     six boost-colour floats (+0x18..+0x2C). Each offset matches a store/load in the asm.
//   - Member NAMES / virtual SHAPE / vtable order come from the DecFIGS DWARF
//     (GameSource/Gui/CustomRenderer/BrnGuiCustRendererDebugComponent.h, struct at h:53).
//   - The vtable override set (RenderWorld / RenderHUD / GetName / GetPath / OnActivate) follows the
//     base DebugComponent vtable order [Update, RenderWorld, RenderHUD, GetName, GetPath, IsSimple,
//     OnActivate, OnRegister].
//
// This TU (GameSource/Gui/CustomRenderer/BrnGuiCustRendererDebugComponent.cpp) owns the bodies of
// the 10 X360-attested functions: Construct, Destruct, GetName, OnActivate, RenderHUD and the five
// SetBoostColour*Callback / ToggleBoostBarDebugScreenCallback action callbacks. The remaining DWARF
// methods (RenderWorld, GetPath, SetBoostColour, SetBoostColourToCustom, ToggleBoostBarDebugScreen,
// UpdateBoostColours, and the inline accessors) are part of the declaration surface: the inline
// accessors are bodied here (trivial flag get/set, folded inline in the X360 build), and the rest
// are bodied in their own TUs (SetBoostColourToCustom @0x824FC868 / UpdateBoostColours @0x824F7C48
// are real symbols this TU calls). FLAG: the live boost-bar renderer is reached through the manager
// (the X360 indexes mpCustRenderManager + the embedded BoostBarRenderer subobject offset); the
// SetBoostColour* path forwards that through external helpers (see the .cpp), so this header models
// no raw offset.

namespace BrnGui
{
    class CustomRendererManager;

    class GuiCustRendererDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // @0x824EC850  Wire up the owning custom-renderer manager and seed the flags/colours: every
        // layer toggle off, the master "Enable Custom Renderer" flag on, and the six boost-colour
        // sliders zeroed. Asserts the manager pointer is valid.
        void Construct(CustomRendererManager* lpCustRenderManager);

        // @0x824EC938  Clear the manager pointer (asserts it was set). Chains to the base teardown.
        void Destruct();

        // @0x82505108  2D HUD: the boost-colour debug read-out (gated on mbBoostBarDebugScreenEnabled)
        // and the active-renderers / texture-count list (gated on mbShowCustomRenderStatus).
        virtual void RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay);

        // 3D world pass (no standalone X360 symbol in this TU's func set; declared for the vtable
        // surface, bodied elsewhere).
        virtual void RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay);

        // ---- inline flag accessors (DWARF h:191..268; folded inline in the X360 build) ----
        bool IsOverrideEnabled()      const { return mbOverride; }                 // h:191
        bool IsSatNavDisplayActive()  const { return mbSatNavRenderEnabled; }      // h:204
        void SetSatNavDisplayActive(bool lbActive)  { mbSatNavRenderEnabled = lbActive; }  // h:217
        void SetMainMapDisplayActive(bool lbActive) { mbMainMapRenderEnabled = lbActive; } // h:231
        bool IsMainMapDisplayActive() const { return mbMainMapRenderEnabled; }     // h:243
        // The master "render everything" gate (the "Enable Custom Renderer" toggle, default on).
        bool IsRenderingActive()      const { return mbAllRenderEnabled; }         // h:256
        bool IsAboveCarDisplayActive() const { return mbAboveCarRenderEnabled; }   // h:268

        // ---- boost-colour editing (bodied in their own TUs; declared for the call surface) ----
        // Apply one of the predefined boost-type colour pairs (Danger / Aggression / Stunt) to the
        // live boost-bar renderer, then refresh the cached slider values.
        void SetBoostColour(BrnWorld::EBoostType leBoostType);   // h-bodied elsewhere

        // @0x824FC868  Push the six live slider values (outer RGB + inner RGB) into the boost-bar
        // renderer as a custom colour pair, then refresh the cache. A real symbol this TU calls.
        void SetBoostColourToCustom();

        // The action-button callbacks the debug menu invokes (RegisterFunction targets). Each takes
        // the component as the registered user-data. Bodied in this TU.
        static void SetBoostColourDangerCallback(void* lpUserData);      // @0x824FC900
        static void SetBoostColourAggressionCallback(void* lpUserData);  // @0x824FC9B8
        static void SetBoostColourStuntsCallback(void* lpUserData);      // @0x824FCA70
        static void SetBoostColourToCustomCallback(void* lpUserData);    // @0x824FCB28
        static void ToggleBoostBarDebugScreenCallback(void* lpUserData); // @0x824EC9F0

        // Flip the boost-bar debug screen on/off (folded inline in the X360 build).
        void ToggleBoostBarDebugScreen();   // h-bodied elsewhere

    protected:
        // @0x824EC9E0  "Gui Custom Renderer"
        virtual const char* GetName() const;
        // Bodied in its own TU (DWARF h:102); declared for the vtable surface.
        virtual const char* GetPath() const;
        // @0x824FCBC8  Register every debug-menu control (the per-layer toggles, the five colour
        // action buttons and the six colour sliders) and prime the slider cache.
        virtual void OnActivate();

    private:
        // @0x824F7C48  Re-read the boost-type-indexed inner/outer colours from the live boost-bar
        // renderer and cache them into the six slider floats. A real symbol this TU calls.
        void UpdateBoostColours();

        // ---- members (byte offsets pinned to the X360 asm; see header note) ----
        CustomRendererManager* mpCustRenderManager;  // +0x0C

        bool mbShowCustomRenderStatus;   // +0x10  "Show Custom Renderer Status"
        bool mbSatNavRenderEnabled;      // +0x11  "Sat Nav Render"
        bool mbMainMapRenderEnabled;     // +0x12  "Main Map Render"
        bool mbAllRenderEnabled;         // +0x13  "Enable Custom Renderer" (default true)
        bool mbAboveCarRenderEnabled;    // +0x14  "Above Car Renderer"
        bool mbOverride;                 // +0x15  "Override what game is telling to render"
        bool mbBoostBarDebugScreenEnabled; // +0x16  toggled by ToggleBoostBarDebugScreen

        f32 mfBoostOuterRed;             // +0x18
        f32 mfBoostOuterGreen;           // +0x1C
        f32 mfBoostOuterBlue;            // +0x20
        f32 mfBoostInnerRed;             // +0x24
        f32 mfBoostInnerGreen;           // +0x28
        f32 mfBoostInnerBlue;            // +0x2C
    };
}

#endif // BRN_GUI_CUST_RENDERER_DEBUG_COMPONENT_H
