#ifndef BRN_WORLD_WORLD_ENTITY_MODULE_H
#define BRN_WORLD_WORLD_ENTITY_MODULE_H

// =============================================================================
// BrnWorld::WorldEntityModule
//   GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Declaration shape from the DecFIGS DWARF (BrnWorldEntityModule.h:72), gated on
// the X360 ledger; Feb-2007 consulted for idiom only.
//
// The world entity module is the world-streaming producer: each PreScene update
// it asks the PVS which zones surround the camera/player, rebuilds the graphics
// streamer's target list ("TRK_UNIT%d" ids -> the GameData request pipeline),
// mirrors zone load/unload completions into prop/sound events and the backdrop
// stand-in entities, and drives the collision-world validate/invalidate
// protocol ("TRK_CLIL%d" poly-soup zone lists + WORLDCOL swap requests).
//
// X360 object map used throughout the .cpp (32-bit offsets, listed for
// cross-reference only -- the PC build accesses every member BY NAME):
//   +0x228 mePrepareStage        +0x22C meReleaseStage
//   +0x230 meInitialLoadStage    +0x234 meWorldColPrepareStage
//   +0x238 meZoneColPrepareStage +0x23C meMassivePrepareStage
//   +0x240 mRenderHandle         +0x248 mReceiverQueue (1024,16)
//   +0x660 mPVSModule            +0x2620 mPlayerZoneResponse (736B)
//   +0x2900 mPVSDebugComponent   +0x4950 mCollisionDebugComponent
//   +0x4990 mWorldGraphicsStreamer (maInstanceLists @ +0x19FC inside)
//   +0x6790 mMassive             +0x7138 massive debug block
//   +0x7148 maiNumInstancesLoadedPerZone[32]  +0x71C8 miNumInstanceListsLoaded
//   +0x71CC miPlayerZoneNumber   +0x71D0 miPreviousPlayerZoneNumber
//   +0x71D4 mbBackdropLoaded     +0x71D8 miCurrentBackdropZoneId
//   +0x71DC mbWaitingForStreaming +0x71DD mbUseCarForPvs
//   +0x71E0 mPositionUsedForPVS  +0x71F0/1 mbDebugTriggerValidate/Invalidate
//   +0x71F4 mSurfaceList         +0x7204 meSurfaceListPrepareStage
//   +0x7208 mpTempCachePreSceneOutput  +0x720C meCollisionWorldState
//   +0x7210 meValidationStage    +0x7214 meInvalidationStage
//   +0x7218 maZoneCollisionHandles[20]
//   +0x72B8 miNumCollisonZonesLoaded +0x72BC miNumZonesInWorld
//   +0x72C0 miCollisionZoneBatchSize +0x72C4 mabWasCollisionZoneAdded[1024]
//   +0x76C4 mbOverrideLod +0x76C8 miLodOverrideValue +0x76CC mbDrawBoundingSpheres
//   +0x76CD mbOverrideLodDistances +0x76D0 mauOverrideLodDistances[3]
//   +0x76DC miEnvironmentMapLOD  +0x76E8 mapDynamicAdvertTextures[128]
//   +0x78E8 mIsBackdropInstanceInScene (FastBitArray<1024>)
//   +0x7968 maiZonesLoadedThisFrame[32] +0x79A8 maiZonesUnloadedThisFrame[32]
//   +0x79E8 muNumZonesLoadedThisFrame   +0x79EC muNumZonesUnloadedThisFrame
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                       // Vector3/Matrix44/Matrix44Affine typedefs
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "SharedClasses/BrnSharedConstants.h"      // BrnUpdateSet
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"     // EventReceiverQueue<N,A>
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"
#include "GameSource/World/EntityModules/WorldEntityModule/PVSModule/BrnPVSModule.h"
#include "GameSource/World/EntityModules/WorldEntityModule/PVSModule/SharedIO/BrnPVSModuleEvents.h"
#include "GameSource/World/DebugComponents/BrnPVSDebugComponent.h"
#include "GameSource/World/DebugComponents/BrnCollisionDebugComponent.h"
#include "GameSource/World/BrnWorldGraphicsStreamer.h"
#include "GameSource/Massive/BrnMassive.h"
#include "GameSource/World/BrnShaderLodInfo.h"
#include "GameSource/AttribSys/Generated/classes/surfacelist.h"
#include "GameSource/AttribSys/Generated/classes/surface.h"

namespace CgsGraphics
{
    struct Instance;       // pointer-only use here (backdrop instance walk)
    class DispatchFrame;   // pointer-only use (dispatch-list generation); class matches CgsDispatcher.h:211 (mangling)
}

