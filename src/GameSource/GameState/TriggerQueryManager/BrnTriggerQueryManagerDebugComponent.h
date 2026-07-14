#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"   // CgsDev::DebugComponent (base)
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"         // DebugUI::StringList

// BrnGameState::TriggerQueryManagerDebugComponent -- the in-game debug visualiser for the track
// TriggerData: it lets you toggle/filter the drawing of trigger regions (landmarks / blackspots /
// generic regions / VFX boxes), roaming & spawn locations and start positions, and drives the clip
// distances used when RenderWorld draws them into the 3D debug renderer. The TriggerQueryManager
// constructs/registers it (TriggerQueryManager::Prepare).
//
// SOURCE-OF-TRUTH: bodies reconstructed from BURNOUT_X360_ARTIST.XEX (offset+branch authority);
// class shape / member names / method list from the DecFIGS DWARF
// (BrnTriggerQueryManagerDebugComponent.h:59-100). Derives from CgsDev::DebugComponent.
//
// LAYOUT (members BY NAME + SEQUENCE; the X360 32-bit `this` offsets are noted for provenance and
// verified against the Construct/OnActivate asm -- every access is by member name). The base
// CgsDev::DebugComponent occupies this+0..11 on the console (vtable + mbActive +
// mpDebugLinkedListNext), so the first member below sits at console +0xC.

namespace BrnTrigger  { struct TriggerData; }

namespace BrnGameState
{
    class TriggerQueryManager;

    class TriggerQueryManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        TriggerQueryManagerDebugComponent() {}

        // X360 0x8236E908. Two-phase construct: base DebugComponent::Construct (the X360 image folds
        // the trivial base body onto the BaseCollisionGenerator::Destruct address), cache the manager
        // and its track TriggerData, default the clip distances / selectors / render flags, then build
        // the trigger-type and generic-region-type option tables.
        void Construct(TriggerQueryManager* lpTriggerQueryManager);

        // X360 0x82364E10. Draw every selected trigger region / roaming & spawn location / start
        // position into the 3D debug renderer, gated by the per-feature render flags. Declared here
        // because the X360 ledger attests it on this class (it is the class's overridden vtable slot);
        // its heavy VMX body is reconstructed in a follow-on wave.
        virtual void RenderWorld(CgsDev::Debug3DImmediateRender* lpRender);

        // X360 attests a Destruct on this class (DWARF cpp:112); the X360 inlines it and it is not in
        // this TU's function set -- declared-only, bodied in a follow-on wave.
        void Destruct();

    protected:
        // X360 0x82358788 / 0x822A9008. The debug-menu display name / path leaf for this component.
        virtual const char* GetName() const;
        virtual const char* GetPath() const;

        // X360 0x82358798. Register every render flag / selector / clip-distance slider with the debug
        // menu (options tables + ranges).
        virtual void OnActivate();

    private:
        // ===========================================================================================
        // FULL MEMBER LAYOUT (DWARF BrnTriggerQueryManagerDebugComponent.h:84-100). Console offsets
        // noted; verified against the Construct asm.
        // ===========================================================================================
        const BrnTrigger::TriggerData*  mpTriggerData;          // +0x0C (12)   track TriggerData (main-memory ptr)
        const TriggerQueryManager*      mpTriggerQueryManager;  // +0x10 (16)   the owning manager

        CgsDev::DebugUI::StringList maTriggerTypeNames[5];        // +0x14 (20)   trigger-type option table
        CgsDev::DebugUI::StringList maGenericRegionTypeNames[33]; // +0x3C (60)   generic-region-type option table

        f32  mfClipDistance;       // +0x144 (324)  region draw clip distance
        f32  mfTextClipDistance;   // +0x148 (328)  info-text draw clip distance

        s32  miTriggerType;        // +0x14C (332)  selected trigger-type filter (4 == All)
        s32  miGenericRegionType;  // +0x150 (336)  selected generic-region-type filter (32 == All)

        bool mbRenderTriggerRegions;   // +0x154 (340)
        bool mbRenderRoamingLocations; // +0x155 (341)
        bool mbRenderSpawnLocations;   // +0x156 (342)
        bool mbRenderStartPositions;   // +0x157 (343)
        bool mbRenderInfoText;         // +0x158 (344)
        bool mbRenderTriggerIds;       // +0x159 (345)
        bool mbRenderAxis;             // +0x15A (346)
    };
}
