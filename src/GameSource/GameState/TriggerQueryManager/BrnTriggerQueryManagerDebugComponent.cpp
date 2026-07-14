#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManagerDebugComponent.h"

#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"  // TriggerQueryManager::GetTriggerData
#include "SharedClasses/Trigger/BrnGenericRegion.h"                           // GenericRegion type-name table

// BrnGameState::TriggerQueryManagerDebugComponent method bodies. Reconstructed from the X360 ARTIST
// build (offset+branch authority) against the DecFIGS DWARF class shape.
//
//   Construct   @ 0x8236E908  (this TU)
//   GetName     @ 0x82358788  (this TU)
//   GetPath     @ 0x822A9008  (this TU)
//   OnActivate  @ 0x82358798  (this TU)
//   RenderWorld @ 0x82364E10  -- heavy VMX body, follow-on wave (declared in the header)
//   Destruct    @ DWARF cpp:112 -- X360-inlined, follow-on wave (declared in the header)

namespace BrnGameState
{
    // =============================================================================================
    // Construct  @ 0x8236E908
    //
    // Two-phase init: base DebugComponent::Construct (the X360 image folds the trivial base body onto
    // the BaseCollisionGenerator::Destruct address), cache the manager + its track TriggerData, default
    // the render flags / clip distances / type filters, then build the two enum-style option tables.
    // The trigger-type table is {Landmark, Blackspot, GenericRegion, VFXBoxRegion, All}; the
    // generic-region-type table mirrors GenericRegion::KAPC_GENERIC_REGION_TYPE_STRINGS plus a final
    // "All" entry (miValue == E_TYPE_COUNT).
    // =============================================================================================
    void TriggerQueryManagerDebugComponent::Construct(TriggerQueryManager* lpTriggerQueryManager)
    {
        CgsDev::DebugComponent::Construct();   // base two-phase init (X360 folded body)

        mpTriggerQueryManager = lpTriggerQueryManager;
        mpTriggerData         = lpTriggerQueryManager->GetTriggerData();

        mbRenderTriggerRegions   = true;
        mfClipDistance           = 500.0f;
        mbRenderInfoText         = true;
        mfTextClipDistance       = 100.0f;
        mbRenderRoamingLocations = false;
        mbRenderSpawnLocations   = false;
        mbRenderStartPositions   = false;
        mbRenderTriggerIds       = false;
        mbRenderAxis             = false;

        maTriggerTypeNames[0].miValue = 0; maTriggerTypeNames[0].mpcName = "Landmark";
        maTriggerTypeNames[1].miValue = 1; maTriggerTypeNames[1].mpcName = "Blackspot";
        maTriggerTypeNames[2].miValue = 2; maTriggerTypeNames[2].mpcName = "GenericRegion";
        maTriggerTypeNames[3].miValue = 3; maTriggerTypeNames[3].mpcName = "VFXBoxRegion";
        maTriggerTypeNames[4].miValue = 4; maTriggerTypeNames[4].mpcName = "All";

        for (s32 li = 0; li < BrnTrigger::GenericRegion::E_TYPE_COUNT; ++li)
        {
            maGenericRegionTypeNames[li].miValue = li;
            maGenericRegionTypeNames[li].mpcName =
                BrnTrigger::GenericRegion::KAPC_GENERIC_REGION_TYPE_STRINGS[li];
        }
        maGenericRegionTypeNames[BrnTrigger::GenericRegion::E_TYPE_COUNT].miValue = BrnTrigger::GenericRegion::E_TYPE_COUNT;
        maGenericRegionTypeNames[BrnTrigger::GenericRegion::E_TYPE_COUNT].mpcName  = "All";

        miTriggerType       = 4;
        miGenericRegionType = BrnTrigger::GenericRegion::E_TYPE_COUNT;
    }

    // =============================================================================================
    // GetName  @ 0x82358788
    // =============================================================================================
    const char* TriggerQueryManagerDebugComponent::GetName() const
    {
        return "Trigger Regions";
    }

    // =============================================================================================
    // GetPath  @ 0x822A9008
    // =============================================================================================
    const char* TriggerQueryManagerDebugComponent::GetPath() const
    {
        return "Triggers";
    }

    // =============================================================================================
    // OnActivate  @ 0x82358798
    //
    // Register every render flag / type filter / clip-distance slider with the debug menu. The two
    // type filters carry their option tables (SetOptions) plus an index range (SetRange 0..N).
    // =============================================================================================
    void TriggerQueryManagerDebugComponent::OnActivate()
    {
        RegisterVariable(&mbRenderTriggerRegions, "Render Trigger Regions");

        RegisterVariable(&miTriggerType, "Render trigger types");
        SetRange(&miTriggerType, 0, 4);
        SetOptions(&miTriggerType, maTriggerTypeNames);

        RegisterVariable(&miGenericRegionType, "Render generic region types");
        SetRange(&miGenericRegionType, 0, 32);
        SetOptions(&miGenericRegionType, maGenericRegionTypeNames);

        RegisterVariable(&mbRenderRoamingLocations, "Render Roaming Locations");
        RegisterVariable(&mbRenderSpawnLocations,   "Render Spawn Locations");
        RegisterVariable(&mbRenderStartPositions,   "Render Start Positions");
        RegisterVariable(&mbRenderInfoText,         "Render Info Text");
        RegisterVariable(&mbRenderTriggerIds,       "Render Trigger Ids");
        RegisterVariable(&mbRenderAxis,             "Render Axis");

        RegisterVariable(&mfClipDistance,     "Clip distance");
        RegisterVariable(&mfTextClipDistance, "Text clip distance");
        SetRange(&mfClipDistance,     10.0f, 10000.0f);
        SetRange(&mfTextClipDistance, 10.0f, 10000.0f);
    }
}