namespace BrnWorld
{

// DWARF BrnWorldEntityModule.h:48/53/55.
static const s32 KI_MAX_COLLISION_MESHES_PER_ZONE      = 1;
static const s32 KI_NUM_COLLISION_ZONES_LOADED_PER_FRAME = 20;
static const s32 KU_MAX_NUM_DYNAMIC_ADVERTS            = 128;

class WorldEntityModule : public CgsModule::ModuleSingleBuffered
{
public:
    // DWARF BrnWorldEntityModule.h:75. (The X360 Prepare body no longer visits
    // E_PREPARESTAGE_COLLISION -- world collision moved to the validate protocol
    // driven from PostPhysicsUpdate -- but the stage id remains in the enum.)
    enum EPrepareStage
    {
        E_PREPARESTAGE_START,
        E_PREPARESTAGE_MANAGER,
        E_PREPARESTAGE_PVS,
        E_PREPARESTAGE_COLLISION,
        E_PREPARESTAGE_SURFACELIST,
        E_PREPARESTAGE_MASSIVE,
        E_PREPARESTAGE_COMMONDATA,
        E_PREPARESTAGE_COMMONDATA_LOADING,
        E_PREPARESTAGE_ACQUIRE_RESOURCES,
        E_PREPARESTAGE_DONE
    };

    // DWARF BrnWorldEntityModule.h:90. (The X360 Release body falls straight
    // from MANAGER into MASSIVE; E_RELEASESTAGE_RESOURCES is not visited.)
    enum EReleaseStage
    {
        E_RELEASESTAGE_START,
        E_RELEASESTAGE_PVS,
        E_RELEASESTAGE_MANAGER,
        E_RELEASESTAGE_RESOURCES,
        E_RELEASESTAGE_MASSIVE,
        E_RELEASESTAGE_DONE
    };

    // DWARF BrnWorldEntityModule.h:100.
    enum EInitialLoadStage
    {
        E_INITIALLOADSTAGE_START,
        E_INITIALLOADSTAGE_LOADBUNDLES,
        E_INITIALLOADSTAGE_WFLOAD,
        E_INITIALLOADSTAGE_DONE
    };

    // DWARF BrnWorldEntityModule.h:109.
    enum EWorldColPrepareStage
    {
        E_WORLDCOL_PREPARESTAGE_START,
        E_WORLDCOL_PREPARESTAGE_LOADBUNDLE,
        E_WORLDCOL_PREPARESTAGE_PREPARING_ZONES,
        E_WORLDCOL_PREPARESTAGE_DONE
    };

    // DWARF BrnWorldEntityModule.h:118.
    enum EZoneColPrepareStage
    {
        E_ZONECOL_PREPARESTAGE_START,
        E_ZONECOL_PREPARESTAGE_ACQUIRING_MESHES,
        E_ZONECOL_PREPARESTAGE_DONE
    };

    // DWARF BrnWorldEntityModule.h:126.
    enum ESurfaceListPrepareStage
    {
        E_SURFACELIST_PREPARESTAGE_START,
        E_SURFACELIST_PREPARESTAGE_LOADING,
        E_SURFACELIST_PREPARESTAGE_DONE
    };

    // DWARF BrnWorldEntityModule.h:133.
    enum ECollisionWorldState
    {
        E_COLWORLDSTATE_INVALID,
        E_COLWORLDSTATE_VALIDATING,
        E_COLWORLDSTATE_INVALIDATING,
        E_COLWORLDSTATE_VALID
    };

    // DWARF BrnWorldEntityModule.h:141.
    enum ECollisionWorldValidationStage
    {
        E_COLWORLDVALIDATIONSTAGE_START,
        E_COLWORLDVALIDATIONSTAGE_VALIDATE_WORLD,
        E_COLWORLDVALIDATIONSTAGE_WF_VALIDATION,
        E_COLWORLDVALIDATIONSTAGE_PREPARING_COLLISION,
        E_COLWORLDVALIDATIONSTAGE_DONE
    };

    // DWARF BrnWorldEntityModule.h:150.
    enum ECollisionWorldInvalidationStage
    {
        E_COLWORLDINVALIDATIONSTAGE_START,
        E_COLWORLDINVALIDATIONSTAGE_REMOVE_FROM_SCENE,
        E_COLWORLDINVALIDATIONSTAGE_INVALIDATE_WORLD,
        E_COLWORLDINVALIDATIONSTAGE_WF_INVALIDATION,
        E_COLWORLDINVALIDATIONSTAGE_DONE
    };

