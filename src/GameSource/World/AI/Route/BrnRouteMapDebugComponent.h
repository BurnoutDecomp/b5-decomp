#ifndef BRN_ROUTE_MAP_DEBUG_COMPONENT_H
#define BRN_ROUTE_MAP_DEBUG_COMPONENT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                                       // Vector2 / Vector3 (rw::math::vpu aliases)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent (base), Debug2D/3DImmediateRender fwd, DebugUI::StringList

// BrnAI::RouteMapDebugComponent -- the in-game AI route/section-map debug overlay (the on-screen
// minimap of AI sections, cars, routes, portals, A* search, HNG planes, ...). It is a
// CgsDev::DebugComponent subclass: it registers its toggles/sliders with the debug menu in
// OnActivate, then RenderHUD (2D minimap) / RenderWorld (3D world overlays) draw whatever toggles
// are on each frame.
//
// SOURCE-OF-TRUTH:
//   - Member LAYOUT is pinned to the X360 ARTIST asm (Construct @0x82767E28 + the offset reads in
//     RenderHUD/RenderWorld/OnActivate): every member's byte offset below matches a store/load in
//     the asm. The 3 pointers (+0xC/+0x10/+0x14), 3 floats (+0x18/+0x1C/+0x20), 2 ints
//     (+0x24/+0x28), the 16 bool toggles (+0x2C..+0x3B), miAISectionIndex (+0x3C) and the
//     3-entry StringList[] for the colour-mode dropdown (+0x40) are all asm-attested.
//   - Member NAMES / virtual SHAPE come from the DecFIGS DWARF
//     (GameSource/World/AI/Route/BrnRouteMapDebugComponent.h, struct at :53).
//   - The vtable override set (RenderWorld/RenderHUD/GetName/GetPath/OnActivate) follows the base
//     DebugComponent vtable order [Update, RenderWorld, RenderHUD, GetName, GetPath, IsSimple,
//     OnActivate, OnRegister].
//
// Several draw helpers (DrawDeadEndsInMap, DrawRouteInMap, DrawAStarInMap, DrawAllPortalGeometry,
// DrawEventCheckpointsInMap, DrawPoint, ToMapCoords, DrawAISectionInfo, Back100SectionsCallback)
// are declared here (DWARF surface) but BODIED IN THEIR OWN TUs -- this TU only owns the 17
// functions whose bodies live in BrnRouteMapDebugComponent.cpp.

namespace BrnAI
{
    class  RouteMapModule;
    struct AISectionsData;
    struct AISection;
    class  AIModule;
    struct Route;

    // The DWARF spells the colour params/consts as bare "RGBA"; the project's packed-colour type is
    // the packed RGBA8 u32 (same as CgsDev::RGBA, which the renderer's DrawLine/DrawText take). Alias
    // it into this scope (as u32, to avoid pulling the heavy renderer header into this declaration)
    // so the declarations read like the source.
    typedef u32 RGBA;
}

namespace BrnAI
{

class RouteMapDebugComponent : public CgsDev::DebugComponent
{
public:
    // @0x82767E28  Two-phase init. Wires up the route-map module + AI module pointers (asserts the
    // route-map module is non-null), zeroes every toggle, and seeds the defaults: map scale 0.1,
    // map offset (580, 450), the 3 colour-mode dropdown labels ("Plain"/"Type"/"Speed").
    void Construct(RouteMapModule* lpRouteMapModule, AIModule* lpAIModule);

    // Base teardown (bodied in its own TU; declared for the DWARF surface).
    void Destruct();

    // @0x8278C7D0  3D world overlays: portals / portal-ids / portal-geometry / HNG-as-planes.
    virtual void RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay);

    // @0x827940A0  2D HUD minimap: AI-section map, cars, master/car routes, A*, event-block
    // sections, dead-ends and the section-info readout (each gated on its toggle).
    virtual void RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay);

protected:
    // @0x82767F30  "Route Map"
    virtual const char* GetName() const;
    // Bodied in its own TU (DWARF :262); declared for the vtable surface.
    virtual const char* GetPath() const;
    // @0x8277FE50  Register every toggle/slider/dropdown/action with the debug menu.
    virtual void OnActivate();

