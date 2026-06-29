#ifndef BRN_RACE_BALANCING_DEBUG_COMPONENT_H
#define BRN_RACE_BALANCING_DEBUG_COMPONENT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                                       // Vector2 / Vector3 (rw::math::vpu aliases)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent (base), Debug2D/3DImmediateRender fwd

// BrnAI::RaceBalancingDebugComponent -- the in-game race-balancing (AI rubber-band) debug overlay.
// It is a CgsDev::DebugComponent subclass: OnActivate registers its two debug-menu controls
// ("Show data" toggle + "Current rival" slider 0..6), then each frame RenderHUD draws the
// per-rival speed-ratio graph + race-balancing time graph + a stack of textual read-outs, and
// RenderWorld draws a marker sphere over the inspected rival's car.
//
// SOURCE-OF-TRUTH:
//   - Member LAYOUT is pinned to the X360 ARTIST asm (the offset reads in FindAICar @0x827676F8,
//     OnActivate @0x82767688, RenderWorld @0x82774750, RenderHUD @0x82796C08): the base
//     CgsDev::DebugComponent occupies [0x00..0x0B], then mpAIModule (+0x0C), mpRaceBalancingManager
//     (+0x10), miCurrentRival (+0x14), mbShowData (+0x18). Each offset matches a store/load in asm.
//   - Member NAMES / virtual SHAPE come from the DecFIGS DWARF
//     (GameSource/World/AI/RaceBalancing/BrnRaceBalancingDebugComponent.h, struct at :51).
//   - The vtable override set (RenderHUD/RenderWorld/GetName/GetPath/OnActivate) follows the base
//     DebugComponent vtable order [Update, RenderWorld, RenderHUD, GetName, GetPath, IsSimple,
//     OnActivate, OnRegister].
//
// Construct / Destruct / GetPath / DrawGraphBackground are part of the DWARF declaration surface but
// have no standalone X360 symbol in this TU's func set (Construct/Destruct/GetPath are inlined-away
// or live elsewhere; DrawGraphBackground is an external helper). They are declared here for the
// vtable / call surface but bodied in their own TUs -- this TU owns only the 6 functions whose
// bodies live in BrnRaceBalancingDebugComponent.cpp (DrawGraphFrame, FindAICar, GetName, OnActivate,
// RenderHUD, RenderWorld).

namespace BrnAI
{
    class  AIModule;
    struct AICar;
    struct RaceBalancingManager;

    // The DWARF spells the colour params/consts as bare "RGBA"; the project's packed-colour type for
    // the 2D debug renderer is the packed RGBA8 u32 (same as CgsDev::RGBA). Alias it into this scope
    // (as u32, to avoid pulling the heavy renderer header into this declaration) so the declarations
    // read like the source.
    typedef u32 RGBA;
}

namespace BrnAI
{

class RaceBalancingDebugComponent : public CgsDev::DebugComponent
{
public:
    // Two-phase init: wire up the AI module + race-balancing manager pointers and seed the state.
    // (No standalone X360 symbol in this TU; declared for the call surface, bodied elsewhere.)
    void Construct(AIModule* lpAIModule, RaceBalancingManager* lpRaceBalancingManager);

    // Base teardown (bodied in its own TU; declared for the DWARF surface).
    void Destruct();

    // @0x82796C08  2D HUD: the per-rival speed-ratio graph, the race-balancing time graph and the
    // textual read-outs (difficulty, completion ratio, car status, speeds, ...), all gated on
    // mbShowData.
    virtual void RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay);

    // @0x82774750  3D world: a marker sphere 2 units above the inspected rival's car (gated on
    // mbShowData).
    virtual void RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay);

    // @0x827747F8  Draw a labelled graph frame (the two axes, the axis background and the two axis
    // labels) at lv2Origin with the given lv2Size and axis names. Used by RenderHUD for both graphs.
    void DrawGraphFrame(CgsDev::Debug2DImmediateRender* lpDisplay, Vector2 lv2Origin, Vector2 lv2Size,
                        const char* lpcXAxisLabel, const char* lpcYAxisLabel);

protected:
    // @0x82767668  "Race Balancing"
    virtual const char* GetName() const;
    // Bodied in its own TU (DWARF :90); declared for the vtable surface.
    virtual const char* GetPath() const;
    // @0x82767688  Register the "Show data" toggle and the "Current rival" slider (range 0..6).
    virtual void OnActivate();

private:
    // Bodied in its own TU (DWARF BrnRaceBalancingDebugComponent.h:97); declared for the call
    // surface. Fills the graph plot area behind the axes.
    void DrawGraphBackground(CgsDev::Debug2DImmediateRender* lpDisplay, f32 lfX, f32 lfY,
                             f32 lfWidth, f32 lfHeight);

    // @0x827676F8  Find the active AI car whose opponent-index matches miCurrentRival (skipping the
    // player car and cars with no assigned opponent index). Returns null when none matches.
    const AICar* FindAICar(s32 liRivalIndex) const;

    // ---- members (byte offsets pinned to the X360 asm; see header note) ----
    AIModule*             mpAIModule;              // +0x0C
    RaceBalancingManager* mpRaceBalancingManager;  // +0x10
    s32                   miCurrentRival;          // +0x14
    bool                  mbShowData;              // +0x18
};

}

#endif
