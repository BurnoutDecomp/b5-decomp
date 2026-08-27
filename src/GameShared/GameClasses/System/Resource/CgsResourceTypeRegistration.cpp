#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistration.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"   // TypeRegistry::Register

#include "GameShared/GameClasses/RenderWare/CgsRwRasterResourceType.h"
#include "GameShared/GameClasses/RenderWare/CgsRwTextureStateResourceType.h"
#include "GameShared/GameClasses/RenderWare/CgsMaterialStateResourceType.h"
#include "GameShared/GameClasses/RenderWare/CgsRwColourCubeResourceType.h"   // CgsResource::RwColourCubeResourceType (0x2B)
#include "GameShared/GameClasses/Fonts/Resources/CgsFontResourceType.h"
#include "GameShared/GameClasses/Graphics/Resources/CgsVideoDataResource.h"
#include "GameShared/GameClasses/RenderWare/cross/CgsModelResourceType.h"
#include "GameShared/GameClasses/Graphics/Instances/CgsInstanceListResourceType.h"
#include "GameShared/GameClasses/System/Resource/CgsEntryListResource.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListResourceType.h"
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceHierarchyResourceType.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsSnrResourceType.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysSchemaResourceType.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultResourceType.h"   // CgsResource::AttribSysVaultResourceType (0x1C)
#include "GameShared/GameClasses/World/Resources/CgsWorldPainter2DResourceType.h"    // CgsResource::WorldPainter2DResourceType (0x30)
#include "GameShared/GameClasses/SceneManager/Zones/Resources/ZoneListResourceType.h" // CgsResource::ZoneListResourceType (0xB000)
#include "GameShared/GameClasses/System/Resource/CgsResourceIdListResourceType.h"     // CgsResource::IdListResourceType (0x25)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsAptDataHeaderType.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessageType.h"  // CgsResource::HudMessageResourceType (0x2C == 44)
#include "GameShared/GameClasses/Fsm/Resources/CgsLuaCodeResource.h"   // CgsResource::LuaCodeResourceType (0x22)
#include "GameShared/GameClasses/Language/Resources/CgsLanguageResourceType.h" // CgsResource::LanguageResourceType (0x27)
#include "GameShared/GameClasses/RenderWare/cross/CgsRwRenderableResourceType.h"          // 0xC
#include "GameShared/GameClasses/RenderWare/cross/CgsMaterialResourceType.h"              // 0x1
#include "GameShared/GameClasses/RenderWare/cross/CgsMaterialTechniqueResourceType.h"     // 0xD
#include "GameShared/GameClasses/Graphics/Resources/CgsShaderTechniqueResourceType.h"     // 0x32
#include "GameShared/GameClasses/RenderWare/x360/materialstates/CgsRwShaderProgramBufferResourceTypeX360.h" // 0x12
#include "SharedClasses/Physics/Props/BrnPropPhysicsListResourceType.h"        // Props::PropPhysicsResourceType (0x1000F)
#include "SharedClasses/Physics/Props/BrnPropGraphicsListResourceType.h"       // Props::PropGraphicsListResourceType (0x10010)
#include "SharedClasses/Physics/Props/BrnPropInstanceDataResourceType.h"       // Props::PropInstanceDataResourceType (0x10011)
#include "SharedClasses/Sound/World/BrnStaticSoundMapResourceType.h"           // World::StaticSoundMapResourceType (0x10016)
#include "SharedClasses/Gui/Flapt/BrnFlaptFileResourceType.h"                  // BrnFlapt::FlaptFileResourceType (0x10020)
#include "SharedClasses/Trigger/BrnTriggerResourceType.h"                      // BrnTrigger::TriggerResourceType (0x10003)
#include "SharedClasses/AI/AISectionsResourceType.h"                           // BrnAI::AISectionsResourceType (0x10001)
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"                  // BrnTraffic::TrafficDataResourceType (0x10002)
#include "SharedClasses/Traffic/BrnTrafficGraphicsStubResourceType.h"          // BrnTraffic::GraphicsStubResourceType (0x10015)
#include "SharedClasses/DataLists/VehicleListResourceType.h"                   // BrnResource::VehicleListResourceType (0x10005)
#include "SharedClasses/DataLists/WheelListResourceType.h"                     // BrnResource::WheelListResourceType (0x10009)
#include "SharedClasses/DataLists/ChallengeListResourceType.h"                 // BrnResource::ChallengeListResourceType (0x1001F)
#include "SharedClasses/World/BrnVehicleGraphicsSpecResourceType.h"            // BrnVehicle::GraphicsSpecResourceType (0x10006)
#include "SharedClasses/World/BrnWheelGraphicsSpecResourceType.h"              // BrnWheel::GraphicsSpecResourceType (0x1000A)
#include "SharedClasses/World/BrnEnvironmentKeyframeResourceType.h"       // EnvironmentSettings::KeyframeResourceType (0x10012)
#include "SharedClasses/World/BrnEnvironmentTimeLineResourceType.h"       // EnvironmentSettings::TimeLineResourceType (0x10013)
#include "SharedClasses/World/BrnEnvironmentDictionaryResourceType.h"     // EnvironmentSettings::DictionaryResourceType (0x10014)
#include "SharedClasses/Graphics/PlayerCarColoursResourceType.h"               // CgsResource::PlayerCarColoursResourceType (0x1001E)
#include "SharedClasses/Progression/BrnProgressionResourceType.h"              // BrnProgression::ProgressionResourceType (0x1000E)
#include "SharedClasses/Physics/Deformation/Resources/StreamedDeformationSpecResourceType.h" // BrnResource::StreamedDeformationSpecResourceType (0x1001C)
#include "SharedClasses/StreetData/BrnStreetDataResourceType.h"                // BrnStreetData::StreetDataResourceType (0x10018)
#include "GameShared/GameClasses/Containers/CgsDictionaryResourceType.h"       // CgsContainers::DictionaryResourceType<ICE::ICETakeData> (0x41)
#include "SDKs/Packages/ICE/ICEData.hpp"                                       // ICE::ICETakeData (the dictionary's element type)

