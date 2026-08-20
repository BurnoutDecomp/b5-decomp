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
#include "SharedClasses/DataLists/VehicleListResourceType.h"                   // BrnResource::VehicleListResourceType (0x10005)
#include "SharedClasses/DataLists/WheelListResourceType.h"                     // BrnResource::WheelListResourceType (0x10009)
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
// Resource-type registration -- the faithful counterpart of the X360
// BrnResource::GameDataModule::RegisterResourceTypes (0x82667EA8).
//
// HOW THE GAME DOES IT (0x82667EA8). The X360 registers its resource-type handlers in ONE
// monolithic GameDataModule method: for each of ~75 types it heap-allocates a CgsResource::Type
// (operator new), stamps the handler's vtable, calls Type::InitCachedValues, and stores the
// handler pointer + its name string into two parallel arrays owned by the GameDataModule
// (a handler table at this+0x60E8C-ish and a name table at this+0x60E0C), bumping a running
// counter. Resolution then indexes those module arrays.
//
// HOW WE DO IT, AND WHY IT DEVIATES. This stand-in instead installs each handler as a
// function-local static into the global CgsResource::TypeRegistry (the table BundleLoader's
// ResolveResourceType walks). Three deliberate, documented deviations:
//   1. *Location*: the real function is a game-layer BrnResource::GameDataModule method. This
//      lives engine-side (CgsResource) because the early boot path brings resources online
//      BEFORE GameDataModule::Prepare runs -- the debug-font bring-up and the GUI MovieManager
//      both LoadBundle (Default.font / VIDEOLIST.BUNDLE) during Construct, so they need the
//      handlers registered without a prepared game-layer module. Routing that through a
//      game-layer method would invert the engine->game layering.
//   2. *Storage*: global TypeRegistry vs. the GameDataModule's own handler/name arrays. The
//      arrays + their resolution path are a later reconstruction; until then BundleLoader
//      resolves through TypeRegistry, so we register there.
//   3. *Mechanism*: static singletons vs. heap-new'd Type objects + InitCachedValues. Static
//      instances are boot-safe and need no allocator; the cached-value init is folded into each
//      handler's own ctor where it matters.
//
// WHY IT IS ONE FUNCTION (and was not). This was previously split into a per-handler
// Register<Name>ResourceType() helper in each handler .cpp, gathered here by name. That spread
// registration across a dozen files and did NOT mirror the game, whose registration is a single
// function. The helpers have been removed and their bodies inlined below, in the GAME'S OWN
// REGISTRATION ORDER, so this reads as the one monolith the original source was -- minus the
// three deviations above. The handler .cpp files now only define behaviour, not registration.
//
// INCREMENTAL. The game registers SEVENTY-SIX types (⭐ census corrected 2026-08-16, boot audit
// F-P7-18/F-P7-19: 76 operator-new/InitCachedValues/vtable triples in RegisterResourceTypes
// @0x82667EA8, 75 distinct name strings); only the handlers reconstructed so far are listed.
// The rest are added here, in their game-order slot, as each handler TU lands.
//
// ⭐ AND THE "NOT IN THE MONOLITH" CLAIM WAS WRONG for three of the four types it named
// (F-P7-19). AptDataHeader, LuaCode, Language and FlaptFile ARE all in the console's
// RegisterResourceTypes -- census sequence numbers 13, 18, 30 and 57 -- and treating them as
// outside it drifted every ordinal after Font by one. Only EntryList and IdList are genuinely
// registered outside the monolith, in their owning module's bring-up (GUI / resource subsystem),
// not the game-data module. They are folded in here as a temporary bridge until those modules'
// registration is reconstructed; see the note at their call sites.
// ============================================================================================

