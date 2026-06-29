#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityDebugComponent.h
//
// BrnWorld::PropEntityDebugComponent -- the in-game debug overlay for the prop entity
// module (the "Physics / Prop Entity Module" page in the debug menu). Lets a developer
// toggle, per render pass, the prop/part collision-volume wireframes, the per-prop /
// per-part stat read-outs, the per-zone cell grid, the inertia boxes, and the traffic-
// light markers, and registers a handful of the owning module's tuning flags with the
// debug menu (OnActivate).
//
// Reconstructed from:
//   - BURNOUT_X360_ARTIST.XEX asm  -> member offsets + behaviour (authoritative). The 12
//     bodies (Construct 0x822A96A8, Draw 0x822A9770, GetName 0x822A9740,
//     OnActivate 0x822C52C8, RenderCellGrid 0x822EFF48, RenderHUD 0x822FC108,
//     RenderInertiaBoxes 0x822DC520, RenderModuleStats 0x822DE3F8,
//     RenderPropStats 0x822DDAE8, RenderProps 0x822DCBF8, RenderTrafficLights 0x822DD868,
//     RenderWorld 0x822EFEB8) pin every member offset used below.
//   - DecFIGS DWARF (BrnPropEntityDebugComponent.h) -> member names/types/order + the
//     class' virtual/method shapes (X360-gated; PS3-only members left out).
//   - references/Feb-2007/.../BrnPropEntityDebugComponent.{h,cpp} -> style / idiom only.
//
// LAYOUT (offsets are X360-console facts; on the x64 PC compile pointer widening shifts
// the base region, so these are provenance, not host asserts -- we model the PC layout
// and do NOT byte-match). Construct @0x822A96A8 + OnActivate/RenderWorld/RenderHUD pin:
//   mpPropEntityModule     @ +0x34 (52)   PropEntityModule*           (Construct stw 0x34)
//   mbRenderProps          @ +0x38 (56)   "Render prop collision volumes"
//   mbRenderPropStats      @ +0x39 (57)   "Render the stats for each prop"
//   mbRenderPartStats      @ +0x3A (58)   "Render the stats for each part"
//   mbRenderModuleStats    @ +0x3B (59)   "Render PropEntityModule stats"
//   mbRenderCellGrid       @ +0x3C (60)   "Render cell grid"
//   mbRenderInertiaBoxes   @ +0x3D (61)   "Render Inertia Boxes"
//   mbRenderTrafficLights  @ +0x3E (62)   "Render props marked as Traffic Lights"
//   mfCellGridZoom         @ +0x40 (64)   f32   ("Cell grid zoom"; Construct = 1.0, step 0.1)
//   muUsedProps/Parts @ +0x48/+0x4C  (RenderModuleStats popcount scratch; the binary has
//     no +0x44 "used instances" field -- the slow-cadence switch only recomputes a1[18]
//     (used props) on frame 10 and a1[19] (used parts) on frame 20)
//   muFrameCountForUpdate  @ +0x50 (80)   u32   (Construct = 0)
//   mMapDimensions/mScreenSize/mfAspect  (cell-grid screen-projection cache; DWARF
//     members 82..84, used by ToCellGridScreenCoords) precede mpPropEntityModule.
//
// NOTE on the DWARF member set: DecFIGS additionally lists Destruct / GetPath / IsSimple /
// OnRegister / RenderParts / RenderPartStats / ToCellGridScreenCoords / ResetProps. The
// X360 ledger attests this build's set as the 12 functions above PLUS the two private
// helpers the bodies call out-of-line (RenderInertiaBox 0x822C54C0, ToCellGridScreenCoords
// 0x822DC3D8) and ResetProps (the OnActivate "Reset props" callback @0x822EFE48). Those
// three are declared here (this class owns them) but their bodies are their own TUs.
// Members/methods the X360 set does NOT attest (the Feb-2007 mbRenderParts / mbRenderPartStats
// / mbRenderPhysicalRed / mbRenderFrozenBlue split, RenderParts/RenderPartStats) are dropped.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2, Vector3, Matrix44Affine, VecFloat
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent

namespace rw { namespace collision { class Volume; } }

namespace BrnWorld
{
    class PropEntityModule;

    // ========================================================================
    // BrnWorld::PropEntityDebugComponent
    // ========================================================================
    class PropEntityDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // 0x822A96A8 -- run the base DebugComponent ctor, stash the owning module, clear
        // every render flag, default the cell-grid zoom to 1.0 and the frame counter to 0.
        void Construct(PropEntityModule* lpPropEntityModule);

