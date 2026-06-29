#ifndef BRN_AI_DEBUG_COMPONENT_H
#define BRN_AI_DEBUG_COMPONENT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                                       // Vector2 / Vector3 (rw::math::vpu aliases)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent (base), Debug2D/3DImmediateRender fwd

// BrnAI::AIDebugComponent -- the in-game "Main AI" debug overlay (the per-car AI state tables,
// route/section/portal/chevron/control-data world overlays, the racing-line and reset-on-track
// markers, ...). Like every debug overlay it is a CgsDev::DebugComponent subclass: it registers
// its toggles/sliders with the debug menu in OnActivate, then RenderWorld (3D world overlays) /
// RenderHUD (2D state tables) draw whatever toggles are on each frame.
//
// SOURCE-OF-TRUTH:
//   - Member NAMES / logical types / virtual SHAPE come from the DecFIGS DWARF
//     (GameSource/World/AI/BrnAIDebugComponent.h, struct at :52).
//   - The vtable override set (RenderWorld / RenderHUD / GetName / GetPath / OnActivate) follows
//     the base CgsDev::DebugComponent vtable order
//     [Update, RenderWorld, RenderHUD, GetName, GetPath, IsSimple, OnActivate, OnRegister].
//   - The RenderWorld dispatch (X360 @0x82796A08) gates each draw helper on the matching toggle;
//     those gates pin which bool each draw call belongs to.
//
// Most draw helpers (DrawAICarRoute / DrawAICarSection / DrawResetOnTrackAISection /
// DrawPortalTargets / DrawDirectDestinationVector / DrawCarControlData / DrawAggressiveTargetPoints
// / DrawAggressiveWarning / DrawCarIndices / DrawChevrons / DrawAISectionEdge / ...) are declared
// here for the vtable/declaration surface; their bodies walk the AIModule's per-car driver array by
// internal byte offset (the X360 build inlines the AIDriver/AICar member reads). Those internal
// AIModule/AIDriver layouts are not yet reconstructed, so the heavy loop bodies live behind those
// missing layouts -- see the .cpp for which functions are bodied in this TU.

namespace BrnAI
{
    class  AIModule;
    struct AISectionsData;
    struct AISection;
    struct Route;
    struct AutoTunedPIDDebugData;

    // The DWARF spells the colour params/consts as bare "RGBA"; the project's packed-colour type is
    // the packed RGBA8 u32 (same as the renderer's DrawLine/DrawText take). Alias it into this scope
    // (as u32, to avoid pulling the heavy renderer header into this declaration surface) so the
    // declarations read like the source.
    typedef u32 RGBA;
}

namespace CgsDev
{
    namespace DebugUI
    {
        // Heavy menu-item type (a MenuItem subclass holding a 5x360 history ring). The DWARF holds
        // two of these by value as layout-tail members; this TU bodies nothing that touches them, so
        // they are forward-declared and reached by pointer to avoid reconstructing the whole
        // MenuItemVariableLineGraph subsystem here (out of scope for this TU).
        struct MenuItemVariableLineGraph;
    }
}

namespace BrnAI
{

class AIDebugComponent : public CgsDev::DebugComponent
{
public:
    // @0x827671?? Two-phase init (DWARF :199): wires up the AI module pointer and seeds the toggle
    // defaults. Bodied in its own TU; declared here for the surface.
    void Construct(AIModule* lpAIModule);
    // Base teardown (DWARF :263). Bodied in its own TU.
    void Destruct();

    // @0x82796A08  3D world overlays: gates each per-car/route/section/portal/chevron draw on its
    // toggle (and forces the speed-calc/personality/buzz sub-toggles on when the states table is on).
    virtual void RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay);
    // 2D HUD: the AI-car state tables / drifting debug. Bodied in its own TU (DWARF :385).
    virtual void RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay);

protected:
    // @0x827671B8  "Main AI"
    virtual const char* GetName() const;
    // @0x82767678  "AI"
    virtual const char* GetPath() const;
    // Register every toggle/slider with the debug menu. Bodied in its own TU (DWARF :462).
    virtual void OnActivate();