    // DWARF BrnWorldEntityModule.h:159.
    enum EMassivePrepareStage
    {
        E_EMASSIVEPREPARESTAGE_START,
        E_LOADBUNDLES,
        E_WLOADBUNDLES,
        E_REQUESTTEXTURES,
        E_WREQUESTTEXTURES,
        E_REQUESTMASSIVETABLE,
        E_WREQUESTMASSIVETABLE,
        E_EMASSIVEPREPARESTAGE_DONE
    };

    typedef CgsModule::EventReceiverQueue<1024, 16> ReceiverQueue;

    // ---- module lifecycle -------------------------------------------------
    void Construct( void );                                              // @0x82302398
    bool Prepare( WorldEntityIO::OutputBuffer_Prepare* lpOutputBuffer ); // @0x823027D0
    bool Release( void );                                                // @0x822A8498
    void Destruct( void );                                               // @0x822A8590

    // ---- per-frame updates ------------------------------------------------
    void PreSceneUpdate(                                                 // @0x82302A08
        const WorldEntityIO::InputBuffer_PreScene* lpInputBuffer,
        WorldEntityIO::OutputBuffer_PreScene* lpOutputBuffer,
        Vector3::InParam lSimulatedCameraPosition,
        BrnUpdateSet lUpdateSet );

    void PostPhysicsUpdate(                                              // @0x823080F0
        const WorldEntityIO::InputBuffer_PostPhysics* lpInputBuffer,
        WorldEntityIO::OutputBuffer_PostPhysics* lpOutputBuffer,
        BrnUpdateSet lUpdateSet );

    // ---- dispatch-list generation (render feed) --------------------------
    void GenerateDispatchLists(                                          // @0x822D5AB0
        const WorldEntityIO::InputBuffer_GenerateDispatchLists* lpInputBuffer,
        const Array<CgsSceneManager::EntityId, 4500u>& lrVisibleEntities,
        Matrix44::InParam lCameraViewProjection,
        Vector3::InParam lCameraPosition,
        Vector3::InParam lCameraDirection,
        f32 lfDrawDistanceScale,
        const ShaderLodInfo* lpShaderLodInfo,
        s32 liList,
        s32 liSortLayer,
        s32 liSortKey,
        s32 liPreZList,
        bool lbGenerateShadows );

    void GenerateDispatchListsForEnvironmentMap(                         // @0x822D7298
        const WorldEntityIO::InputBuffer_GenerateDispatchLists* lpInputBuffer,
        const Array<CgsSceneManager::EntityId, 4500u>& lrVisibleEntities,
        Matrix44::InParam lCameraViewProjection,
        Vector3::InParam lCameraPosition,
        const ShaderLodInfo* lpShaderLodInfo,
        s32 liList,
        s32 liSortLayer,
        s32 liSortKey );

    // ---- massive (in-game advertising) slice: bodied by its own class TU --
    bool PrepareMassive( WorldEntityIO::OutputBuffer_Prepare* lpOutputBuffer );
    void UpdateMassive( BrnUpdateSet lUpdateSet );
    void GenerateMassiveImpressionData( CgsGraphics::Instance* lpInstance,
                                        Vector3::InParam lCameraPosition );

    // DWARF cpp:134 -- the shared per-instance dispatch path (inlined by the
    // X360 into both GenerateDispatchLists loops; de-inlined here).
    void RenderInstance(
        CgsGraphics::Instance* lpInstance,
        bool lbShadow,
        Vector3::InParam lCameraPosition,
        f32 lfScaledDistanceSq,
        s32 liList,
        s32 liSortLayer,
        s32 liSortKey,
        CgsGraphics::DispatchFrame* lpDispatchFrame,
        const ShaderLodInfo* lpShaderLodInfo );

    // DWARF BrnWorldEntityModule.h:632. Inline surface lookup by surface id.
    void GetSurface( u8 luSurfaceId, Attrib::Gen::surface* lpOutSurface ) const;

    // DWARF BrnWorldEntityModule.cpp:2369.
    static void DebugMemoryInit( WorldEntityModule* lpData );

private:
    static const s32 KU_NUM_RENDERABLES = 2048;   // DWARF :288
    static const s32 KI_NUM_LODS        = 3;      // DWARF :289

    // ---- prepare/release helpers -----------------------------------------
    void BridgePVSToOutput_Prepare( WorldEntityIO::OutputBuffer_Prepare* lpOutputBuffer ); // @0x822F9EC8