// ============================================================================================
// Resource-type registration -- the counterpart of the X360
// BrnResource::GameDataModule::RegisterResourceTypes @0x82667EA8, which registers 76 types in
// one monolithic method (76 operator-new / InitCachedValues / vtable triples, 75 distinct name
// strings), storing each handler pointer and name into two parallel GameDataModule arrays.
// Only the handlers reconstructed so far are listed; the rest land in their game-order slot as
// each handler TU arrives.
//
// THREE DELIBERATE DEVIATIONS from the console:
//   1. Location. This lives engine-side (CgsResource), not in the game-layer GameDataModule,
//      because the early boot path brings resources online BEFORE GameDataModule::Prepare runs
//      (the debug-font bring-up and the GUI MovieManager both LoadBundle during Construct).
//      Routing that through a game-layer method would invert the engine->game layering.
//   2. Storage. The global CgsResource::TypeRegistry, which BundleLoader's ResolveResourceType
//      walks, instead of the GameDataModule's own handler/name arrays -- those and their
//      resolution path are a later reconstruction.
//   3. Mechanism. Static singletons instead of heap-new'd Type objects plus InitCachedValues.
//      Statics are boot-safe and need no allocator; cached-value init folds into each handler's
//      own ctor where it matters.
//
// EntryList and IdList are the only two the console registers outside the monolith, in their
// owning module's bring-up. They are folded in here as a bridge until that registration is
// reconstructed.
// ============================================================================================