private:
    // ---- helpers bodied in this TU ----

    // @0x82767360  Draw one AI state-table cell as 2D text, fading every other row, then advance the
    // running table Y by one row.
    void StateTableDrawEntry(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText, RGBA lColour);

    // @0x827735D8  Draw the current car's route as a line strip through its route nodes (the
    // current node highlighted).
    void DrawAICarRoute(CgsDev::Debug3DImmediateRender* lpDisplay) const;

    // @0x8278BAC8  Draw the next few AI sections under the current car (all-section-data per section).
    void DrawAICarSection(CgsDev::Debug3DImmediateRender* lpDisplay) const;

    // @0x827671C8  Draw the reset-on-track section's two portals (line + the two crossing markers).
    void DrawResetOnTrackAISection(CgsDev::Debug3DImmediateRender* lpDisplay) const;

    // @0x82773798  Draw each car's racing-line target points as solid spheres.
    void DrawPortalTargets(CgsDev::Debug3DImmediateRender* lpDisplay) const;

    // @0x8277F228  Draw, for each car following a route, a line from the car to the last section's
    // first portal.
    void DrawDirectDestinationVector(CgsDev::Debug3DImmediateRender* lpDisplay) const;

    // @0x827738F8  Draw each car's normalised heading/velocity control vectors as lines.
    void DrawCarControlData(CgsDev::Debug3DImmediateRender* lpDisplay) const;

    // @0x82773B48  Draw the aggressive-target sphere + line for each aggressive car chasing a target.
    void DrawAggressiveTargetPoints(CgsDev::Debug3DImmediateRender* lpDisplay) const;

    // @0x82773C18  Draw "DANGER" text over each car being aggressively targeted.
    void DrawAggressiveWarning(CgsDev::Debug3DImmediateRender* lpDisplay) const;

    // @0x827743A0  Draw each car's index ("%d" / "%d*") as 3D text over the car.
    void DrawCarIndices(CgsDev::Debug3DImmediateRender* lpDisplay) const;

    // @0x8278BBE0  Draw the current car's route as on-road turn-direction chevrons.
    void DrawChevrons(CgsDev::Debug3DImmediateRender* lpDisplay) const;

    // @0x8277F390  Draw one chevron edge (a corner of the section quad) as a coloured quad/triangle.
    void DrawAISectionEdge(CgsDev::Debug3DImmediateRender* lpDisplay, const AISection* lpSection,
                           s32 liEdge, Vector3 lv3Direction, s32 liColour) const;

    // @0x82767428  Clears the "drivers stay still" / "max aggression" debug flags on every driver
    // (a debug-menu action callback). DWARF :1934. DECLARED-ONLY / bodied in a follow-on (matches the
    // .cpp scope note). When bodied, the asm shape is a free/static `BOOL(<DriverArrayBase>*)` callback
    // (r3 is the driver base, no `this`), not this member void(void*) form -- adjust the decl then.
    void DeactiveateDriversCallback(void* lpUserData);

    // ---- declared-only here: bodied in their own TUs (DWARF surface) ----
    void DrawAIStatesTable(CgsDev::Debug2DImmediateRender* lpDisplay);
    void DrawAIStatesOnCar(CgsDev::Debug3DImmediateRender* lpDisplay);
    void DrawAIRacingLines(CgsDev::Debug3DImmediateRender* lpDisplay) const;
    void DrawDirectDrivingVector(CgsDev::Debug3DImmediateRender* lpDisplay) const;
    void DrawNearbyVehicles(CgsDev::Debug3DImmediateRender* lpDisplay) const;
    void DrawHNGMap() const;
    void DrawFinishDebugData(CgsDev::Debug3DImmediateRender* lpDisplay) const;
    void DrawDriftingDebug(CgsDev::Debug2DImmediateRender* lpDisplay);
    void StateTableBegin();
    void StateTableNextColumn();

    // ---- members (names/types from the DecFIGS DWARF :233-293) ----
    AIModule*       mpAIModule;                         // :233
    AISectionsData* mpAISectionsData;                   // :234
    s32             miCurrentAICar;                     // :235

    bool            mbDrawCarUnderSection;              // :238

    bool            mbDrawAICarStatesTable;             // :241
    bool            mbDrawSpeedCalculationStateOnTable; // :242
    bool            mbDrawrPersonalityOnTable;          // :243
    bool            mbDrawBuzzTime;                     // :244

    bool            mbDrawAICarStatesOnCar;             // :247
    bool            mbDrawBehaviourOnCar;               // :248
    bool            mbDrawProximityOnCar;               // :249
    bool            mbDrawSpeedOnCar;                   // :250
    bool            mbDrawFanModeOnCar;                 // :251
    bool            mbDrawSpeedMatchingOnCar;           // :252
    bool            mbDrawAggressionStateOnCar;         // :253
    bool            mbDrawAggressionLevelOnCar;         // :254
    bool            mbDrawPersonalityTypeOnCar;         // :255
    bool            mbDrawScheduleOffsetOnCar;          // :256
    bool            mbDrawInvulnerabilityOnCar;         // :257
    bool            mbDrawPlayerSlowSpeedTimeOnCar;     // :258

    bool            mbDrawAICarRoute;                   // :260
    bool            mbDrawCurrentSection;               // :261
    bool            mbDrawResetOnTrackSection;          // :262
    bool            mbDrawAllDriverData;                // :263
    bool            mbDrawRacingLines;                  // :264
    bool            mbDrawPortalTargets;                // :265
    bool            mbDrawDirectVector;                 // :266
    bool            mbDrawDestinationVector;            // :267
    bool            mbDrawControlData;                  // :268
    bool            mbDrawAggressiveTargetPoints;       // :269
    bool            mbDrawAggressiveWarning;            // :270
    bool            mbDrawNearbyVehicles;               // :271
    bool            mbDrawCarIndices;                   // :272
    bool            mbDrawChevrons;                     // :273
    bool            mbDrawHNGMap;                        // :274
    bool            mbDrawFinishDebugData;              // :275
    bool            mbDrawDriftingDebug;                // :276
    bool            mbAlwaysWantToDrift;                // :277
    bool            mbShowDriftTarget;                  // :278
    f32             mfCruisingSpeed;                    // :279

    f32             mfStateTableX;                      // :282
    f32             mfStateTableY;                      // :283

    // Layout-tail UI/PID members (DWARF :289-293). Held here to complete the declaration surface; no
    // function bodied in this TU touches them. The two line-graph menu items are by-value in the
    // DWARF but their subsystem is not reconstructed (out of scope), so they are reached by pointer;
    // the PID debug data is forward-declared (struct defined in the PID subsystem TU).
    CgsDev::DebugUI::MenuItemVariableLineGraph* mpPIDCoefficientsGraph;     // :289
    CgsDev::DebugUI::MenuItemVariableLineGraph* mpSteerAngleVsPIDOutputGraph; // :290
    AutoTunedPIDDebugData*                      mpPIDDebugData;             // :293
};

}

#endif
