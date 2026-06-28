#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                          // Vector3 / Vector3Plus
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"   // CgsDev::DebugComponent (base) + DebugUI::StringList

// BrnWorld::PVSDebugComponent - the in-game debug component for the streaming PVS
// (potentially-visible-set) system. It draws a top-down 2D overlay of the loaded PVS zones
// (solid / wire convex polygons coloured by per-zone load status or by which zone the player
// is in) plus a separate overlay of the registered "collision zones" (bounding-sphere debug
// circles), and registers the PVS-module score-tuning knobs with the debug menu.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   PVSDebugComponent::Construct                @ 0x827B2108  (this TU)
//   PVSDebugComponent::OnActivate               @ 0x827B2178  (this TU)
//   PVSDebugComponent::RenderCollisionZones     @ 0x827C7378  (this TU)
//   PVSDebugComponent::RenderHUD                @ 0x827CEAD8  (this TU)
//   PVSDebugComponent::RenderPVS                @ 0x827C6E58  (this TU)
//   PVSDebugComponent::RenderPvsCentrePosition  @ 0x827BFB08  (this TU)
//   PVSDebugComponent::GetName                  @ 0x827DD240  -> "PVS"
//   PVSDebugComponent::IsSimple                 @ 0x827E2F38  -> false
//
// Member layout taken from the Construct/RenderHUD/RenderCollisionZones asm displacements
// (0x0C..0x2044) and the DecFIGS DWARF (member names/types). CollisionZone is the small
// bounding-sphere + zone-number record the component stores in its fixed table; its home is
// this header (DWARF BrnPVSDebugComponent.h:53).

namespace CgsDev
{
    struct Debug2DImmediateRender;
}

namespace BrnWorld
{
    // The world-entity module this component debugs. Forward-declared only: the component
    // holds it by pointer and reaches its PVS/zone-list/streamer internals (the full class is
    // not yet reconstructed; see BrnPVSDebugComponent.cpp / RenderPVS / OnActivate).
    class WorldEntityModule;

    // ---- DWARF BrnPVSDebugComponent.h:36 ----
    const s32 KI_MAX_NUM_COLLISION_ZONES = 256;

    // ---- DWARF BrnPVSDebugComponent.h:53 ----
    // One debug "collision zone": a bounding sphere (centre in xyz, radius packed in the w lane
    // of the Vector3Plus) plus an integer zone number. 32 bytes on the console (16-byte vector +
    // s32 + 12 bytes alignment padding), matching the 0x20 array stride in Construct.
    class CollisionZone
    {
    public:
        // @ inline. Zero the sphere and the zone number (the X360 Construct zeroes every record
        // of the table this way: a zeroed 16-byte vector + a zeroed +0x10 word).
        void Construct()
        {
            mBoundingSphere.SetZero();
            miZoneNumber = 0;
        }

        Vector3 GetSpherePosition() const { return mBoundingSphere.GetVector3(); }
        f32     GetSphereRadius()   const { return mBoundingSphere.GetPlus(); }
        s32     GetZoneNumber()     const { return miZoneNumber; }

        void SetSpherePosition(Vector3 lPosition) { mBoundingSphere.SetVector3(lPosition); }
        void SetSphereRadius(f32 lfRadius)        { mBoundingSphere.SetPlus(lfRadius); }
        void SetZoneNumber(s32 liZoneNumber)      { miZoneNumber = liZoneNumber; }

    private:
        Vector3Plus mBoundingSphere;   // DWARF :89 - xyz centre, w = radius
        s32         miZoneNumber;      // DWARF :90 - +0x10
        u8          mPad0[12];         // pad the record to the console's 0x20 stride
    };

    class PVSDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // @ 0x827B2108. Bind the module to debug, clear the collision-zone table (256 records),
        // and default the render toggles off.
        void Construct(WorldEntityModule* lpWorldEntityModule);
        void Destruct();

        // @ 0x827CEAD8. HUD pass: draw the PVS overlay, then (if the collision-zones toggle is on)
        // the collision-zone overlay.
        virtual void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender);

        // @ 0x827C6E58. Draw the loaded PVS zones as solid / wire convex polygons.
        void RenderPVS(CgsDev::Debug2DImmediateRender* lpRender);
        // @ 0x827BFB08. Draw the PVS-centre / player-position marker.
        void RenderPvsCentrePosition(CgsDev::Debug2DImmediateRender* lpRender);
        // @ 0x827C7378. Draw each registered collision zone's bounding-sphere circle + zone number.
        void RenderCollisionZones(CgsDev::Debug2DImmediateRender* lpRender);

        void SetPvsCentre(Vector3 lCentre) { mPvsCentrePosition = lCentre; }
        void SetRendering(bool lbCanRender) { mbCanRender = lbCanRender; }
        void AddCollisionZone(Vector3 lPosition, f32 lfRadius, s32 liZoneNumber);

    protected:
        // @ 0x827B2178. Register the component's render toggles + the PVS-module score-tuning knobs
        // with the debug menu.
        virtual void OnActivate();

        // Identity hooks. The X360 build returns "PVS" / false; GetPath inherits the base "World".
        virtual const char* GetName() const { return "PVS"; }
        virtual bool        IsSimple() const { return false; }

    private:
        WorldEntityModule* mpWorldEntityModule;                       // +0x0C
        CollisionZone      maCollisionZones[KI_MAX_NUM_COLLISION_ZONES]; // +0x10
        s32                miNumCollisionZones;                       // +0x2010
        Vector3            mPvsCentrePosition;                        // +0x2020
        bool               mbCanRender;                               // +0x2030
        u8                 mPad0[3];
        CgsDev::DebugUI::StringList maColourModeOptions[2];           // +0x2034 (DWARF "maStringList[2]")
        bool               mbDrawPVSWireFrame;                        // +0x2044
        u8                 mPad1[3];
    };
}