namespace CgsResource
{
    void RegisterAllResourceTypes()
    {
        // Idempotent: the debug-font bring-up, the GUI module's movie-bundle load, and the
        // GameDataModule prepare all reach this; register the handler set exactly once.
        //
        // Every entry below is load-bearing. BundleLoader::LoadBundle gates all three fix-up
        // passes on `mpResourceType != 0`, so an unregistered type skips FixUp, import
        // resolution and PostFixUp, and CgsResource::Pool::CreateEntryInSlot stores a NULL
        // mpResourceType. Serialised offsets then stay offsets and imports stay null, usually
        // with no error beyond one "[bundle] UNREGISTERED resource type id N" line.
        static bool sbRegistered = false;
        if (sbRegistered)
            return;
        sbRegistered = true;

        // ---- in BrnResource::GameDataModule::RegisterResourceTypes order (0x82667EA8) --------
        static RwRasterResourceType        sRwRaster;          // [game #1]  0x00  Texture / RwRaster
        TypeRegistry::Register(&sRwRaster, "RwRaster");
        static RwTextureStateResourceType  sRwTextureState;    // [game #5]  0x0E  TextureState
        TypeRegistry::Register(&sRwTextureState, "RwTextureState");
        static MaterialStateResourceType   sMaterialState;     // [game #6]  0x0F  MaterialState / BlendState
        TypeRegistry::Register(&sMaterialState, "MaterialState");
        static FontResourceType            sFont;              // [game #16] 0x21  Font (imports rasters)
        TypeRegistry::Register(&sFont, "Font");
        static VoiceHierarchyResourceType  sVoiceHierarchy;    // [game #25] 0x18  VoiceHierarchy (sound)
        TypeRegistry::Register(&sVoiceHierarchy, "VoiceHierarchy");
        static SnrResourceType             sSnr;               // [game #27] 0x19  SNR (streamed audio)
        TypeRegistry::Register(&sSnr, "Snr");
        static VideoDataResourceType       sVideoData;         // [game #30] 0x42  VideoData (movie metadata)
        TypeRegistry::Register(&sVideoData, "VideoData");
        static CgsGraphics::InstanceListResourceType sInstanceList; // [game #32] 0x23  InstanceList (world instances)
        TypeRegistry::Register(&sInstanceList, "InstanceList");
        static AttribSysSchemaResourceType sAttribSysSchema;   // [game #36] 0x1B  AttribSys schema
        TypeRegistry::Register(&sAttribSysSchema, "AttribSysSchema");
        static AttribSysVaultResourceType  sAttribSysVault;    // [game #37] 0x1C  AttribSys vault (WORLDVAULT/SURFACELIST)
        TypeRegistry::Register(&sAttribSysVault, "AttribSysVault");
        static ModelResourceType           sModel;             // [game #38] 0x2A  Model (serialised model)
        TypeRegistry::Register(&sModel, "Model");
        // The console emits "RwColourCubeResourceType" in exactly this slot, after
        // "ModelResourceType" and before "WorldPainter2DResourceType". Without it nothing can
        // acquire PostFx/colourcubedictionary.bin and the composite's 3D tint has no LUT.
        static RwColourCubeResourceType    sRwColourCube;      // [game #39] 0x2B  ColourCube (3D tint LUT)
        TypeRegistry::Register(&sRwColourCube, "RwColourCube");
        static WorldPainter2DResourceType  sWorldPainter2D;    // [game #40] 0x30  WorldPainter2D (DISTRICTS.DAT map)
        TypeRegistry::Register(&sWorldPainter2D, "WorldPainter2D");
        static PolygonSoupListResourceType sPolygonSoupList;   // [game #71] 0x43  PolygonSoupList (collision)
        TypeRegistry::Register(&sPolygonSoupList, "PolygonSoupList");
        // PVS.BNDL's single resource. Unregistered, AllocateMemoryForResource null-derefs the
        // entry's mpResourceType in GetCachedCanDefrag.
        static ZoneListResourceType        sZoneList;          // 0xB000 ZoneList (PVS zone grid)
        TypeRegistry::Register(&sZoneList, "ZoneList");
        // WORLDCOL.BIN is 396 PolygonSoupLists (0x43, just above) paired with 396 IdLists --
        // the "TRK_CLIL<n>" per-zone lists WorldEntityModule::PrepareZoneCollision acquires.
        // Unregistered, FixUp is skipped, mpaIds stays the on-disk offset 0x10, and the first
        // acquire access-violates.
        static IdListResourceType          sIdList;            // 0x25 (37) IdList (zone-collision lists)
        TypeRegistry::Register(&sIdList, "IdList");
        // The four world-prop/sound types. The console registers all four, PropPhysics
        // immediately before PropGraphicsList; id-keyed lookup is order-independent.
        //
        // PropPhysics is the prop TYPE table, the game's single 0x1000F resource
        // (PROPS/PROPPHYSICS.BUNDLE). Unregistered, nothing calls PropPhysicsDataHeader::FixUp,
        // every mapPropTypes[] slot stays a raw file offset, and GetType hands every prop spawn
        // a garbage PropTypeData*. No props exist in the world without this.
        static BrnPhysics::Props::PropPhysicsResourceType      sPropPhysics;      // 0x1000F (65551)
        TypeRegistry::Register(&sPropPhysics, "PropPhysics");
        static BrnPhysics::Props::PropGraphicsListResourceType sPropGraphicsList; // 0x10010 (65552)
        TypeRegistry::Register(&sPropGraphicsList, "PropGraphicsList");
        static BrnPhysics::Props::PropInstanceDataResourceType sPropInstanceData; // 0x10011 (65553)
        TypeRegistry::Register(&sPropInstanceData, "PropInstanceData");
        static BrnSound::World::StaticSoundMapResourceType     sStaticSoundMap;   // 0x10016 (65558)
        TypeRegistry::Register(&sStaticSoundMap, "StaticSoundMap");
        // The DIRECTOR/world lane-data types. Each bundle's resource id is the CRC32 of the
        // lowercased name the GameDataModule GET handler hashes:
        //   AI.DAT         -> id 0xA8CD78D4 == HashString("WorldMapData") type 65537 (0x10001)
        //   B5TRAFFIC.BNDL -> id 0xC43359DA == HashString("BaseTraffic")  type 65538 (0x10002)
        //   TRIGGERS.DAT   -> id 0xE4A32837 == HashString("TriggerData")  type 65539 (0x10003)
        // The payloads are transcoded to platform 4 with 64-bit pointer slots by
        // tools/assets/bundles/lane_transcode.py, so each FixUp relocates real host pointers.
        static BrnAI::AISectionsResourceType          sAISectionsData; // 0x10001 (65537)
        TypeRegistry::Register(&sAISectionsData, "AISectionsData");
        static BrnTrigger::TriggerResourceType        sTriggerData;  // 0x10003 (65539)
        TypeRegistry::Register(&sTriggerData, "TriggerData");
        static BrnTraffic::TrafficDataResourceType    sTrafficData;  // 0x10002 (65538)
        TypeRegistry::Register(&sTrafficData, "TrafficData");
        // ---- the TRAFFIC CAR graphics stub ---------------------------------------------------
        // GraphicsStub (0x10015 / 65557): the resource each of the 42 traffic
        // VEHICLES/VEH_T<code>_GR.BIN carries instead of a BrnVehicle::GraphicsSpec, a pair of
        // import slots naming the body and wheel graphics that car reuses. This is the console's
        // own slot -- RegisterResourceTypes @0x82667EA8 emits "GraphicsStubResourceType"
        // immediately after "TrafficDataResourceType" and before "LoopModelResourceType".
        //
        // Every meaningful byte of this type is an import, so an unregistered type does not
        // merely skip FixUp: the stub loads as two serialised-null words and
        // TrafficCarStreamer::GetGraphicsSpec / ::GetWheelGraphicsSpec hand the renderer a null
        // body spec and a null wheel spec -- an invisible traffic car, with no error anywhere.
        // Measured over all 568 retail VEH_*/WHE_* graphics bundles: exactly 42 carry a 65557
        // resource, the 42 T-prefixed traffic bundles, each with importCount 2 and import
        // offsets {0x0, 0x4}.
        static BrnTraffic::GraphicsStubResourceType   sTrafficGraphicsStub;  // 0x10015 (65557)
        TypeRegistry::Register(&sTrafficGraphicsStub, "TrafficGraphicsStub");

        // ---- the vehicle/wheel LIST types ----------------------------------------------------
        // GameDataModule::Prepare stages 9 and 12 stream Vehicles/VehicleList.bundle and
        // Wheels/WheelList.bundle into pool 5, then acquire "B5VehicleList" / "B5WheelList"
        // (shipped resource ids 0x1521E14B and 0xC2D08298, the hashes of those two names).
        // Both carry a serialised 32-bit entry-array slot that only FixUp rebases, so without a
        // handler every entry lookup reads through an un-relocated offset.
        static BrnResource::VehicleListResourceType   sVehicleList;  // 0x10005 (65541)
        TypeRegistry::Register(&sVehicleList, "VehicleList");
        static BrnResource::WheelListResourceType     sWheelList;    // 0x10009 (65545)
        TypeRegistry::Register(&sWheelList, "WheelList");
        // [challenge-list wave 2026-08-27] The THIRD member of the same family, and it is
        // load-bearing for exactly the same reason: GameDataModule::Prepare stage 10
        // (PrepareFreeburnChallengeList @0x8266C088) streams "OnlineChallenges.bndl" into
        // POOL 26 and acquires "B5ChallengeList". MEASURED against the shipped
        // build/game/ONLINECHALLENGES.BNDL: one resource, id 0x0D82D720 ==
        // HashString("B5ChallengeList"), type 0x1001F (65567) -- and with no registered
        // handler the pool stores a NULL mpResourceType, skips FixUp, and every one of the
        // 458 challenge records is then reached through an un-rebased 32-bit offset.
        static BrnResource::ChallengeListResourceType sChallengeList; // 0x1001F (65567)
        TypeRegistry::Register(&sChallengeList, "ChallengeList");
        // The per-car graphics bundle's own spec resource. Measured over
        // build/game/VEHICLES/VEH_PUSMC01_GR.BIN (275 resources): its type set is
        // {0, 1, 10, 12, 13, 14, 15, 42, 65542}, and 65542 was the only one without a handler.
        // Type 10 (VertexDescriptor) is also unregistered, but the shipped TRK_UNIT world
        // bundles carry 7-10 each and load fine on the null-type path, so that is not a
        // vehicle-specific gap.
        static BrnVehicle::GraphicsSpecResourceType   sVehicleGraphicsSpec;  // 0x10006 (65542)
        TypeRegistry::Register(&sVehicleGraphicsSpec, "VehicleGraphicsSpec");
        // ---- the streamed DEFORMATION spec ---------------------------------------------------
        // StreamedDeformationSpec (0x1001C / 65564), the deformation resource inside each
        // Vehicles\VEH_*_AT.bin. Registering it routes the load through
        // StreamedDeformationSpec::FixUp; without it TransformToNewCOMSpace's first table walk
        // dereferences an un-rebased serialised offset. The shipped _AT payload is platform 4.
        static BrnResource::StreamedDeformationSpecResourceType sStreamedDeformationSpec; // 0x1001C (65564)
        TypeRegistry::Register(&sStreamedDeformationSpec, "StreamedDeformationSpec");
        // The WHEEL graphics spec, the twin of the gap above. Measured over the shipped
        // build/game/WHEELS/WHE_51916650_GR.BNDL (48 resources): the single 0x1000A resource
        // (id 0xB289585E) carries two imports, at +0x04 and +0x08, both targeting type-42
        // CgsGraphics::Model resources in the same bundle. +0x04 is the wheel model
        // RenderRaceCar's wheel block reads (`*(spec + 4)`); unregistered it stays
        // serialised-null and the wheel has no model. The handler's FixUp does no relocation,
        // only a spec-version assert, so registering it is import resolution, not layout risk.
        static BrnWheel::GraphicsSpecResourceType     sWheelGraphicsSpec;    // 0x1000A (65546)
        TypeRegistry::Register(&sWheelGraphicsSpec, "WheelGraphicsSpec");
        // ---- the ICE take dictionary ----------------------------------------------------------
        // GameDataModule::PrepareICEList streams Cameras.bundle into pool 5 and acquires
        // "StandardICETakes" (id 0x0DC0EE8F), type 0x41. Every pointer slot in a serialised
        // dictionary -- mpaIndex and each entry's mpData -- is a resource-relative offset on
        // disk that only DictionaryResourceType<T>::FixUp turns into an address, so without a
        // handler ICEList::GetICETakeData{,FromGuid} dereferences offsets as pointers on every
        // lookup. Measured on build/game/CAMERAS.BUNDLE: miNumEntries 549, mpaIndex 0x10,
        // entry[0].mpData 0x3388.
        static CgsContainers::DictionaryResourceType<ICE::ICETakeData> sICETakeDictionary; // 0x41 (65)
        TypeRegistry::Register(&sICETakeDictionary, "ICETakeDictionary");
        // ---- the player-car colour palette ----------------------------------------------------
        // PlayerCarColours (0x1001E / 65566), the second resource inside VEHICLELIST.BUNDLE.
        // Without a handler the "CarColours" acquire returns a null memory pointer and
        // WorldDataController::GetColourPaletteFromType hands out `&null->maPalettes[type]`,
        // which is an empty car-livery colour picker. Both pointer columns stay 32-bit
        // serialised slots -- see the banner in SharedClasses/Graphics/BrnGlobalColourPalette.h.
        static PlayerCarColoursResourceType sPlayerCarColours;   // 0x1001E (65566)
        TypeRegistry::Register(&sPlayerCarColours, "PlayerCarColours");
        // ---- the offline progression resource -------------------------------------------------
        // PROGRESSION.DAT / BTTPROGRESSION.DAT carry exactly one resource: id 0x988F38C0 ==
        // HashString("ProgressionData"), type 0x1000E (65550), which
        // ProgressionManager::LoadProgressionData @0x82399ED0 acquires from pool 5. Every table
        // base in the payload is a serialised 32-bit offset that only ProgressionData::FixUp
        // rebases, so without a handler the acquire hands the manager a live-looking record
        // whose nine array bases are still file offsets.
        static BrnProgression::ProgressionResourceType sProgressionData;   // 0x1000E (65550)
        TypeRegistry::Register(&sProgressionData, "ProgressionData");

        // ---- the road / street table ----------------------------------------------------------
        // STREETDATA.DAT carries exactly one resource: id 0xBC9CC502 == HashString("StreetData"),
        // type 0x10018 (65560), which StreetManager::LoadStreetData @0x8234F630 acquires from
        // pool 5 and StreetDataResourceType::GetTypeID @0x82676798 returns. mpaStreets,
        // mpaJunctions, mpaRoads and mpaChallengeParScores are serialised 32-bit offsets that
        // only StreetData::FixUp rebases.
        static BrnStreetData::StreetDataResourceType  sStreetData;         // 0x10018 (65560)
        TypeRegistry::Register(&sStreetData, "StreetData");

        // ---- the ENVIRONMENT-SETTINGS resource family ------------------------------------------
        // Keyframe (0x10012), TimeLine (0x10013) and Dictionary (0x10014), the three types in
        // build/game/ENVIRONMENTSETTINGS. Unregistering bites twice here: the TimeLine's
        // mpLocationDatii / mpfKeyframeTimes / mppKeyframes and the Dictionary's mpSeasonDatii /
        // mpLocationDatii stay serialised offsets, AND the timeline's nine keyframe imports
        // (measured on ENVIRONMENTSETTINGS/PARADISE_INGAME_JUNK.BUNDLE.x360: 9 entries at
        // payload +0x20..+0x40, one per city_HHMM keyframe) are never resolved, so every
        // mppKeyframes slot stays null and the time-of-day blend has nothing to read.
        static BrnWorld::EnvironmentSettings::KeyframeResourceType   sEnvKeyframe;   // 0x10012 (65554)
        TypeRegistry::Register(&sEnvKeyframe, "EnvKeyframe");
        static BrnWorld::EnvironmentSettings::TimeLineResourceType   sEnvTimeLine;   // 0x10013 (65555)
        TypeRegistry::Register(&sEnvTimeLine, "EnvTimeLine");
        static BrnWorld::EnvironmentSettings::DictionaryResourceType sEnvDictionary; // 0x10014 (65556)
        TypeRegistry::Register(&sEnvDictionary, "EnvDictionary");

        // ---- the world-render resource types ---------------------------------------------------
        // The streamed TRK_UNIT bundles carry these. Without handlers the loader cannot FixUp
        // them and the renderable mesh/material graph is never built.
        static RwRenderableResourceType    sRwRenderable;      // 0xC   Renderable (mesh graph)
        TypeRegistry::Register(&sRwRenderable, "RwRenderable");
        static MaterialResourceType        sMaterial;          // 0x1   Material (assembly)
        TypeRegistry::Register(&sMaterial, "Material");
        static MaterialTechniqueResourceType sMaterialTechnique; // 0xD MaterialTechnique
        TypeRegistry::Register(&sMaterialTechnique, "MaterialTechnique");
        static ShaderTechniqueResourceType sShaderTechnique;       // 0x32 ShaderTechnique
        TypeRegistry::Register(&sShaderTechnique, "ShaderTechnique");
        static RwShaderProgramBufferResourceType sShaderProgramBuffer;    // 0x12 vertex/pixel program
        TypeRegistry::Register(&sShaderProgramBuffer, "ShaderProgramBuffer");

        // ---- owning-module registrations (NOT in GameDataModule::RegisterResourceTypes) -------
        // PARK: the console registers these in their owning subsystem's bring-up. They are
        // folded in here as a bridge until that registration is reconstructed.
        static EntryListResourceType       sEntryList;         // 0x1D  EntryList (resource subsystem)
        TypeRegistry::Register(&sEntryList, "EntryList");
        static AptDataHeaderType           sAptDataHeader;     // 0x1E  AptData (GUI/Flash movie header)
        TypeRegistry::Register(&sAptDataHeader, "AptDataHeader");
        // HUD messages, type 44 (0x2C). Handler virtuals at 0x82846578 / 0x8267B110 /
        // 0x828465D8 / 0x828465E8. Unregistered, the acquire completes with an un-relocated
        // pointer table and GameDataModule::PrepareHudMessages refuses to bind. The shipped
        // build/game/HUDMESSAGES.HM carries `2c 00 00 00` as its resource type at 0x68.
        static HudMessageResourceType      sHudMessage;        // 0x2C  HudMessage (HUDMESSAGES.HM)
        TypeRegistry::Register(&sHudMessage, "HudMessage");
        static LuaCodeResourceType         sLuaCode;           // 0x22  LuaCode (FSM scripts; loaded by the GUI flow)
        TypeRegistry::Register(&sLuaCode, "LuaCode");
        static LanguageResourceType        sLanguage;          // 0x27  Language (localised string table)
        TypeRegistry::Register(&sLanguage, "Language");
        static BrnFlapt::FlaptFileResourceType sFlaptFile;     // 0x10020 FLApt (GUI-owned vendor movie)
        TypeRegistry::Register(&sFlaptFile, "FlaptFile");
    }
}