        // RenderWorld 0x822EFEB8 -- the world-space (3D) debug pass: volumes, prop stats,
        // inertia boxes, traffic-light markers (each gated on its flag).
        virtual void RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay);
        // RenderHUD 0x822FC108 -- the screen-space (2D) debug pass: module stats + cell grid.
        virtual void RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay);

    protected:
        // GetName 0x822A9740 -- "Prop Entity Module".
        virtual const char* GetName() const;
        // OnActivate 0x822C52C8 -- register the render flags, cell-grid zoom, the module's
        // override tuning + online/progression flags, and the "Reset props" action with the
        // debug menu.
        virtual void OnActivate();

    private:
        // RenderProps 0x822DCBF8 -- per prop in every loaded zone, within the volume-render
        // distance and not yet smashed: draw each of its collision volumes (and, for moved/
        // smashed parts, the per-part volumes) plus a small per-prop stat string.
        void RenderProps(CgsDev::Debug3DImmediateRender* lpDisplay);
        // RenderPropStats 0x822DDAE8 -- per prop within the (closer) stat-render distance:
        // draw a multi-line stat read-out (type / instance id / index) at the prop origin.
        void RenderPropStats(CgsDev::Debug3DImmediateRender* lpDisplay);
        // RenderInertiaBoxes 0x822DC520 -- per prop: draw the inertia-tensor box of every
        // collision volume, coloured by the volume's solid/wire state.
        void RenderInertiaBoxes(CgsDev::Debug3DImmediateRender* lpDisplay);
        // RenderTrafficLights 0x822DD868 -- per prop whose type is flagged as a traffic
        // light, within the render distance: draw an "ID: %u" marker at the prop.
        void RenderTrafficLights(CgsDev::Debug3DImmediateRender* lpDisplay);
        // RenderModuleStats 0x822DE3F8 -- the 2D module-wide stat panel (loaded zones, prop/
        // part counts, pool usage), refreshed on a slow frame cadence.
        void RenderModuleStats(CgsDev::Debug2DImmediateRender* lpDisplay);
        // RenderCellGrid 0x822EFF48 -- the 2D top-down cell-grid map for the player's zone:
        // each loaded cell as a polygon (solid if active) plus a marker per prop cell entry.
        void RenderCellGrid(CgsDev::Debug2DImmediateRender* lpDisplay);

        // Draw 0x822A9770 -- draw one collision volume (sphere / box / aabbox / capsule)
        // under a world transform, in the debug colour for its type.
        void Draw(rw::collision::Volume* lpVolume,
                  CgsDev::Debug3DImmediateRender* lpDisplay,
                  Matrix44Affine lTransform) const;

        // ---- out-of-line private helpers (this class owns them; bodies are their own TUs) ----
        // 0x822DC3D8 -- project a world position into the 2D cell-grid map.
        Vector2 ToCellGridScreenCoords(Vector3 lWorldPosition);
        // 0x822C54C0 -- draw a single inertia box (called per-volume by RenderInertiaBoxes).
        void RenderInertiaBox(CgsDev::Debug3DImmediateRender* lpDisplay,
                              Matrix44Affine lTransform, Vector3 lCentre, VecFloat lfExtent);
        // 0x822EFE48 -- the "Reset props" debug-menu action callback.
        static void ResetProps(void* lpUserData);

    private:
        // ---- DWARF-faithful layout (X360 offsets in the banner above) ----
        Vector2            mMapDimensions;        // cell-grid map size (world units)
        Vector2            mScreenSize;           // cell-grid map size (screen units)
        f32                mfAspect;              // cell-grid map aspect ratio
        PropEntityModule*  mpPropEntityModule;    // +0x34 owning module
        bool               mbRenderProps;         // +0x38
        bool               mbRenderPropStats;     // +0x39
        bool               mbRenderPartStats;     // +0x3A
        bool               mbRenderModuleStats;   // +0x3B
        bool               mbRenderCellGrid;      // +0x3C
        bool               mbRenderInertiaBoxes;  // +0x3D
        bool               mbRenderTrafficLights; // +0x3E
        f32                mfCellGridZoom;        // +0x40 (default 1.0)
        u32                muUsedProps;           // +0x48 (RenderModuleStats popcount scratch)
        u32                muUsedParts;           // +0x4C
        u32                muFrameCountForUpdate; // +0x50 (slow-cadence refresh counter)
    };
}