    // ---- streaming --------------------------------------------------------
    void UpdateStream( WorldEntityIO::OutputBuffer_PreScene* lpOutputBuffer );             // @0x822F9740
    s32  QueryWorldGraphicsLoad( const StreamerTargetEntry* lpPotentialList,               // @0x822A87B0
                                 s32 liPotentialListLength );
    s32  QueryWorldGraphicsUnload( const StreamerTargetEntry* lpPotentialList,
                                   s32 liPotentialListLength );
    void OnWorldGraphicsLoadComplete( const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent, // @0x822D7828
                                      s32 liIndex );
    void OnWorldGraphicsUnloadComplete( const BrnResource::GameDataIO::UnloadGameDataResponse* lpEvent );
    void OnWorldGraphicsLoadBegin( s32 liListIndex );
    void OnWorldGraphicsUnloadBegin( s32 liListIndex );                                    // @0x822D7CB0

    // ---- backdrop stand-in entities ---------------------------------------
    void LoadBackdropForZone( s32 liZone );                              // @0x822EDBC8
    void UnloadBackdropForZone( s32 liZone );                            // @0x822D7E10
    void UpdateBackdropSceneEntities( s32 liZoneNumber, bool lbZoneLoaded, // @0x822D8730
                                      WorldEntityIO::OutputBuffer_PreScene* lpOutputBuffer );
    void AddBackdropEntity( CgsGraphics::Instance* lpInstance, s32 liListIndex, // @0x822D8380
                            s32 liInstanceIndex,
                            WorldEntityIO::OutputBuffer_PreScene* lpOutputBuffer );
    void RemoveBackdropEntity( s32 liListIndex, s32 liInstanceIndex,     // @0x822C34B8
                               WorldEntityIO::OutputBuffer_PreScene* lpOutputBuffer );

    // ---- collision world ---------------------------------------------------
    void ProcessValidationRequests( const WorldEntityIO::RequestInterface* lpRequestInterface ); // @0x822A85E0
    void UpdateCollisionValidation( WorldEntityIO::OutputBuffer_PostPhysics* lpOutputBuffer );   // @0x82307FC0
    bool PrepareWorldCollision(                                          // @0x823068F8
        WorldEntityIO::ResourceRequestInterface* lpRequestInterface,
        WorldEntityIO::SceneInputInterface* lpSceneInterface,
        bool lbLoadBundle = true );
    bool PrepareZoneCollision(                                           // @0x82302C38
        WorldEntityIO::ResourceRequestInterface* lpRequestInterface,
        WorldEntityIO::SceneInputInterface* lpSceneInterface );
    bool PrepareSurfaceList( WorldEntityIO::OutputBuffer_Prepare* lpOutputBuffer ); // @0x822F9B70
    bool AddCollisionZoneToSceneManager(                                 // @0x822D8130
        WorldEntityIO::ResourceRequestInterface* lpRequestInterface,
        WorldEntityIO::SceneInputInterface* lpSceneInterface,
        CgsResource::ResourceHandle* lpaMeshHandles,
        s32 liNumHandles );
    bool ValidateCollision(                                              // @0x82306A48
        WorldEntityIO::ResourceRequestInterface* lpRequestInterface,
        WorldEntityIO::SceneInputInterface* lpSceneInterface );
    bool InvalidateCollision(                                            // @0x822F9D78
        WorldEntityIO::ResourceRequestInterface* lpRequestInterface,
        WorldEntityIO::SceneInputInterface* lpSceneInterface );

    void HandleExternalRequests( const WorldEntityIO::InputBuffer_PostPhysics* lpInput ); // @0x822D82C0

    // ---- inline queries ----------------------------------------------------
    s32 GetTotalZones( void );          // DWARF :583
    s32 GetPlayerZone() const;          // DWARF :612

    // ---- members (DWARF order; see the X360 offset map in the file header) --
    EPrepareStage       mePrepareStage;                 // :291
    EReleaseStage       meReleaseStage;                 // :292
    EInitialLoadStage   meInitialLoadStage;             // :293
    EWorldColPrepareStage meWorldColPrepareStage;       // :294
    EZoneColPrepareStage  meZoneColPrepareStage;        // :295
    EMassivePrepareStage  meMassivePrepareStage;        // :296

    CgsResource::ResourceHandle mRenderHandle;          // :298

    ReceiverQueue       mReceiverQueue;                 // :300

    PVSModule           mPVSModule;                     // :302
    PVSIO::GetZoneResponse mPlayerZoneResponse;         // :303

    PVSDebugComponent       mPVSDebugComponent;         // :305
    CollisionDebugComponent mCollisionDebugComponent;   // :306