namespace CgsResource
{
    void RegisterAllResourceTypes()
    {
        // Idempotent: the debug-font bring-up, the GUI module's movie-bundle load, and the
        // GameDataModule prepare all reach this; register the handler set exactly once.
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
        // Post-fx colour-cube wave (2026-08-16). The X360 registers this handler in exactly this
        // slot -- BrnResource::GameDataModule::RegisterResourceTypes @0x82667EA8 emits
        // "RwColourCubeResourceType" immediately after "ModelResourceType" and before
        // "WorldPainter2DResourceType" -- and without it the loader refuses every colour cube in
        // the game:
        //   [bundle] UNREGISTERED resource type id 43 in 'PostFx/colourcubedictionary.bin'
        //     -- scratch/postfx_step9_final/boot1_BrnGame.log:387
        // Same failure mode ZoneList / PlayerCarColours / IdList were added for: CreateEntryInSlot
        // stores a NULL mpResourceType for the entry and nothing can acquire it, so the composite's
        // 3D tint has no LUT to blend.
        static RwColourCubeResourceType    sRwColourCube;      // [game #39] 0x2B  ColourCube (3D tint LUT)
        TypeRegistry::Register(&sRwColourCube, "RwColourCube");
        static WorldPainter2DResourceType  sWorldPainter2D;    // [game #40] 0x30  WorldPainter2D (DISTRICTS.DAT map)
        TypeRegistry::Register(&sWorldPainter2D, "WorldPainter2D");
        static PolygonSoupListResourceType sPolygonSoupList;   // [game #71] 0x43  PolygonSoupList (collision)
        TypeRegistry::Register(&sPolygonSoupList, "PolygonSoupList");
        // PVS wave (2026-07-27): PVS.BNDL's single resource is type 45056 (0xB000). With no
        // registered handler CgsResource::Pool::CreateEntryInSlot stores a NULL mpResourceType
        // and AllocateMemoryForResource null-derefs it (GetCachedCanDefrag).
        static ZoneListResourceType        sZoneList;          // 0xB000 ZoneList (PVS zone grid)
        TypeRegistry::Register(&sZoneList, "ZoneList");
        // World-collision wave (2026-08-10). WORLDCOL.BIN is 396 PolygonSoupLists (0x43,
        // registered just above) PAIRED with 396 IdLists (0x25) -- the "TRK_CLIL<n>" per-zone
        // lists WorldEntityModule::PrepareZoneCollision acquires. With no handler the loader
        // logged "[bundle] UNREGISTERED resource type id 37 in 'worldcol.bin'" and skipped
        // FixUp, so mpaIds stayed the on-disk offset 0x10 and the first acquire AV'd. Same
        // failure mode ZoneList and PlayerCarColours were added for; the type existed, it was
        // just a partial hollow shell (GetTypeID/GetSerialisedResourceDescriptor unbodied) so
        // it could not be instantiated here. Both are bodied now.
        static IdListResourceType          sIdList;            // 0x25 (37) IdList (zone-collision lists)
        TypeRegistry::Register(&sIdList, "IdList");
        // The four world-prop/sound types (X360 GameDataModule::RegisterResourceTypes
        // @0x82667EA8 registers all four, PropPhysics immediately before PropGraphicsList;
        // id-keyed lookup is order-independent).
        //
        // PropPhysics is the prop TYPE table -- the game's single 0x1000F resource,
        // PROPS/PROPPHYSICS.BUNDLE. Without a registered handler
        // CgsResource::Pool::CreateEntryInSlot stores a NULL mpResourceType for that entry
        // and AllocateMemoryForResource null-derefs it (GetCachedCanDefrag) -- the same
        // failure mode ZoneList and PlayerCarColours were added for. Even surviving that,
        // nothing would call PropPhysicsDataHeader::FixUp, so every mapPropTypes[] slot
        // would stay a raw file offset and PropPhysicsDataHeader::GetType would hand every
        // prop spawn a garbage PropTypeData*. No props exist in the world without this.
        static BrnPhysics::Props::PropPhysicsResourceType      sPropPhysics;      // 0x1000F (65551)
        TypeRegistry::Register(&sPropPhysics, "PropPhysics");
        static BrnPhysics::Props::PropGraphicsListResourceType sPropGraphicsList; // 0x10010 (65552)
        TypeRegistry::Register(&sPropGraphicsList, "PropGraphicsList");
        static BrnPhysics::Props::PropInstanceDataResourceType sPropInstanceData; // 0x10011 (65553)
        TypeRegistry::Register(&sPropInstanceData, "PropInstanceData");
        static BrnSound::World::StaticSoundMapResourceType     sStaticSoundMap;   // 0x10016 (65558)
        TypeRegistry::Register(&sStaticSoundMap, "StaticSoundMap");
        // The DIRECTOR/world lane-data types (traffic-lane fetch wave, 2026-07-29). Same
        // reason ZoneList had to be added for the PVS wave: the lane bundles' single
        // resources carry these type ids, and with no registered handler
        // CgsResource::Pool::CreateEntryInSlot stores a NULL mpResourceType and
        // AllocateMemoryForResource null-derefs it (GetCachedCanDefrag). Each bundle's id is
        // the CRC32-lowercase of the resource name the GameDataModule GET handler hashes:
        //   AI.DAT         -> id 0xA8CD78D4 == HashString("WorldMapData") type 65537 (0x10001)
        //   B5TRAFFIC.BNDL -> id 0xC43359DA == HashString("BaseTraffic")  type 65538 (0x10002)
        //   TRIGGERS.DAT   -> id 0xE4A32837 == HashString("TriggerData")  type 65539 (0x10003)
        // All three are registered as of the lane-data widening wave (2026-07-29): the seven
        // missing Fix* bodies are reconstructed, and the payloads are transcoded to platform 4
        // with 64-bit pointer slots by tools/assets/bundles/lane_transcode.py, so each type's
        // FixUp now relocates real host pointers.
        static BrnAI::AISectionsResourceType          sAISectionsData; // 0x10001 (65537)
        TypeRegistry::Register(&sAISectionsData, "AISectionsData");
        static BrnTrigger::TriggerResourceType        sTriggerData;  // 0x10003 (65539)
        TypeRegistry::Register(&sTriggerData, "TriggerData");
        static BrnTraffic::TrafficDataResourceType    sTrafficData;  // 0x10002 (65538)
        TypeRegistry::Register(&sTrafficData, "TrafficData");

        // ---- the vehicle/wheel LIST types (vehicle-load wave, 2026-07-31) --------------------
        // GameDataModule::Prepare stages 9 and 12 stream Vehicles/VehicleList.bundle and
        // Wheels/WheelList.bundle into pool 5 and then acquire "B5VehicleList" / "B5WheelList".
        // Both resources carry a serialised 32-bit entry-array slot that ONLY FixUp rebases, so
        // without a registered handler the pool stores a NULL mpResourceType, the loader skips
        // FixUp, and every entry lookup reads through an un-relocated offset. (Measured: the
        // shipped VEHICLELIST.BUNDLE resource id 0x1521E14B == HashString("B5VehicleList") and
        // WHEELLIST.BUNDLE's 0xC2D08298 == HashString("B5WheelList") -- the same names the two
        // Prepare stages hash, so this is the right pair.)
        static BrnResource::VehicleListResourceType   sVehicleList;  // 0x10005 (65541)
        TypeRegistry::Register(&sVehicleList, "VehicleList");
        static BrnResource::WheelListResourceType     sWheelList;    // 0x10009 (65545)
        TypeRegistry::Register(&sWheelList, "WheelList");
        // The per-car graphics bundle's own spec resource. MEASURED over
        // build/game/VEHICLES/VEH_PUSMC01_GR.BIN (275 resources): its type set is
        // {0, 1, 10, 12, 13, 14, 15, 42, 65542}, and every one of those already had a
        // registered handler EXCEPT 65542 -- so this single registration is what a
        // Vehicles\VEH_*_GR.bin load was missing. (Type 10 == VertexDescriptor is also
        // unregistered, but the shipped TRK_UNIT world bundles carry 7-10 of them each and
        // load fine on the documented null-type path, so it is not a vehicle-specific gap.)
        static BrnVehicle::GraphicsSpecResourceType   sVehicleGraphicsSpec;  // 0x10006 (65542)
        TypeRegistry::Register(&sVehicleGraphicsSpec, "VehicleGraphicsSpec");
        // ---- the streamed DEFORMATION spec (deformation-mount wave, 2026-08-14) --------------
        // StreamedDeformationSpec (0x1001C / 65564), the deformation resource inside each
        // Vehicles\VEH_*_AT.bin. The handler TU has existed since the spec landed; it was NEVER
        // REGISTERED, which was invisible for as long as the deformation manager was unmounted --
        // the loader logged "[bundle] UNREGISTERED resource type id 65564 in
        // 'Vehicles\VEH_PUSMC01_AT.bin'" and SKIPPED FixUp on every boot. The moment the mount
        // landed and ProcessAddDeformationModelEvents ran the resolved spec for real, the first
        // table walk (TransformToNewCOMSpace) dereferenced an un-rebased serialised offset and
        // AV'd READING 0x6D0 (boot-measured, 2026-08-14 04:27 run). Registering the handler
        // routes the load through StreamedDeformationSpec::FixUp exactly as the IdList/ICE/
        // PlayerCarColours precedents above. (The shipped _AT payload is platform 4 / ported --
        // bnd2 +8 == 04, byte-checked.)
        static BrnResource::StreamedDeformationSpecResourceType sStreamedDeformationSpec; // 0x1001C (65564)
        TypeRegistry::Register(&sStreamedDeformationSpec, "StreamedDeformationSpec");
        // The WHEEL graphics spec -- the exact twin of the gap above, and LOAD-BEARING for the
        // same reason the vehicle one was. BundleLoader::LoadBundle gates ALL THREE fix-up
        // passes on `mpResourceType != 0`, so an unregistered type does not merely skip FixUp:
        // it skips ResolveImportsForEntry too. MEASURED over the shipped
        // build/game/WHEELS/WHE_51916650_GR.BNDL (48 resources): the single 0x1000A resource
        // (id 0xB289585E) carries TWO imports, at +0x04 and +0x08, and both target type-42
        // CgsGraphics::Model resources inside the same bundle -- +0x04 being the wheel model
        // RenderRaceCar's wheel block reads (`*(spec + 4)`). Unregistered, that slot stays
        // serialised-null and the wheel has no model at all. The handler's own FixUp performs
        // NO relocation (it only asserts the spec version), and the shipped payload's version
        // word reads 1, so registering it is import resolution, not layout risk.
        static BrnWheel::GraphicsSpecResourceType     sWheelGraphicsSpec;    // 0x1000A (65546)
        TypeRegistry::Register(&sWheelGraphicsSpec, "WheelGraphicsSpec");
        // ---- the ICE take dictionary (ICE take-runtime wave, 2026-08-01) --------------------
        // GameDataModule::PrepareICEList streams Cameras.bundle into pool 5 and acquires
        // "StandardICETakes" (id 0x0DC0EE8F == HashString("StandardICETakes")), whose type is
        // 0x41. Same reason as the two list types above, and it BITES here: every pointer slot
        // in a serialised dictionary -- mpaIndex and each of the 549 entries' mpData -- is a
        // resource-relative OFFSET on disk, and only DictionaryResourceType<T>::FixUp turns
        // them into addresses. Without a registered handler the pool stores a NULL
        // mpResourceType, the loader skips FixUp, and ICEList::GetICETakeData{,FromGuid} would
        // dereference offsets as pointers on every single lookup. (Measured on the shipped
        // build/game/CAMERAS.BUNDLE: miNumEntries 549, mpaIndex 0x10, entry[0].mpData 0x3388.)
        static CgsContainers::DictionaryResourceType<ICE::ICETakeData> sICETakeDictionary; // 0x41 (65)
        TypeRegistry::Register(&sICETakeDictionary, "ICETakeDictionary");
        // ---- the player-car colour palette (colour-picker wave, 2026-08-02) ------------------
        // PlayerCarColours (0x1001E / 65566), the SECOND resource inside VEHICLELIST.BUNDLE.
        // This registration is the whole reason the car-livery colour picker was empty: with
        // no handler the pool took the null-type path (CgsResourcePool.cpp
        // AllocateMemoryForResource), the loader logged "[bundle] UNREGISTERED resource type id
        // 65566 in 'Vehicles/VehicleList.bundle'" and SKIPPED FixUp, so the "CarColours"
        // acquire came back with a NULL memory pointer (BrnGame.log "carColours=0") and
        // WorldDataController::GetColourPaletteFromType handed out `&null->maPalettes[type]`.
        // The handler itself was already reconstructed; it simply did not compile for x64
        // because it spelled its load-base truncations as `reinterpret_cast<u32>(pointer)`.
        // That is fixed (it now uses the shared CgsResource::GetLoadBase, like VehicleList),
        // its GetTypeID is defined, and BOTH pointer columns stay 32-bit serialised slots --
        // see the two-proof banner in SharedClasses/Graphics/BrnGlobalColourPalette.h.
        static PlayerCarColoursResourceType sPlayerCarColours;   // 0x1001E (65566)
        TypeRegistry::Register(&sPlayerCarColours, "PlayerCarColours");
        // ---- the offline progression resource (progression-load wave, 2026-08-11) ------------
        // PROGRESSION.DAT / BTTPROGRESSION.DAT carry exactly one resource: id 0x988F38C0 ==
        // HashString("ProgressionData"), type 0x1000E (65550) -- the name+id
        // ProgressionManager::LoadProgressionData @0x82399ED0 loads the bundle for and then
        // acquires from pool 5. MEASURED on the shipped build/game/PROGRESSION.DAT (bnd2 v2,
        // platform 4, 1 resource, type 65550, id 0x988F38C0).
        // LOAD-BEARING for the same reason ZoneList/IdList/PlayerCarColours were: EVERY table base
        // in the payload is a serialised 32-bit offset that only ProgressionData::FixUp rebases,
        // so with no registered handler the pool stores a NULL mpResourceType, BundleLoader skips
        // all three fix-up passes, and the acquire hands the ProgressionManager a live-looking
        // record whose nine array bases are still file offsets.
        static BrnProgression::ProgressionResourceType sProgressionData;   // 0x1000E (65550)
        TypeRegistry::Register(&sProgressionData, "ProgressionData");

        // ---- the road / street table (street-data load wave, 2026-08-11) ---------------------
        // STREETDATA.DAT carries exactly one resource: id 0xBC9CC502 == HashString("StreetData"),
        // type 0x10018 (65560) -- the name+id StreetManager::LoadStreetData @0x8234F630 loads the
        // bundle for and then acquires from pool 5. MEASURED on the shipped
        // build/game/STREETDATA.DAT (bnd2 v2, platform 4, 1 resource, type 65560, id 0xBC9CC502),
        // and 65560 is exactly what BrnStreetData::StreetDataResourceType::GetTypeID @0x82676798
        // returns.
        // LOAD-BEARING for the same reason ProgressionData was: mpaStreets / mpaJunctions /
        // mpaRoads / mpaChallengeParScores are serialised 32-bit offsets that only
        // StreetData::FixUp rebases, so with no registered handler the pool stores a NULL
        // mpResourceType, BundleLoader skips the fix-up passes, and the acquire hands the
        // StreetManager a live-looking record whose table bases are still file offsets.
        static BrnStreetData::StreetDataResourceType  sStreetData;         // 0x10018 (65560)
        TypeRegistry::Register(&sStreetData, "StreetData");

        // ---- the ENVIRONMENT-SETTINGS resource family (env wave, 2026-08-16) ------------------
        // Keyframe (0x10012), TimeLine (0x10013) and Dictionary (0x10014) -- the three types in
        // build/game/ENVIRONMENTSETTINGS. All three handler TUs have existed since the post-fx
        // rung-5 wave and NONE was ever registered, which is why EnvironmentManager::Prepare had
        // to be stubbed inert: BundleLoader::LoadBundle gates ALL THREE fix-up passes on
        // `mpResourceType != 0`, so an unregistered type skips FixUp AND ResolveImportsForEntry
        // (the exact trap the wheel-graphics-spec note above records). For this family that is
        // fatal twice over: the TimeLine's mpLocationDatii / mpfKeyframeTimes / mppKeyframes and
        // the Dictionary's mpSeasonDatii / mpLocationDatii stay serialised offsets, and the nine
        // keyframe imports the timeline carries (MEASURED on the shipped
        // ENVIRONMENTSETTINGS/PARADISE_INGAME_JUNK.BUNDLE.x360: 9 entries at payload +0x20..+0x40,
        // one per city_HHMM keyframe) are never resolved, so every mppKeyframes slot stays NULL
        // and the time-of-day blend has nothing to read.
        static BrnWorld::EnvironmentSettings::KeyframeResourceType   sEnvKeyframe;   // 0x10012 (65554)
        TypeRegistry::Register(&sEnvKeyframe, "EnvKeyframe");
        static BrnWorld::EnvironmentSettings::TimeLineResourceType   sEnvTimeLine;   // 0x10013 (65555)
        TypeRegistry::Register(&sEnvTimeLine, "EnvTimeLine");
        static BrnWorld::EnvironmentSettings::DictionaryResourceType sEnvDictionary; // 0x10014 (65556)
        TypeRegistry::Register(&sEnvDictionary, "EnvDictionary");

        // ---- the world-render resource types (2026-07-27) -------------------------------------
        // The streamed TRK_UNIT bundles carry these; without a registered handler the
        // loader cannot FixUp them, so the renderable mesh/material graph is never built
        // (found by the renderer wave: nothing registered Renderable/Material/technique).
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
        // The X360 registers these in their owning subsystem's bring-up, not the game-data module.
        // Folded in here as a temporary bridge until that registration is reconstructed.
        static EntryListResourceType       sEntryList;         // 0x1D  EntryList (resource subsystem)
        TypeRegistry::Register(&sEntryList, "EntryList");
        static AptDataHeaderType           sAptDataHeader;     // 0x1E  AptData (GUI/Flash movie header)
        TypeRegistry::Register(&sAptDataHeader, "AptDataHeader");
        // ⭐ [gateui r4] CE-2. HUD messages, type 44 (0x2C) -- the handler existed
        // (CgsGuiHudMessageType.cpp, all four virtuals reconstructed off
        // 0x82846578/0x8267B110/0x828465D8/0x828465E8) but was NEVER REGISTERED, so
        // TypeRegistry::GetType(44) answered null, CgsResourceBundleLoader.cpp's
        // `mpResourceType != 0` guards (:195/:251/:258/:266) skipped every FixUp / import /
        // PostFixUp pass, and the acquire completed with an UN-RELOCATED pointer table --
        // which is why GameDataModule::PrepareHudMessages had to refuse to bind. Verified
        // against the shipped bundle: `build/game/HUDMESSAGES.HM` carries `2c 00 00 00` as
        // its resource type at 0x68 (verify_r3_fix3hud §1.4, re-measured byte for byte).
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