private:
    // ---- the 17 functions whose bodies live in this TU ----

    // @0x8278C890  Draw the AI-section connectivity map (portal-to-portal lines, coloured by
    // type/speed, plus the section/line counts readout). Heavy VMX in the X360 (segment normalise
    // + map-space transform); reconstructed as plain vector math.
    void DrawAISectionMap(CgsDev::Debug2DImmediateRender* lpDisplay) const;

    // @0x8278D7B0  Draw each AI car as an oriented arrow on the minimap, optionally with the car's
    // distance label, its current route and its target section.
    void DrawCarsInMap(CgsDev::Debug2DImmediateRender* lpDisplay) const;

    // @0x8278DD28  Draw the event-block (checkpoint) sections on the minimap with per-checkpoint
    // counts.
    void DrawEventBlockSectionsInMap(CgsDev::Debug2DImmediateRender* lpDisplay) const;

    // @0x82780138  Draw the player's HNG (heading-no-go) section and its neighbours as planes in 3D.
    void DrawHNGAsPlanes(CgsDev::Debug3DImmediateRender* lpDisplay) const;

    // @0x827758F0  Draw one section's no-go boundary lines as upright translucent quads.
    void DrawHNGSectionAsPlanes(CgsDev::Debug3DImmediateRender* lpDisplay, u16 luSectionIndex) const;

    // @0x82767F40  Draw each section's index as 3D text at its first portal (distance-culled).
    void DrawPortalIds(CgsDev::Debug3DImmediateRender* lpDisplay) const;

    // @0x82775280  Draw the portals (and their boundary lines) as 3D markers (distance-culled).
    void DrawPortals(CgsDev::Debug3DImmediateRender* lpDisplay) const;

    // @0x827682A8  Draw an 'X' marker (two crossing segments) on the minimap at lv2Centre.
    void DrawX(CgsDev::Debug2DImmediateRender* lpDisplay, Vector2 lv2Centre, f32 lfSize, RGBA lColour) const;

    // @0x82775780  Pick a section's minimap colour by the current colour mode (type / speed / plain).
    RGBA GetSectionColour(s32 liColourMode, const AISection* lpSection) const;

    // @0x82768120  Section colour by type flags (junction / freeway / dangerous / ...).
    RGBA GetSectionTypeColour(const AISection* lpSection) const;

    // @0x827681C8  Section colour by speed band (muSpeed in [0, E_SECTION_SPEED_COUNT)).
    RGBA GetSectionSpeedColour(const AISection* lpSection) const;

    // @0x82768380  "+100 sections" debug-menu action: bump the inspected section index by 100.
    static void Advance100SectionsCallback(void* lpUserData);

    // ---- declared-only here: bodied in their own TUs (DWARF surface) ----
    void   DrawDeadEndsInMap(CgsDev::Debug2DImmediateRender* lpDisplay) const;
    void   DrawRouteInMap(CgsDev::Debug2DImmediateRender* lpDisplay, const Route* lpRoute, RGBA lColour) const;
    void   DrawEventCheckpointsInMap(CgsDev::Debug2DImmediateRender* lpDisplay) const;
    void   DrawAStarInMap(CgsDev::Debug2DImmediateRender* lpDisplay) const;
    void   DrawAllPortalGeometry(CgsDev::Debug3DImmediateRender* lpDisplay) const;
    void   DrawPoint(CgsDev::Debug2DImmediateRender* lpDisplay, Vector2 lv2Centre, f32 lfSize, RGBA lColour) const;
    Vector2 ToMapCoords(Vector2 lv2World) const;
    Vector2 ToMapCoords(Vector3 lv3World) const;
    void   DrawAISectionInfo(CgsDev::Debug2DImmediateRender* lpDisplay, u16 luSectionIndex);
    static void Back100SectionsCallback(void* lpUserData);

    // ---- members (byte offsets pinned to the X360 asm; see header note) ----
    RouteMapModule* mpRouteMapModule;                // +0x0C
    AISectionsData* mpAISectionsData;                // +0x10
    AIModule*       mpAIModule;                       // +0x14

    f32             mfMapScale;                       // +0x18  (default 0.1)
    f32             mfMapOffsetX;                      // +0x1C  (default 580)
    f32             mfMapOffsetY;                      // +0x20  (default 450)

    s32             miCarIndex;                        // +0x24
    s32             miMapColourMode;                   // +0x28

    bool            mbDrawAISectionMap;                // +0x2C
    bool            mbHighDetailMap;                   // +0x2D
    bool            mbDrawCarsInMap;                   // +0x2E
    bool            mbDrawCarRoutesInMap;              // +0x2F
    bool            mbDrawMasterRouteInMap;            // +0x30
    bool            mbDrawAStarInMap;                  // +0x31
    bool            mbCarInfoInMap;                    // +0x32
    bool            mbDrawDeadEndsInMap;               // +0x33
    bool            mbDrawEventBlockSectionsInMap;     // +0x34
    bool            mbOnlyDrawOneCar;                  // +0x35
    bool            mbOnlyDrawDataForPlayerSection;    // +0x36
    bool            mbDrawPortals;                     // +0x37
    bool            mbDrawPortalIds;                   // +0x38
    bool            mbDrawPortalLines;                 // +0x39  ("Draw Portal Geometry" -> DrawAllPortalGeometry)
    bool            mbDrawHNGAsPlanes;                 // +0x3A
    bool            mbShowSectionInfo;                 // +0x3B

    s32             miAISectionIndex;                  // +0x3C

    // The colour-mode dropdown labels ("Plain"/"Type"/"Speed"); registered with SetOptions
    // (mpiValue = &miMapColourMode @+0x28, lpStringList = &maColourModeList @+0x40).
    CgsDev::DebugUI::StringList maColourModeList[3];   // +0x40
};

}

#endif