    WorldGraphicsStreamer mWorldGraphicsStreamer;       // :309

    // X360-attested addition over the Feb-2007 shape: the embedded Massive
    // (in-game advertising) client, constructed/destructed with the module
    // (@0x6790 in the X360 object).
    BrnMassive::BrnMassive mMassive;

    // Massive debug block (X360 Construct debug registrations, "World/Massive").
    f32  mfMassiveImpressionDebugDistance;              // = 150.0f
    s32  miGenerateDispatchListsPM;                     // :371
    s32  miMassiveFrameCounter;                         // X360 +0x7140, zeroed at Construct
    bool mbMassiveGenerateImpressionData;               // = true  ("Massive Generate Impression Data")
    bool mbMassiveDistanceLinesDebug;                   // = false ("Massive Distance Lines Debug")
    bool mbMassive3dDebug;                              // = false ("Massive 3D Debug")
    bool mbDownloadMassiveTextures;                     // = true  ("Download Massive Textures")

    s32  maiNumInstancesLoadedPerZone[32];              // :323
    s32  miNumInstanceListsLoaded;                      // :324

    s32  miPlayerZoneNumber;                            // :326
    s32  miPreviousPlayerZoneNumber;                    // :327
    bool mbBackdropLoaded;                              // :328
    s32  miCurrentBackdropZoneId;                       // :330

    bool mbWaitingForStreaming;                         // :332
    bool mbUseCarForPvs;                                // :333

    Vector3 mPositionUsedForPVS;              // :335

    bool mbDebugTriggerValidate;                        // :337
    bool mbDebugTriggerInvalidate;                      // :338

    Attrib::Gen::surfacelist mSurfaceList;              // :341
    ESurfaceListPrepareStage meSurfaceListPrepareStage; // :342

    WorldEntityIO::OutputBuffer_PreScene* mpTempCachePreSceneOutput; // :344

    ECollisionWorldState             meCollisionWorldState; // :346
    ECollisionWorldValidationStage   meValidationStage;     // :347
    ECollisionWorldInvalidationStage meInvalidationStage;   // :348

    CgsResource::ResourceHandle maZoneCollisionHandles[ KI_NUM_COLLISION_ZONES_LOADED_PER_FRAME *
                                                        KI_MAX_COLLISION_MESHES_PER_ZONE ]; // :355

    s32  miNumCollisonZonesLoaded;                      // :357
    s32  miNumZonesInWorld;                             // :358
    s32  miCollisionZoneBatchSize;                      // :359
    bool mabWasCollisionZoneAdded[1024];                // :360

    bool mbOverrideLod;                                 // :364
    s32  miLodOverrideValue;                            // :365
    bool mbDrawBoundingSpheres;                         // :366
    bool mbOverrideLodDistances;                        // :367
    s32  mauOverrideLodDistances[KI_NUM_LODS];          // :368
    s32  miEnvironmentMapLOD;                           // :369

    // DWARF :373. Dynamic advert textures resolved by the Massive slice.
    void* mapDynamicAdvertTextures[KU_MAX_NUM_DYNAMIC_ADVERTS];

    CgsContainers::FastBitArray<1024> mIsBackdropInstanceInScene; // :375

    s16  maiZonesLoadedThisFrame[32];                   // :378
    s16  maiZonesUnloadedThisFrame[32];                 // :379
    u32  muNumZonesLoadedThisFrame;                     // :380
    u32  muNumZonesUnloadedThisFrame;                   // :381

    // DWARF BrnWorldEntityModule.cpp:109 -- file-static in the .cpp.
    // (declared there as `_mbAllowStreamStalling`).

    friend class PVSDebugComponent;
    friend class CollisionDebugComponent;
    friend class WorldGraphicsStreamer;
    friend class WorldDebugComponent;
};

// -----------------------------------------------------------------------------
// Inline queries (DWARF :583 / :612; bodies from the Feb-2007 header, whose
// inlined shapes the X360 callers reproduce verbatim).
// -----------------------------------------------------------------------------
inline s32
WorldEntityModule::GetTotalZones( void )
{
    u32 luNumZones = 0;

    mPVSModule.LockForOutput();
    luNumZones = mPVSModule.GetOutputInterface()->GetTotalZones();
    mPVSModule.UnlockForOutput();

    return static_cast<s32>( luNumZones );
}

inline s32
WorldEntityModule::GetPlayerZone() const
{
    return miPlayerZoneNumber;
}

} // namespace BrnWorld

#endif // BRN_WORLD_WORLD_ENTITY_MODULE_H
