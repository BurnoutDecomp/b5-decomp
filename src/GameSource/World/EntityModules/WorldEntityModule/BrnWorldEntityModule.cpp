// =============================================================================
// BrnWorld::WorldEntityModule -- GameSource/World/EntityModules/WorldEntityModule/
//                                BrnWorldEntityModule.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Function addresses are noted per body. Declaration shape from the DecFIGS
// DWARF; Feb-2007 used for idiom/inlining shapes only.
//
// This TU is the world-streaming producer for the open world:
//   * PreSceneUpdate asks the PVS which zones surround the camera/player and
//     rebuilds the graphics streamer's target list ("TRK_UNIT%d" GameData ids).
//   * The streamer callbacks mirror zone load/unload into prop + sound events
//     and swap the backdrop stand-in entities in and out of the scene manager.
//   * PostPhysicsUpdate drives the collision-world validate/invalidate protocol
//     ("TRK_CLIL%d" poly-soup zone lists + SwapIn/SwapOutCollisionWorld).
//   * GenerateDispatchLists feeds the visible world instances to the renderer.
//
// DWARF-declared members NOT emitted by the X360 build (left out per the
// X360-ledger gate): OnEnterNewZone (cpp:1887 -- its loops live on in
// Load/UnloadBackdropForZone, whose assert text still names it),
// DebugMemoryInit (cpp:2369).
// =============================================================================

#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"
#include "GameShared/GameClasses/Graphics/CgsModel.h"
#include "GameShared/GameClasses/Graphics/Instances/CgsInstance.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"
#include "GameShared/GameClasses/Graphics/Dispatch/Renderable.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"
#include "GameSource/Resource/SharedIO/BrnAssetIds.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/World/ShadowMap/BrnShadowMap.h"
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"

// The global runtime shader-constant register (X360 symbol mShaderConstantTable;
// bodied by the CgsShaderConstants TU). Slot 0 holds the per-draw world transform.
namespace CgsGraphics { extern ShaderConstantTable mShaderConstantTable; }

namespace BrnWorld
{

// BrnWorldEntityModule.cpp:109 (DWARF: WorldEntityModule::_mbAllowStreamStalling).
// X360 byte_82CDB588. When set, PostPhysicsUpdate only reports the world as
// streamed once the player + required zones are resident.
static bool _mbAllowStreamStalling = true;

// X360 dword_82FAD9B4 -- UpdateStream's warm-up counter: the stream does not run
// for the first few frames after boot (static, file scope).
static s32 siUpdateStreamFrameCounter = 0;

// X360 dword_82FAD308 -- debug: the track-unit index currently being rendered
// (read by the instance-render debug path).
static s32 siCurrentTrackUnitIndex = 0;

// X360 byte_82CDB6B8 -- debug gate for the per-instance bounding-circle draw.
static bool sbDrawInstanceCircles = false;

// Scene-manager entity owner id for world instances (the X360 EntityId::Set owner 5).
static const u32 KU_ENTITY_OWNER_WORLD = 5;

// X360 rodata flt_82014460: the surface-list sanity threshold PrepareSurfaceList
// compares the probed first attribute against.
static const f32 KF_SURFACE_SANITY_THRESHOLD = 10000.0f;

// Maximum axis scale of an affine transform (the X360 inline: max of the three
// basis-row magnitudes via the vrsqrte refinement pipeline).
static f32 GetMaximumScale( const Matrix44Affine& lrTransform )
{
    f32 lfScale = rw::math::vpu::Magnitude( lrTransform.Right() );
    const f32 lfUp = rw::math::vpu::Magnitude( lrTransform.Up() );
    const f32 lfAt = rw::math::vpu::Magnitude( lrTransform.At() );
    if ( lfUp > lfScale ) lfScale = lfUp;
    if ( lfAt > lfScale ) lfScale = lfAt;
    return lfScale;
}

// =============================================================================
// Construct  @ 0x82302398
// =============================================================================
void
WorldEntityModule::Construct( void )
{
    CgsModule::ModuleSingleBuffered::Construct();

    mePrepareStage = E_PREPARESTAGE_START;
    meReleaseStage = E_RELEASESTAGE_DONE;
    meInitialLoadStage = E_INITIALLOADSTAGE_START;

    meWorldColPrepareStage = E_WORLDCOL_PREPARESTAGE_START;
    meZoneColPrepareStage = E_ZONECOL_PREPARESTAGE_START;
    meMassivePrepareStage = E_EMASSIVEPREPARESTAGE_START;

    mReceiverQueue.Construct();
    mPVSModule.Construct();

    mPVSDebugComponent.Construct( this );
    mPVSDebugComponent.Register();

    mCollisionDebugComponent.Construct( this );
    mCollisionDebugComponent.Register();

    mWorldGraphicsStreamer.Construct( this );

    mMassive.Construct();
    mbMassive3dDebug = false;
    mbMassiveDistanceLinesDebug = false;
    miGenerateDispatchListsPM = 0;
    miMassiveFrameCounter = 0;
    mfMassiveImpressionDebugDistance = 150.0f;
    mbMassiveGenerateImpressionData = true;
    mbDownloadMassiveTextures = true;

    miNumCollisonZonesLoaded = 0;
    miNumZonesInWorld = 0;

    miCurrentBackdropZoneId = -1;
    miPlayerZoneNumber = 0;
    miPreviousPlayerZoneNumber = -1;
    mbBackdropLoaded = false;
    miCollisionZoneBatchSize = 0;

    miEnvironmentMapLOD = 2;

    mbOverrideLod = false;
    miLodOverrideValue = 0;
    mbDrawBoundingSpheres = false;
    mbOverrideLodDistances = false;
    muNumZonesLoadedThisFrame = 0;
    muNumZonesUnloadedThisFrame = 0;

    for ( s32 liI = 0; liI < KI_NUM_LODS; liI++ )
    {
        mauOverrideLodDistances[ liI ] = 300 * ( liI + 1 );
    }

    mbWaitingForStreaming = false;
    mbUseCarForPvs = false;

    mPositionUsedForPVS.SetZero();

    for ( s32 liI = 0; liI < 32; liI++ )
    {
        maiNumInstancesLoadedPerZone[ liI ] = -1;
    }

    miNumInstanceListsLoaded = -1;

    meCollisionWorldState = E_COLWORLDSTATE_INVALID;
    meSurfaceListPrepareStage = E_SURFACELIST_PREPARESTAGE_START;

    mbDebugTriggerValidate = false;
    mbDebugTriggerInvalidate = false;

    for ( s32 liI = 0; liI < 1024; liI++ )
    {
        mabWasCollisionZoneAdded[ liI ] = false;
    }

    {
        CgsDev::DebugInterface lDebugInterface;

        lDebugInterface.RegisterVariable( &mbOverrideLod, "World/LODs", "Override Instance LOD" );
        lDebugInterface.RegisterVariable( &mbDrawBoundingSpheres, "World", "Draw instance bounding spheres" );
        lDebugInterface.RegisterVariable( &miLodOverrideValue, "World/LODs", "LOD Instance number" );
        lDebugInterface.SetRange( &miLodOverrideValue, 0, 15 );

        lDebugInterface.RegisterVariable( &mbOverrideLodDistances, "World/LODs", "OverrideDistances" );
        lDebugInterface.RegisterVariable( &miEnvironmentMapLOD, "World/LODs", "Environment Map LOD" );
        lDebugInterface.SetRange( &miEnvironmentMapLOD, 0, 2 );

        lDebugInterface.RegisterVariable( &mauOverrideLodDistances[0], "World/LODs", "LOD0Distance" );
        lDebugInterface.RegisterVariable( &mauOverrideLodDistances[1], "World/LODs", "LOD1Distance" );
        lDebugInterface.RegisterVariable( &mauOverrideLodDistances[2], "World/LODs", "LOD2Distance" );

        for ( s32 liI = 0; liI < KI_NUM_LODS; liI++ )
        {
            lDebugInterface.SetRange( &mauOverrideLodDistances[ liI ], 25, 10000 );
            lDebugInterface.SetStep( &mauOverrideLodDistances[ liI ], 25 );
        }

        lDebugInterface.RegisterVariable( &mbDownloadMassiveTextures, "World/Massive", "Download Massive Textures" );
        lDebugInterface.RegisterVariable( &mbMassive3dDebug, "World/Massive", "Massive 3D Debug" );
        lDebugInterface.RegisterVariable( &mbMassiveDistanceLinesDebug, "World/Massive", "Massive Distance Lines Debug" );
        lDebugInterface.RegisterVariable( &mbMassiveGenerateImpressionData, "World/Massive", "Massive Generate Impression Data" );
        lDebugInterface.RegisterVariable( &mfMassiveImpressionDebugDistance, "World/Massive", "Massive Impression Debug" );
    }

    mIsBackdropInstanceInScene.Construct();

    { mbIsNewModule = true; }
}

// =============================================================================
// Prepare  @ 0x823027D0
//
// Stage machine. NOTE vs the DWARF enum: the X360 build no longer visits
// E_PREPARESTAGE_COLLISION (world collision rides the validate protocol from
// PostPhysicsUpdate instead) -- the switch falls from PVS to SURFACELIST.
// =============================================================================
bool
WorldEntityModule::Prepare( WorldEntityIO::OutputBuffer_Prepare* lpOutputBuffer )
{
    lpOutputBuffer->LockForWrite();

    switch ( mePrepareStage )
    {
        case E_PREPARESTAGE_START:
        case E_PREPARESTAGE_MANAGER:
        {
            mePrepareStage = E_PREPARESTAGE_MANAGER;

            if ( !CgsModule::ModuleSingleBuffered::Prepare() )
            {
                break;
            }
        }
        // fall through

        case E_PREPARESTAGE_PVS:
        {
            mePrepareStage = E_PREPARESTAGE_PVS;

            if ( !mPVSModule.Prepare() )
            {
                mPVSModule.LockForOutput();
                BridgePVSToOutput_Prepare( lpOutputBuffer );
                mPVSModule.UnlockForOutput();

                lpOutputBuffer->UnlockForWrite();
                return false;
            }

            mPlayerZoneResponse.Construct( -1, 0, -1 );
        }
        // fall through

        case E_PREPARESTAGE_SURFACELIST:
        {
            mePrepareStage = E_PREPARESTAGE_SURFACELIST;

            if ( !PrepareSurfaceList( lpOutputBuffer ) )
            {
                break;
            }
        }
        // fall through

        case E_PREPARESTAGE_MASSIVE:
        {
            mePrepareStage = E_PREPARESTAGE_MASSIVE;

            if ( !PrepareMassive( lpOutputBuffer ) )
            {
                break;
            }
        }
        // fall through

        case E_PREPARESTAGE_COMMONDATA:
        {
            mePrepareStage = E_PREPARESTAGE_COMMONDATA;

            // [FLAG PC boot gate 2026-07-26] the three common-data bundle loads are the
            // REAL X360 posts, but on PC today they wedge the boot: WORLDTEX.BIN loads and
            // then floods CreateTexture(fmt=0 0x0) failures (the ported world Texture
            // headers do not FixUp through the PC raster path yet -- the porter/consumer
            // seam), and GLOBALPROPS.BIN is refused (-1: its PropGraphicsList/
            // PropInstanceData payloads are still passthrough-BE in the converter).
            // Skip the posts + the 3-reply wait (one-shot log) until the WORLDTEX
            // texture seam + the prop-type porters land; then delete this gate.
            {
                static bool s_bLoggedCommonDataGate = false;
                if ( !s_bLoggedCommonDataGate )
                {
                    s_bLoggedCommonDataGate = true;
                    if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                        *CgsDev::Log::gpDebugPrint
                            << "WorldEntityModule::Prepare: COMMONDATA loads skipped "
                               "(WORLDTEX texture seam / prop porters pending) "
                               "[FLAG PC boot gate]\n";
                }
            }
            if ( false )   // the gated X360 interior, kept verbatim:
            {
                lpOutputBuffer->GetResourceRequestInterface()->LoadBundle(
                    &mReceiverQueue, 1, 3 /* open-world graphics pool (GameDataModule pool table id 3) */, "WORLDTEX.BIN", false );
                lpOutputBuffer->GetResourceRequestInterface()->LoadBundle(
                    &mReceiverQueue, 0, 3 /* open-world graphics pool (GameDataModule pool table id 3) */, "GLOBALPROPS.BIN", false );
                lpOutputBuffer->GetResourceRequestInterface()->LoadBundle(
                    &mReceiverQueue, 2, 3 /* open-world graphics pool (GameDataModule pool table id 3) */, "GLOBALBACKDROPS.BNDL", false );
            }
        }
        // fall through

        case E_PREPARESTAGE_COMMONDATA_LOADING:
        {
            mePrepareStage = E_PREPARESTAGE_COMMONDATA_LOADING;

            // [FLAG PC boot gate] the 3-reply wait is skipped with the gated posts above.
            if ( false && mReceiverQueue.GetLength() < 3 )
            {
                break;
            }
        }
        // fall through

        case E_PREPARESTAGE_ACQUIRE_RESOURCES:
        case E_PREPARESTAGE_DONE:
        {
            mePrepareStage = E_PREPARESTAGE_MANAGER;

            mbWaitingForStreaming = false;

            meReleaseStage = E_RELEASESTAGE_START;

            lpOutputBuffer->UnlockForWrite();

            return true;
        }

        default:
        {
            CGS_ASSERT( false, "Invalid Stage\n" );
        }
        break;
    }

    lpOutputBuffer->UnlockForWrite();

    return false;
}

// =============================================================================
// Release  @ 0x822A8498
//
// NOTE vs the DWARF enum: the X360 body falls straight from MANAGER into
// MASSIVE (E_RELEASESTAGE_RESOURCES is not visited).
// =============================================================================
bool
WorldEntityModule::Release( void )
{
    switch ( meReleaseStage )
    {
        case E_RELEASESTAGE_START:
        case E_RELEASESTAGE_PVS:
        {
            meReleaseStage = E_RELEASESTAGE_PVS;

            if ( !mPVSModule.Release() )
            {
                break;
            }
        }
        // fall through

        case E_RELEASESTAGE_MANAGER:
        {
            meReleaseStage = E_RELEASESTAGE_MANAGER;

            if ( !CgsModule::ModuleSingleBuffered::Release() )
            {
                break;
            }
        }
        // fall through

        case E_RELEASESTAGE_MASSIVE:
        {
            meReleaseStage = E_RELEASESTAGE_MASSIVE;

            // The X360 massive-release hook ICF-folded with the trivial `return true`
            // body (CgsSound::Playback::Content::DoOnPostLoad alias @ the call site):
            // there is nothing to tear down here in the shipped build.
        }
        // fall through

        case E_RELEASESTAGE_DONE:
        {
            meReleaseStage = E_RELEASESTAGE_MANAGER;

            mePrepareStage = E_PREPARESTAGE_START;

            return true;
        }

        default:
        {
            CGS_ASSERT( false, "Invalid Stage\n" );
        }
        break;
    }

    return false;
}

// =============================================================================
// Destruct  @ 0x822A8590
// =============================================================================
void
WorldEntityModule::Destruct( void )
{
    mPVSModule.Destruct();
    mMassive.Destruct();
    CgsModule::ModuleSingleBuffered::Destruct();
}

// =============================================================================
// ProcessValidationRequests  @ 0x822A85E0  (cpp:953)
// =============================================================================
void
WorldEntityModule::ProcessValidationRequests( const WorldEntityIO::RequestInterface* lpRequestInterface )
{
    CGS_ASSERT( lpRequestInterface, "lpRequestInterface" );

    if ( lpRequestInterface->mbValidateCollisionWorld || mbDebugTriggerValidate )
    {
        if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 )   // GLOBAL
        {
            ( *CgsDev::Log::gpDebugPrint ) << "Collision world validate requested\n";
        }
        CGS_ASSERT( meCollisionWorldState == E_COLWORLDSTATE_INVALID,
                        "Can only validate when invalid\n" );

        meValidationStage = E_COLWORLDVALIDATIONSTAGE_START;
        mbDebugTriggerValidate = false;
        meCollisionWorldState = E_COLWORLDSTATE_VALIDATING;
    }
    else if ( lpRequestInterface->mbInvalidateCollisionWorld || mbDebugTriggerInvalidate )
    {
        if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 )   // GLOBAL
        {
            ( *CgsDev::Log::gpDebugPrint ) << "Collision world invalidate requested\n";
        }
        CGS_ASSERT( meCollisionWorldState == E_COLWORLDSTATE_VALID,
                        "Can only invalidate when valid\n" );

        meInvalidationStage = E_COLWORLDINVALIDATIONSTAGE_START;
        mbDebugTriggerInvalidate = false;
        meCollisionWorldState = E_COLWORLDSTATE_INVALIDATING;
    }
}

// =============================================================================
// PreSceneUpdate  @ 0x82302A08  (cpp:989)
// =============================================================================
void
WorldEntityModule::PreSceneUpdate(
    const WorldEntityIO::InputBuffer_PreScene* lpInputBuffer,
    WorldEntityIO::OutputBuffer_PreScene* lpOutputBuffer,
    Vector3::InParam lSimulatedCameraPosition,
    BrnUpdateSet lUpdateSet )
{
    lpOutputBuffer->LockForWrite();
    lpInputBuffer->LockForRead();

    ProcessValidationRequests( lpInputBuffer->GetRequestInterface() );

    UpdateMassive( lUpdateSet );

    // Cache the output buffer for the streamer callbacks fired inside
    // UpdateStream / mPVSModule.Update.
    mpTempCachePreSceneOutput = lpOutputBuffer;

    mPVSDebugComponent.SetRendering( true );

    mPVSModule.LockForInput();

    if ( lpInputBuffer->GetActiveRaceCarInterface()->IsPlayerCarActive() )
    {
        const EActiveRaceCarIndex lePlayerActiveRaceCarIndex =
            lpInputBuffer->GetActiveRaceCarInterface()->GetPlayerActiveRaceCarIndex();

        const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState =
            lpInputBuffer->GetActiveRaceCarInterface()->GetRaceCarState( lePlayerActiveRaceCarIndex );

        if ( mbUseCarForPvs )
        {
            mPositionUsedForPVS = lpRaceCarState->mTransform.Pos();
        }
        else
        {
            mPositionUsedForPVS = lSimulatedCameraPosition;
        }

        PVSIO::GetZoneRequest lRequest;
        lRequest.miPlayerIndex = 0;
        lRequest.miPad04 = 0;
        lRequest.mPosition = Vector4{ mPositionUsedForPVS.x, mPositionUsedForPVS.y,
                                      mPositionUsedForPVS.z, 0.0f };
        lRequest.miLookupIndex = mPlayerZoneResponse.GetLookupIndex();
        lRequest.mbUseVelocity = 1;
        lRequest.mVelocity = Vector4{ lpRaceCarState->mLinearVelocity.x,
                                      lpRaceCarState->mLinearVelocity.y,
                                      lpRaceCarState->mLinearVelocity.z, 0.0f };

        mPVSModule.GetInputInterface()->mZoneRequestQueue.AddEvent( lRequest );
    }
    else
    {
        mPositionUsedForPVS = lSimulatedCameraPosition;

        PVSIO::GetZoneRequest lRequest;
        lRequest.miPlayerIndex = 0;
        lRequest.miPad04 = 0;
        lRequest.mPosition = Vector4{ mPositionUsedForPVS.x, mPositionUsedForPVS.y,
                                      mPositionUsedForPVS.z, 0.0f };
        lRequest.miLookupIndex = mPlayerZoneResponse.GetLookupIndex();
        lRequest.mbUseVelocity = 0;
        lRequest.mVelocity.SetZero();

        mPVSModule.GetInputInterface()->mZoneRequestQueue.AddEvent( lRequest );
    }

    mPVSDebugComponent.SetPvsCentre( mPositionUsedForPVS );

    mPVSModule.UnlockForInput();

    mPVSModule.Update();

    UpdateStream( lpOutputBuffer );

    mpTempCachePreSceneOutput = 0;

    lpInputBuffer->UnlockForRead();
    lpOutputBuffer->UnlockForWrite();
}

// =============================================================================
// UpdateCollisionValidation  @ 0x82307FC0  (cpp:1096)
// =============================================================================
void
WorldEntityModule::UpdateCollisionValidation( WorldEntityIO::OutputBuffer_PostPhysics* lpOutputBuffer )
{
    CGS_ASSERT( lpOutputBuffer, "lpOutputBuffer" );

    lpOutputBuffer->GetStatusInterface()->SetCollisionWorldInvalid( meCollisionWorldState != E_COLWORLDSTATE_VALID );
    lpOutputBuffer->GetStatusInterface()->SetCollisionWorldInvalidating( meCollisionWorldState == E_COLWORLDSTATE_INVALIDATING );
    lpOutputBuffer->GetStatusInterface()->SetCollisionWorldValidating( meCollisionWorldState == E_COLWORLDSTATE_VALIDATING );

    if ( meCollisionWorldState == E_COLWORLDSTATE_VALIDATING )
    {
        if ( ValidateCollision( lpOutputBuffer->GetResourceRequestInterface(),
                                lpOutputBuffer->GetSceneInputInterface() ) )
        {
            meCollisionWorldState = E_COLWORLDSTATE_VALID;
        }
    }
    else if ( meCollisionWorldState == E_COLWORLDSTATE_INVALIDATING )
    {
        if ( InvalidateCollision( lpOutputBuffer->GetResourceRequestInterface(),
                                  lpOutputBuffer->GetSceneInputInterface() ) )
        {
            meCollisionWorldState = E_COLWORLDSTATE_INVALID;
        }
    }
}

// =============================================================================
// PostPhysicsUpdate  @ 0x823080F0  (cpp:1135)
// =============================================================================
void
WorldEntityModule::PostPhysicsUpdate(
    const WorldEntityIO::InputBuffer_PostPhysics* lpInputBuffer,
    WorldEntityIO::OutputBuffer_PostPhysics* lpOutputBuffer,
    BrnUpdateSet lUpdateSet )
{
    lpInputBuffer->LockForRead();
    lpOutputBuffer->LockForWrite();

    UpdateCollisionValidation( lpOutputBuffer );

    lpOutputBuffer->GetStatusInterface()->SetAllStreamed(
        mWorldGraphicsStreamer.IsStreamComplete() );

    // The world counts as graphically streamed when the player's zone -- and
    // every zone the PVS marks as required -- is resident in the streamer.
    bool lbWorldStreamed = mWorldGraphicsStreamer.IsAssetLoaded(
        BrnResource::MakeTrackUnitId( mPlayerZoneResponse.GetZoneNumber( 0 ) ),
        mPlayerZoneResponse.GetZoneNumber( 0 ) );

    for ( u32 luZone = 1; luZone < static_cast<u32>( mPlayerZoneResponse.GetNumZones() ); luZone++ )
    {
        if ( !mPlayerZoneResponse.IsZoneRequired( luZone ) )
        {
            continue;
        }

        if ( !mWorldGraphicsStreamer.IsAssetLoaded(
                 BrnResource::MakeTrackUnitId( mPlayerZoneResponse.GetZoneNumber( luZone ) ),
                 mPlayerZoneResponse.GetZoneNumber( luZone ) ) )
        {
            lbWorldStreamed = false;
        }
    }

    if ( !_mbAllowStreamStalling )
    {
        lbWorldStreamed = true;
    }

    lpOutputBuffer->GetStatusInterface()->SetImmediateStreamed( lbWorldStreamed );

    lpOutputBuffer->GetResourceRequestInterface()->Append(
        *mWorldGraphicsStreamer.GetGameDataRequestInterface() );

    HandleExternalRequests( lpInputBuffer );

    lpInputBuffer->UnlockForRead();
    lpOutputBuffer->UnlockForWrite();
}

// =============================================================================
// UpdateStream  @ 0x822F9740  (cpp:1421)
// =============================================================================
void
WorldEntityModule::UpdateStream( WorldEntityIO::OutputBuffer_PreScene* lpOutputBuffer )
{
    // The stream sits out the first few frames after boot (X360 static counter).
    if ( ++siUpdateStreamFrameCounter < 5 )
    {
        return;
    }

    mPVSModule.LockForOutput();

    PVSIO::OutputBuffer* lpPVSOutput = mPVSModule.GetOutputInterface();

    CGS_ASSERT( lpPVSOutput->GetZoneResponseQueue()->GetLength() > 0, "Expected zone response\n" );

    // Latch the newest PVS reply as the player-zone response.
    mPlayerZoneResponse = lpPVSOutput->GetZoneResponseQueue()->GetEvent( 0 );

    // Track the last player zone whose graphics were resident.
    {
        const s32 liLoadedIndex = mWorldGraphicsStreamer.GetIndexFromId( miPlayerZoneNumber );
        if ( mWorldGraphicsStreamer.IsListLoaded( liLoadedIndex ) )
        {
            miPreviousPlayerZoneNumber = miPlayerZoneNumber;
        }
    }

    miPlayerZoneNumber = mPlayerZoneResponse.GetZoneNumber( 0 );
    lpOutputBuffer->SetPlayerZoneNumber( miPlayerZoneNumber );

    // Rebuild the streamer's target list from the PVS zone set.
    mWorldGraphicsStreamer.ClearTargetList();

    for ( u32 luZone = 0; luZone < static_cast<u32>( mPlayerZoneResponse.GetNumZones() ); luZone++ )
    {
        const s32 liZoneNumber = mPlayerZoneResponse.GetZoneNumber( luZone );

        mWorldGraphicsStreamer.AddEntry(
            BrnResource::MakeTrackUnitId( liZoneNumber ),
            mPlayerZoneResponse.IsZoneSafe( luZone ),
            static_cast<u64>( static_cast<u32>( liZoneNumber ) ) );

        if ( mPlayerZoneResponse.ZoneHasPropInstances( luZone ) )
        {
            PropEntityIO::PropInstancesNeededForZoneEvent lEvent;
            lEvent.muZoneId = static_cast<u16>( liZoneNumber );
            lpOutputBuffer->GetPropInstancesNeededForZoneQueue()->AddEvent( lEvent );
        }
    }

    muNumZonesLoadedThisFrame = 0;
    muNumZonesUnloadedThisFrame = 0;

    mWorldGraphicsStreamer.Update();

    // A GameAction asked us to report when the world stream settles.
    if ( mbWaitingForStreaming && mWorldGraphicsStreamer.IsStreamComplete() )
    {
        BrnGameState::GameStateModuleIO::StreamingCompleteEvent lEvent;
        lEvent.meModule = BrnGameState::GameStateModuleIO::StreamingCompleteEvent::E_MODULE_WORLD_GRAPHICS;
        lEvent.mUserId  = 0;
        lpOutputBuffer->GetGameEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>( &lEvent ),
            BrnGameState::GameStateModuleIO::E_EVENT_STREAMING_COMPLETE,
            sizeof( lEvent ) );
        mbWaitingForStreaming = false;
    }

    // Swap the backdrop stand-in set as the player crosses zones.
    const s32 liPreviousZoneIndex = mWorldGraphicsStreamer.GetIndexFromId( miPreviousPlayerZoneNumber );
    const s32 liCurrentZoneIndex  = mWorldGraphicsStreamer.GetIndexFromId( miPlayerZoneNumber );

    bool lbUpdateEntities = true;

    if ( mbBackdropLoaded || miPreviousPlayerZoneNumber != -1 )
    {
        if ( liPreviousZoneIndex != liCurrentZoneIndex || !mbBackdropLoaded )
        {
            if ( mbBackdropLoaded )
            {
                UnloadBackdropForZone( liPreviousZoneIndex );
                lbUpdateEntities = false;
                mbBackdropLoaded = false;
            }

            if ( mWorldGraphicsStreamer.IsListLoaded( liCurrentZoneIndex ) )
            {
                LoadBackdropForZone( liCurrentZoneIndex );
                mbBackdropLoaded = true;
                lbUpdateEntities = false;
            }
        }
    }
    else if ( mWorldGraphicsStreamer.IsListLoaded( liCurrentZoneIndex ) )
    {
        LoadBackdropForZone( liCurrentZoneIndex );
        mbBackdropLoaded = true;
        lbUpdateEntities = false;
    }

    if ( lbUpdateEntities )
    {
        for ( u32 luI = 0; luI < muNumZonesLoadedThisFrame; luI++ )
        {
            UpdateBackdropSceneEntities( maiZonesLoadedThisFrame[ luI ], true, lpOutputBuffer );
        }
        for ( u32 luI = 0; luI < muNumZonesUnloadedThisFrame; luI++ )
        {
            UpdateBackdropSceneEntities( maiZonesUnloadedThisFrame[ luI ], false, lpOutputBuffer );
        }
    }

    mPVSModule.UnlockForOutput();
}

// =============================================================================
// QueryWorldGraphicsLoad  @ 0x822A87B0  (cpp:1585)
//
// Picks the potential entry whose zone carries the highest PVS weight in the
// current player-zone response.
// =============================================================================
s32
WorldEntityModule::QueryWorldGraphicsLoad( const StreamerTargetEntry* lpPotentialList,
                                           s32 liPotentialListLength )
{
    s32 liBestEntry = -1;
    f32 lfBestWeight = -1.0f;

    for ( s32 liEntry = 0; liEntry < liPotentialListLength; liEntry++ )
    {
        const u32 luZoneNumber = static_cast<u32>( lpPotentialList[ liEntry ].GetUserId() );

        for ( u32 luZone = 0; luZone < static_cast<u32>( mPlayerZoneResponse.GetNumZones() ); luZone++ )
        {
            if ( static_cast<u32>( mPlayerZoneResponse.GetZoneNumber( luZone ) ) != luZoneNumber )
            {
                continue;
            }

            if ( mPlayerZoneResponse.GetZoneWeight( luZone ) > lfBestWeight )
            {
                liBestEntry = liEntry;
                lfBestWeight = mPlayerZoneResponse.GetZoneWeight( luZone );
            }
            break;
        }
    }

    return liBestEntry;
}

// =============================================================================
// QueryWorldGraphicsUnload  (cpp:1631)
//
// Not separately emitted by the X360 (folded into the streamer's QueryUnload
// slot); the unload policy is "first candidate".
// =============================================================================
s32
WorldEntityModule::QueryWorldGraphicsUnload( const StreamerTargetEntry* lpPotentialList,
                                             s32 liPotentialListLength )
{
    (void)lpPotentialList;

    return ( liPotentialListLength > 0 ) ? 0 : -1;
}

// =============================================================================
// OnWorldGraphicsLoadComplete  @ 0x822D7828  (cpp:1648)
//
// A track unit's graphics arrived: add each instance to the scene manager,
// tell the prop + sound systems, and record the zone for the backdrop update.
// =============================================================================
void
WorldEntityModule::OnWorldGraphicsLoadComplete( const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent,
                                                s32 liIndex )
{
    (void)lpEvent;

    const u16 luZoneNumber = static_cast<u16>( mWorldGraphicsStreamer.GetUserId( liIndex ) );

    {
        PropEntityIO::PropGraphicsLoadedEvent lPropEvent;
        lPropEvent.muPropGraphicsId = luZoneNumber;
        mpTempCachePreSceneOutput->GetPropGraphicsLoadedQueue()->AddEvent( lPropEvent );
    }

    {
        BrnSound::Module::Io::SoundWorldLoadEvent lSoundEvent;
        lSoundEvent.Construct( BrnSound::Module::Io::SoundWorldLoadEvent::E_LOAD_EVENT_PASSBY_MAP_LOADED,
                               luZoneNumber );
        mpTempCachePreSceneOutput->GetSoundWorldLoadInterface()->AddEvent( lSoundEvent );
    }

    CGS_ASSERT( mpTempCachePreSceneOutput, "mpTempCachePreSceneOutput" );

    CgsGraphics::InstanceList* lpList = mWorldGraphicsStreamer.GetInstanceList( liIndex );
    if ( !lpList )
    {
        return;
    }

    s32 liInstanceCount = 0;

    for ( liInstanceCount = 0; liInstanceCount < static_cast<s32>( lpList->muNumInstances ); liInstanceCount++ )
    {
        const CgsGraphics::Instance* lpInstance = lpList->GetInstance( liInstanceCount );
        CGS_ASSERT( lpInstance,
            "BrnWorld::WorldEntityModule::OnWorldGraphicsLoadComplete: Couldn't find instance in instance list" );

        const CgsGraphics::Model* lpModel = lpInstance->mpModel;
        CGS_ASSERT( lpModel, "lpModel" );
        CGS_ASSERT( lpModel->DoesStateExist( CgsGraphics::Model::E_STATE_LOD_0 ), "lpModel->DoesStateExist( CgsGraphics::Model::E_STATE_LOD_0 )" );

        const Renderable* lpRenderable =
            lpModel->GetRenderable( CgsGraphics::Model::E_STATE_LOD_0 );
        CGS_ASSERT( lpRenderable, "lpRenderable" );

        // Sphere centre = the renderable centre through the instance transform;
        // radius scaled by the transform's maximum axis scale.
        Vector3 lCentre = rw::math::vpu::TransformPoint(
            lpInstance->mTransform, lpRenderable->mBoundingSphere.GetVector3() );
        const f32 lfRadius =
            lpRenderable->mBoundingSphere.GetPlus() * GetMaximumScale( lpInstance->mTransform );

        CgsSceneManager::EntityId lEntityId;
        lEntityId.Set( KU_ENTITY_OWNER_WORLD, liIndex, liInstanceCount );

        mpTempCachePreSceneOutput->GetSceneInputInterface()->AddEntity(
            lEntityId, 9408u, lCentre, lfRadius );
    }

    CGS_ASSERT( muNumZonesLoadedThisFrame < 32, "muNumZonesLoadedThisFrame < 32" );

    maiZonesLoadedThisFrame[ muNumZonesLoadedThisFrame ] = static_cast<s16>( luZoneNumber );
    muNumZonesLoadedThisFrame++;

    maiNumInstancesLoadedPerZone[ liIndex ] = liInstanceCount;
    miNumInstanceListsLoaded++;
}

// =============================================================================
// OnWorldGraphicsUnloadComplete  (cpp:1774)
//
// Empty in the shipped build (the X360 folded it, together with
// OnWorldGraphicsLoadBegin, into an ICF alias of an empty body).
// =============================================================================
void
WorldEntityModule::OnWorldGraphicsUnloadComplete( const BrnResource::GameDataIO::UnloadGameDataResponse* lpEvent )
{
    (void)lpEvent;
}

// =============================================================================
// OnWorldGraphicsLoadBegin  (cpp:1789) -- empty in the shipped build (see above).
// =============================================================================
void
WorldEntityModule::OnWorldGraphicsLoadBegin( s32 liListIndex )
{
    (void)liListIndex;
}

// =============================================================================
// OnWorldGraphicsUnloadBegin  @ 0x822D7CB0  (cpp:1803)
//
// A track unit's graphics are about to go away: remove its scene entities and
// tell the prop + sound systems.
// =============================================================================
void
WorldEntityModule::OnWorldGraphicsUnloadBegin( s32 liListIndex )
{
    const u16 luZoneNumber = static_cast<u16>( mWorldGraphicsStreamer.GetUserId( liListIndex ) );

    {
        PropEntityIO::PropGraphicsUnloadedEvent lPropEvent;
        lPropEvent.muPropGraphicsId = luZoneNumber;
        mpTempCachePreSceneOutput->GetPropGraphicsUnloadedQueue()->AddEvent( lPropEvent );
    }

    {
        BrnSound::Module::Io::SoundWorldLoadEvent lSoundEvent;
        lSoundEvent.Construct( BrnSound::Module::Io::SoundWorldLoadEvent::E_LOAD_EVENT_PASSBY_MAP_UNLOAD,
                               luZoneNumber );
        mpTempCachePreSceneOutput->GetSoundWorldLoadInterface()->AddEvent( lSoundEvent );
    }

    CGS_ASSERT( mpTempCachePreSceneOutput, "mpTempCachePreSceneOutput" );

    CgsGraphics::InstanceList* lpList = mWorldGraphicsStreamer.GetInstanceList( liListIndex );
    if ( !lpList )
    {
        return;
    }

    s32 liInstanceCount = 0;

    for ( liInstanceCount = 0; liInstanceCount < static_cast<s32>( lpList->muNumInstances ); liInstanceCount++ )
    {
        CgsSceneManager::EntityId lEntityId;
        lEntityId.Set( KU_ENTITY_OWNER_WORLD, liListIndex, liInstanceCount );

        mpTempCachePreSceneOutput->GetSceneInputInterface()->RemoveEntity( lEntityId, 0 );
    }

    CGS_ASSERT( maiNumInstancesLoadedPerZone[ liListIndex ] == liInstanceCount, "maiNumInstancesLoadedPerZone[ liListIndex ] == liInstanceCount" );
}

// =============================================================================
// LoadBackdropForZone  @ 0x822EDBC8  (cpp:1905)
//
// Adds the zone's backdrop stand-in entities for every instance whose real
// track unit is not itself resident.
// =============================================================================
void
WorldEntityModule::LoadBackdropForZone( s32 liZone )
{
    miCurrentBackdropZoneId = static_cast<s32>( static_cast<u32>( mWorldGraphicsStreamer.GetUserId( liZone ) ) );

    CGS_ASSERT( mpTempCachePreSceneOutput, "mpTempCachePreSceneOutput" );

    CgsGraphics::InstanceList* lpList = mWorldGraphicsStreamer.GetInstanceList( liZone );
    if ( !lpList )
    {
        return;
    }

    // The backdrop stand-ins are the tail entries beyond the complete instances.
    for ( s32 liInstance = static_cast<s32>( lpList->muNumInstances );
          liInstance < static_cast<s32>( lpList->muArraySize );
          liInstance++ )
    {
        CgsGraphics::Instance* lpInstance = lpList->GetInstance( liInstance );
        CGS_ASSERT( lpInstance,
            "BrnWorld::WorldEntityModule::OnEnterNewZone: Couldn't find backdrop in instance list" );

        mIsBackdropInstanceInScene.UnSetBit( liInstance );

        // Skip the stand-in when its real track unit is already resident.
        const u32 luBackdropZone = lpInstance->muBackdropZoneNumber;
        bool lbRealZoneLoaded = false;

        if ( luBackdropZone != 0xFFFF )
        {
            const s32 liRealIndex = mWorldGraphicsStreamer.GetIndexFromId( luBackdropZone );
            if ( liRealIndex != -1 && mWorldGraphicsStreamer.IsListLoaded( liRealIndex ) )
            {
                lbRealZoneLoaded = true;
            }
        }

        if ( !lbRealZoneLoaded )
        {
            AddBackdropEntity( lpInstance, liZone, liInstance, mpTempCachePreSceneOutput );
        }
    }
}

// =============================================================================
// UnloadBackdropForZone  @ 0x822D7E10  (cpp:1969)
// =============================================================================
void
WorldEntityModule::UnloadBackdropForZone( s32 liZone )
{
    CGS_ASSERT( mpTempCachePreSceneOutput, "mpTempCachePreSceneOutput" );

    miCurrentBackdropZoneId = -1;

    CgsGraphics::InstanceList* lpList = mWorldGraphicsStreamer.GetInstanceList( liZone );
    if ( !lpList )
    {
        return;
    }

    // The backdrop stand-ins are the tail entries beyond the complete instances.
    for ( s32 liInstance = static_cast<s32>( lpList->muNumInstances );
          liInstance < static_cast<s32>( lpList->muArraySize );
          liInstance++ )
    {
        if ( mIsBackdropInstanceInScene.IsBitSet( liInstance ) )
        {
            CGS_ASSERT( lpList->GetInstance( liInstance ),
                "BrnWorld::WorldEntityModule::OnEnterNewZone: Couldn't find backdrop in instance list" );

            RemoveBackdropEntity( liZone, liInstance, mpTempCachePreSceneOutput );
        }
    }
}

// =============================================================================
// PrepareSurfaceList  @ 0x822F9B70  (cpp:2027)
// =============================================================================
bool
WorldEntityModule::PrepareSurfaceList( WorldEntityIO::OutputBuffer_Prepare* lpOutputBuffer )
{
    CGS_ASSERT( lpOutputBuffer != 0, "lpOutputBuffer != 0" );

    switch ( meSurfaceListPrepareStage )
    {
        case E_SURFACELIST_PREPARESTAGE_START:
        {
            lpOutputBuffer->GetResourceRequestInterface()->GetSurfaceList( &mReceiverQueue, 0, 7 );
            mReceiverQueue.Clear();
            meSurfaceListPrepareStage = E_SURFACELIST_PREPARESTAGE_LOADING;
            return false;
        }

        case E_SURFACELIST_PREPARESTAGE_LOADING:
        {
            if ( mReceiverQueue.GetLength() <= 0 )
            {
                return false;
            }

            {
                const CgsModule::Event* lpEventData = 0;
                s32 liSize = 0;
                const s32 liEventId = mReceiverQueue.GetFirstEvent( &lpEventData, &liSize );
                CGS_ASSERT( liEventId == BrnResource::GameDataIO::EVENT_GET_SURFACE_LIST,
                                "liEventId == BrnResource::GameDataIO::EVENT_GET_SURFACE_LIST" );
            }

            // [FLAG PC boot gate] the surface-list attrib rebind + sanity probe read the
            // LIVE Attrib database (ChangeWithDefault -> FindCollectionWithDefault; the
            // probe walks surface 0's collection). On PC the AttribSys schema is not yet
            // loaded (PrepareAttribSysSchemaResource gate: the exe-baked BE schema blobs
            // need an LE port) so the surfacelist vault was never registered into the DB
            // (AttribSysModule::RegisterVault gate) -- the reads CANNOT resolve. Skip them
            // (one-shot log) and advance; remove together with the schema/RegisterVault
            // gates once the Attrib SDK runtime cluster is committed.
            {
                static bool s_bLoggedSurfaceGate = false;
                if ( !s_bLoggedSurfaceGate )
                {
                    s_bLoggedSurfaceGate = true;
                    if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                        *CgsDev::Log::gpDebugPrint
                            << "WorldEntityModule::PrepareSurfaceList: attrib rebind skipped "
                               "(schema/DB deferred) [FLAG PC boot gate]\n";
                }
            }
            if ( false )   // the gated X360 interior, kept verbatim:
            {
                mSurfaceList.ChangeWithDefault();

                // Sanity-probe surface 0: an implausible first attribute means the
                // streamed surface list is corrupt.
                Attrib::RefSpec* lpRefSpec =
                    static_cast<Attrib::RefSpec*>( mSurfaceList.Surfaces( 0 ) );
                Attrib::Gen::surface lSurface(
                    const_cast<Attrib::Collection*>( lpRefSpec->GetCollection() ), 0 );
                const f32* lpfFirstAttribute =
                    static_cast<const f32*>( lSurface.GetAttributeData() );
                const f32 lfMagnitude =
                    ( lpfFirstAttribute[0] < 0.0f ) ? -lpfFirstAttribute[0] : lpfFirstAttribute[0];
                CGS_ASSERT( !( lfMagnitude > KF_SURFACE_SANITY_THRESHOLD ),
                            "Surface list appears to be corrupt" );
            }

            meSurfaceListPrepareStage = E_SURFACELIST_PREPARESTAGE_DONE;
            return false;
        }

        case E_SURFACELIST_PREPARESTAGE_DONE:
        {
            mReceiverQueue.Clear();
            return true;
        }

        default:
        {
            CGS_ASSERT( false, "Invalid surface list prepare state" );
        }
        break;
    }

    return false;
}

// =============================================================================
// PrepareWorldCollision  @ 0x823068F8  (cpp:2115)
// =============================================================================
bool
WorldEntityModule::PrepareWorldCollision(
    WorldEntityIO::ResourceRequestInterface* lpRequestInterface,
    WorldEntityIO::SceneInputInterface* lpSceneInterface,
    bool lbLoadBundle )
{
    CGS_ASSERT( lpRequestInterface && lpSceneInterface, "lpRequestInterface && lpSceneInterface" );

    switch ( meWorldColPrepareStage )
    {
        case E_WORLDCOL_PREPARESTAGE_START:
        {
            meZoneColPrepareStage = E_ZONECOL_PREPARESTAGE_START;
            miNumCollisonZonesLoaded = 0;

            if ( lbLoadBundle )
            {
                mReceiverQueue.Clear();
                lpRequestInterface->LoadWorldCollision( &mReceiverQueue, 1, 2 );
            }
        }
        // fall through

        case E_WORLDCOL_PREPARESTAGE_LOADBUNDLE:
        {
            meWorldColPrepareStage = E_WORLDCOL_PREPARESTAGE_LOADBUNDLE;

            if ( lbLoadBundle && mReceiverQueue.GetLength() == 0 )
            {
                return false;
            }
        }
        // fall through

        case E_WORLDCOL_PREPARESTAGE_PREPARING_ZONES:
        {
            meWorldColPrepareStage = E_WORLDCOL_PREPARESTAGE_PREPARING_ZONES;

            miNumZonesInWorld = GetTotalZones();

            while ( miNumCollisonZonesLoaded < miNumZonesInWorld )
            {
                if ( !PrepareZoneCollision( lpRequestInterface, lpSceneInterface ) )
                {
                    return false;
                }
                meZoneColPrepareStage = E_ZONECOL_PREPARESTAGE_START;
            }

            meWorldColPrepareStage = E_WORLDCOL_PREPARESTAGE_DONE;
            meZoneColPrepareStage = E_ZONECOL_PREPARESTAGE_DONE;
        }
        // fall through

        case E_WORLDCOL_PREPARESTAGE_DONE:
        {
            meCollisionWorldState = E_COLWORLDSTATE_VALID;
            return true;
        }

        default:
            break;
    }

    return false;
}

// =============================================================================
// PrepareZoneCollision  @ 0x82302C38  (cpp:2218)
//
// Acquires up to KI_NUM_COLLISION_ZONES_LOADED_PER_FRAME "TRK_CLIL%d" zone
// collision lists per batch, then feeds each into the scene manager.
// =============================================================================
bool
WorldEntityModule::PrepareZoneCollision(
    WorldEntityIO::ResourceRequestInterface* lpRequestInterface,
    WorldEntityIO::SceneInputInterface* lpSceneInterface )
{
    CGS_ASSERT( meWorldColPrepareStage == E_WORLDCOL_PREPARESTAGE_PREPARING_ZONES, "meWorldColPrepareStage == E_WORLDCOL_PREPARESTAGE_PREPARING_ZONES" );

    switch ( meZoneColPrepareStage )
    {
        case E_ZONECOL_PREPARESTAGE_START:
        {
            mReceiverQueue.Clear();

            const s32 liFirstZone = miNumCollisonZonesLoaded;
            s32 liEndZone = miNumZonesInWorld;
            if ( liFirstZone + KI_NUM_COLLISION_ZONES_LOADED_PER_FRAME < liEndZone )
            {
                liEndZone = liFirstZone + KI_NUM_COLLISION_ZONES_LOADED_PER_FRAME;
            }
            miCollisionZoneBatchSize = liEndZone - liFirstZone;

            for ( s32 liZone = liFirstZone; liZone < liEndZone; liZone++ )
            {
                lpRequestInterface->AcquireZoneCollision(
                    &mReceiverQueue,
                    liZone,
                    &maZoneCollisionHandles[ ( liZone - liFirstZone ) * KI_MAX_COLLISION_MESHES_PER_ZONE ],
                    KI_MAX_COLLISION_MESHES_PER_ZONE );
            }

            meZoneColPrepareStage = E_ZONECOL_PREPARESTAGE_ACQUIRING_MESHES;
        }
        // fall through

        case E_ZONECOL_PREPARESTAGE_ACQUIRING_MESHES:
        {
            if ( mReceiverQueue.GetLength() < miCollisionZoneBatchSize )
            {
                return false;
            }

            if ( mReceiverQueue.GetLength() > 0 )
            {
                const CgsModule::Event* lpEventData = 0;
                s32 liSize = 0;
                mReceiverQueue.GetFirstEvent( &lpEventData, &liSize );

                while ( lpEventData )
                {
                    // The reply is the AcquireResourceListRequest echoed back with the
                    // handle array filled (X360 reads +24/+28 = mpHandles/miMaxHandles).
                    const CgsResource::Events::AcquireResourceListRequest* lpResponse =
                        reinterpret_cast<const CgsResource::Events::AcquireResourceListRequest*>( lpEventData );

                    if ( lpResponse->miMaxHandles > 0 )
                    {
                        AddCollisionZoneToSceneManager(
                            lpRequestInterface, lpSceneInterface,
                            lpResponse->mpHandles, lpResponse->miMaxHandles );
                        miNumCollisonZonesLoaded++;
                    }

                    mReceiverQueue.GetNextEvent( lpEventData, &lpEventData, &liSize );
                }
            }

            mReceiverQueue.Clear();
            meZoneColPrepareStage = E_ZONECOL_PREPARESTAGE_DONE;
            return false;
        }

        case E_ZONECOL_PREPARESTAGE_DONE:
        {
            return true;
        }

        default:
            break;
    }

    return false;
}

// =============================================================================
// AddCollisionZoneToSceneManager  @ 0x822D8130  (cpp:2339)
// =============================================================================
bool
WorldEntityModule::AddCollisionZoneToSceneManager(
    WorldEntityIO::ResourceRequestInterface* lpRequestInterface,
    WorldEntityIO::SceneInputInterface* lpSceneInterface,
    CgsResource::ResourceHandle* lpaMeshHandles,
    s32 liNumHandles )
{
    CGS_ASSERT( meZoneColPrepareStage == E_ZONECOL_PREPARESTAGE_ACQUIRING_MESHES, "meZoneColPrepareStage == E_ZONECOL_PREPARESTAGE_ACQUIRING_MESHES" );
    CGS_ASSERT( meWorldColPrepareStage == E_WORLDCOL_PREPARESTAGE_PREPARING_ZONES, "meWorldColPrepareStage == E_WORLDCOL_PREPARESTAGE_PREPARING_ZONES" );
    CGS_ASSERT( liNumHandles == 1,
        "Currently only support 1 handle per zone - DONT change this without fixing invalidation / validation code\n" );
    CGS_ASSERT( lpRequestInterface && lpSceneInterface, "lpRequestInterface && lpSceneInterface" );

    for ( s32 liHandle = 0; liHandle < liNumHandles; liHandle++ )
    {
        CgsSceneManager::TriangleCollisionManagerIO::InEventAddPolySoupList lEvent;
        lEvent.mPolySoupListHandle = lpaMeshHandles[ liHandle ];
        lEvent.miZoneNumber = miNumCollisonZonesLoaded;
        lEvent.mbRebuildSpacialPartitioning =
            ( miNumCollisonZonesLoaded + miCollisionZoneBatchSize >= miNumZonesInWorld );

        lpSceneInterface->mAddPolySoupListQueue.AddEvent( lEvent );
    }

    return true;
}

// =============================================================================
// HandleExternalRequests  @ 0x822D82C0  (cpp:2383)
// =============================================================================
void
WorldEntityModule::HandleExternalRequests( const WorldEntityIO::InputBuffer_PostPhysics* lpInput )
{
    CGS_ASSERT( lpInput != 0, "lpInput != 0" );

    const WorldEntityIO::InputBuffer_PostPhysics::GameActionQueue* lpQueue = lpInput->GetGameActionQueue();

    const CgsModule::Event* lpEventData = 0;
    s32 liEventSize = 0;
    s32 liEventId = lpQueue->GetFirstEvent( &lpEventData, &liEventSize );

    while ( lpEventData )
    {
        if ( liEventId == 192 )
        {
            mbWaitingForStreaming = true;
        }
        else
        {
            // (assert text preserved from the original source)
            CGS_ASSERT( false, "Unknown GameAction coming into TrafficModule" );
        }

        liEventId = lpQueue->GetNextEvent( lpEventData, &lpEventData, &liEventSize );
    }
}

// =============================================================================
// ValidateCollision  @ 0x82306A48  (cpp:2430)
// =============================================================================
bool
WorldEntityModule::ValidateCollision(
    WorldEntityIO::ResourceRequestInterface* lpRequestInterface,
    WorldEntityIO::SceneInputInterface* lpSceneInterface )
{
    switch ( meValidationStage )
    {
        case E_COLWORLDVALIDATIONSTAGE_START:
        case E_COLWORLDVALIDATIONSTAGE_VALIDATE_WORLD:
        {
            meValidationStage = E_COLWORLDVALIDATIONSTAGE_VALIDATE_WORLD;

            mReceiverQueue.Clear();

            lpRequestInterface->SwapInCollisionWorld( &mReceiverQueue, 0 );
        }
        // fall through

        case E_COLWORLDVALIDATIONSTAGE_WF_VALIDATION:
        {
            meValidationStage = E_COLWORLDVALIDATIONSTAGE_WF_VALIDATION;

            if ( mReceiverQueue.GetLength() <= 0 )
            {
                return false;
            }

            mReceiverQueue.Clear();
            meWorldColPrepareStage = E_WORLDCOL_PREPARESTAGE_START;
        }
        // fall through

        case E_COLWORLDVALIDATIONSTAGE_PREPARING_COLLISION:
        {
            meValidationStage = E_COLWORLDVALIDATIONSTAGE_PREPARING_COLLISION;

            if ( !PrepareWorldCollision( lpRequestInterface, lpSceneInterface, false ) )
            {
                return false;
            }
        }
        // fall through

        case E_COLWORLDVALIDATIONSTAGE_DONE:
        {
            meValidationStage = E_COLWORLDVALIDATIONSTAGE_DONE;
            return true;
        }

        default:
        {
            CGS_ASSERT( false, "Invalid validation state\n" );
        }
        break;
    }

    return false;
}

// =============================================================================
// InvalidateCollision  @ 0x822F9D78  (cpp:2499)
// =============================================================================
bool
WorldEntityModule::InvalidateCollision(
    WorldEntityIO::ResourceRequestInterface* lpRequestInterface,
    WorldEntityIO::SceneInputInterface* lpSceneInterface )
{
    switch ( meInvalidationStage )
    {
        case E_COLWORLDINVALIDATIONSTAGE_START:
        case E_COLWORLDINVALIDATIONSTAGE_REMOVE_FROM_SCENE:
        {
            meInvalidationStage = E_COLWORLDINVALIDATIONSTAGE_REMOVE_FROM_SCENE;

            CgsSceneManager::TriangleCollisionManagerIO::InEventClearPolySoupLists lEvent;
            lEvent.miDummy = 0;
            lpSceneInterface->mClearPolySoupListsQueue.AddEvent( lEvent );
        }
        // fall through

        case E_COLWORLDINVALIDATIONSTAGE_INVALIDATE_WORLD:
        {
            meInvalidationStage = E_COLWORLDINVALIDATIONSTAGE_INVALIDATE_WORLD;

            mReceiverQueue.Clear();

            lpRequestInterface->SwapOutCollisionWorld( &mReceiverQueue, 0 );
        }
        // fall through

        case E_COLWORLDINVALIDATIONSTAGE_WF_INVALIDATION:
        {
            meInvalidationStage = E_COLWORLDINVALIDATIONSTAGE_WF_INVALIDATION;

            if ( mReceiverQueue.GetLength() <= 0 )
            {
                return false;
            }

            mReceiverQueue.Clear();
        }
        // fall through

        case E_COLWORLDINVALIDATIONSTAGE_DONE:
        {
            meInvalidationStage = E_COLWORLDINVALIDATIONSTAGE_DONE;
            return true;
        }

        default:
        {
            CGS_ASSERT( false, "Invalid invalidation state\n" );
        }
        break;
    }

    return false;
}

// =============================================================================
// AddBackdropEntity  @ 0x822D8380  (cpp:2567)
// =============================================================================
void
WorldEntityModule::AddBackdropEntity( CgsGraphics::Instance* lpInstance, s32 liListIndex,
                                      s32 liInstanceIndex,
                                      WorldEntityIO::OutputBuffer_PreScene* lpOutputBuffer )
{
    CGS_ASSERT( !mIsBackdropInstanceInScene.IsBitSet( liInstanceIndex ),
                    "Backdrop entity already added" );

    CGS_ASSERT( lpInstance->mpModel, "lpInstance->mpModel" );
    CGS_ASSERT( lpInstance->mpModel->DoesStateExist( CgsGraphics::Model::E_STATE_LOD_0 ), "lpInstance->mpModel->DoesStateExist( CgsGraphics::Model::E_STATE_LOD_0 )" );

    const Renderable* lpRenderable =
        lpInstance->mpModel->GetRenderable( CgsGraphics::Model::E_STATE_LOD_0 );
    CGS_ASSERT( lpRenderable, "lpRenderable" );

    Vector3 lCentre = rw::math::vpu::TransformPoint(
        lpInstance->mTransform, lpRenderable->mBoundingSphere.GetVector3() );
    const f32 lfRadius = lpRenderable->mBoundingSphere.GetPlus();

    CgsSceneManager::EntityId lEntityId;
    lEntityId.Set( KU_ENTITY_OWNER_WORLD,
                   static_cast<u32>( liListIndex ), liInstanceIndex );

    // Transparent backdrops carry the extra scene flag.
    const u32 luEntityFlags = ( lpRenderable->mu16Flags & 1 ) != 0 ? 12480u : 4160u;

    lpOutputBuffer->GetSceneInputInterface()->AddEntity(
        lEntityId, luEntityFlags, lCentre, lfRadius );

    mIsBackdropInstanceInScene.SetBit( liInstanceIndex );
}

// =============================================================================
// RemoveBackdropEntity  @ 0x822C34B8  (cpp:2631)
// =============================================================================
void
WorldEntityModule::RemoveBackdropEntity( s32 liListIndex, s32 liInstanceIndex,
                                         WorldEntityIO::OutputBuffer_PreScene* lpOutputBuffer )
{
    CGS_ASSERT( mIsBackdropInstanceInScene.IsBitSet( liInstanceIndex ),
                    "Backdrop entity doesn't exist" );

    CgsSceneManager::EntityId lEntityId;
    lEntityId.Set( KU_ENTITY_OWNER_WORLD,
                   static_cast<u32>( liListIndex ), liInstanceIndex );

    lpOutputBuffer->GetSceneInputInterface()->RemoveEntity( lEntityId, 0 );

    mIsBackdropInstanceInScene.UnSetBit( liInstanceIndex );
}

// =============================================================================
// UpdateBackdropSceneEntities  @ 0x822D8730  (cpp:2656)
//
// When a zone's real graphics load (lbZoneLoaded), remove the stand-in
// backdrop entities that represent it; when they unload, re-add them.
// =============================================================================
void
WorldEntityModule::UpdateBackdropSceneEntities( s32 liZoneNumber, bool lbZoneLoaded,
                                                WorldEntityIO::OutputBuffer_PreScene* lpOutputBuffer )
{
    if ( miCurrentBackdropZoneId == -1 )
    {
        return;
    }

    const s32 liBackdropListIndex = mWorldGraphicsStreamer.GetIndexFromId( miCurrentBackdropZoneId );
    if ( liBackdropListIndex == -1 )
    {
        return;
    }

    CgsGraphics::InstanceList* lpList = mWorldGraphicsStreamer.GetInstanceList( liBackdropListIndex );
    if ( !lpList )
    {
        return;
    }

    // The backdrop stand-ins are the tail entries beyond the complete instances.
    for ( s32 liInstance = static_cast<s32>( lpList->muNumInstances );
          liInstance < static_cast<s32>( lpList->muArraySize );
          liInstance++ )
    {
        CgsGraphics::Instance* lpInstance = lpList->GetInstance( liInstance );
        CGS_ASSERT( lpInstance, "lpInstance" );

        if ( static_cast<s32>( lpInstance->muBackdropZoneNumber ) != liZoneNumber )
        {
            continue;
        }

        if ( lbZoneLoaded )
        {
            if ( mIsBackdropInstanceInScene.IsBitSet( liInstance ) )
            {
                RemoveBackdropEntity( liBackdropListIndex, liInstance, lpOutputBuffer );
            }
        }
        else
        {
            if ( !mIsBackdropInstanceInScene.IsBitSet( liInstance ) )
            {
                AddBackdropEntity( lpInstance, liBackdropListIndex, liInstance, lpOutputBuffer );
            }
        }
    }
}

// =============================================================================
// RenderInstance  (cpp:134 -- DWARF-declared helper, inlined by the X360 into
// both GenerateDispatchLists loops; de-inlined here per the reconstruction
// rules. Shared per-instance path: LOD selection + technique pick + dispatch.)
// =============================================================================
void
WorldEntityModule::RenderInstance(
    CgsGraphics::Instance* lpInstance,
    bool lbShadow,
    Vector3::InParam lCameraPosition,
    f32 lfScaledDistanceSq,
    s32 liList,
    s32 liSortLayer,
    s32 liSortKey,
    CgsGraphics::DispatchFrame* lpDispatchFrame,
    const ShaderLodInfo* lpShaderLodInfo )
{
    CGS_ASSERT( lpInstance, "lpInstance" );

    CgsGraphics::Model* lpModel = lpInstance->mpModel;
    CGS_ASSERT( lpModel, "lpModel" );

    CGS_ASSERT( lpModel->GetNumLods() != 0, "Model in unit has no lods!" );
    CGS_ASSERT( lpModel->GetNumLods() == lpModel->GetNumRenderables(), "lpModel->GetNumLods() == lpModel->GetNumRenderables()" );

    // Distance LOD: the first LOD whose distance covers the (scaled) camera
    // distance. (X360: branchless sign-bit/min chain over the four LOD
    // distances; de-optimized to the loop form.)
    u32 luLodState = CgsGraphics::Model::E_STATE_INVALID;
    for ( u32 luLod = 0; luLod < lpModel->GetNumLods(); luLod++ )
    {
        const f32 lfLodDistance = lpModel->GetLodDistance( luLod );
        if ( lfScaledDistanceSq <= lfLodDistance * lfLodDistance )
        {
            luLodState = luLod;
            break;
        }
    }
    CGS_ASSERT( luLodState != CgsGraphics::Model::E_STATE_INVALID,
                    "leLodState != CgsGraphics::Model::E_STATE_INVALID" );

    if ( mbOverrideLod )
    {
        // X360 (both passes): with the override armed, an instance whose model lacks
        // the override state is NOT dispatched at all.
        if ( !lpModel->DoesStateExist( static_cast<CgsGraphics::Model::State>( miLodOverrideValue ) ) )
        {
            return;
        }
        luLodState = static_cast<u32>( miLodOverrideValue );
    }
    else
    {
        // Re-walk honouring DoesStateExist and the debug LOD-distance overrides.
        for ( u32 luLod = 0; luLod < lpModel->GetNumLods(); luLod++ )
        {
            if ( !lpModel->DoesStateExist( static_cast<CgsGraphics::Model::State>( luLod ) ) )
            {
                continue;
            }

            f32 lfLodDistance = lpModel->GetLodDistance( luLod );
            if ( mbOverrideLodDistances )
            {
                lfLodDistance = static_cast<f32>( mauOverrideLodDistances[ luLod ] );
            }

            if ( lfScaledDistanceSq <= lfLodDistance * lfLodDistance )
            {
                luLodState = luLod;
                break;
            }
        }
    }

    CGS_ASSERT( lpModel->DoesStateExist( static_cast<CgsGraphics::Model::State>( luLodState ) ), "lpModel->DoesStateExist( static_cast<CgsGraphics::Model::State>( luLodState ) )" );
    CGS_ASSERT( !lpModel->GetFlag( CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER ),
                "Instancing not supported. If required, enable D_WORLD_GRAPHICS_INSTANCING" );

    // Technique selection: shadow pass forces Z-only; otherwise the shader-LOD
    // policy decides from LOD 0's bounding sphere (ShaderLodInfo::GetLodTechnique).
    u8 luTechnique;
    if ( lbShadow )
    {
        luTechnique = static_cast<u8>( E_TECHNIQUE_ZONLY );
    }
    else
    {
        const Renderable* lpRenderable0 =
            lpModel->GetRenderable( CgsGraphics::Model::E_STATE_LOD_0 );

        luTechnique = static_cast<u8>( lpShaderLodInfo->GetLodTechnique(
            lpRenderable0->mBoundingSphere, lpInstance->mTransform, lCameraPosition ) );
    }

    const Renderable* lpRenderable =
        lpModel->GetRenderable( static_cast<CgsGraphics::Model::State>( luLodState ) );
    CGS_ASSERT( lpRenderable, "Missing renderable in a model" );
    CGS_ASSERT( lpDispatchFrame, "lpDispatchFrame" );

    CgsGraphics::DispatchList* lpDispatchList = lpDispatchFrame->GetList( liList );
    CGS_ASSERT( lpDispatchList, "lpDispatchList" );

    // Bind the instance transform for this draw's shader constants.
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 0, lpInstance->mTransform );

    const bool lbFirstInList = ( lpDispatchList->GetCount() & 0x7F ) == 0;

    // Argument values pinned from the X360 asm at both call sites (@0x822D64D0
    // shadow / @0x822D7054 camera): exclude byte 1 always; the shadow pass sends
    // list byte 1 + preZ true + thread byte 0xFF + trailing zeroes, the camera
    // pass sends the technique in the list byte, preZ false, then {0, 1, 0, 0}.
    lpDispatchFrame->GetBin().BeginPacket();
    if ( lbShadow )
    {
        CgsGraphics::DrawRenderable::AddToBin(
            lpRenderable, lpDispatchFrame, lbFirstInList,
            static_cast<s8>( liSortLayer ), static_cast<s8>( liSortLayer ),
            1, 1, true, 0xFF, 0, 0, 0 );
    }
    else
    {
        CgsGraphics::DrawRenderable::AddToBin(
            lpRenderable, lpDispatchFrame, lbFirstInList,
            static_cast<s8>( liSortLayer ), static_cast<s8>( liSortKey ),
            1, luTechnique, false, 0, 1, 0, 0 );
    }

    lpDispatchList->Submit( 0, lpDispatchFrame->GetBin().EndPacket() );

    if ( !lbShadow )
    {
        if ( mbMassiveGenerateImpressionData )
        {
            GenerateMassiveImpressionData( lpInstance, lCameraPosition );
        }
    }

    {
        // Debug: colour-coded bounding circles for the player-zone unit (the X360
        // runs this block in BOTH the camera and shadow passes).
        static const u32 KAU_LOD_COLOURS[ 5 ] =
            { 0xFF00FF00u, 0xFFFF0000u, 0xFF0000FFu, 0xFF00FFFFu, 0xFFFFFF00u };

        if ( mbDrawBoundingSpheres &&
             mWorldGraphicsStreamer.GetIndexFromId( miPlayerZoneNumber ) == siCurrentTrackUnitIndex )
        {
            CgsDev::DebugInterface lDebugInterface;

            if ( sbDrawInstanceCircles )
            {
                const CgsGraphics::Renderable* lpLodRenderable =
                    lpModel->GetRenderable( static_cast<CgsGraphics::Model::State>( luLodState ) );

                Vector3 lCentre = rw::math::vpu::TransformPoint(
                    lpInstance->mTransform, lpLodRenderable->mBoundingSphere.GetVector3() );
                const f32 lfRadius =
                    lpLodRenderable->mBoundingSphere.GetPlus() * GetMaximumScale( lpInstance->mTransform );

                lDebugInterface.GetRender().DrawCircle(
                    lCentre, ( lCentre - lCameraPosition ),
                    lfRadius, KAU_LOD_COLOURS[ luLodState ] );
            }
        }
    }
}

// =============================================================================
// GenerateDispatchLists  @ 0x822D5AB0  (cpp:1201)
//
// The renderer feed for the visible world set: every scene-manager-visible
// world entity id is resolved to its streamed instance, distance-culled,
// LOD-selected and dispatched. Two variants share RenderInstance: the camera
// pass and the shadow-map pass (the shadow pass biases the LOD distance by the
// shadow map's modifier and forces the shadow technique).
// =============================================================================
void
WorldEntityModule::GenerateDispatchLists(
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
    bool lbGenerateShadows )
{
    (void)lCameraViewProjection;
    (void)lCameraDirection;
    (void)lbGenerateShadows;

    CGS_ASSERT( liPreZList < 256, "liPreZList < 256" );

    // (The X360 also opens/unlinks a small dev performance-marker stack node around
    // this body -- the miGenerateDispatchListsPM instrumentation; dev-only, omitted.)
    lpInputBuffer->LockForRead();

    CgsGraphics::DispatchFrame* lpDispatchFrame = lpInputBuffer->GetDispatchFrame();
    ShadowMap* lpShadowMap = lpInputBuffer->GetShadowMap();
    CGS_ASSERT( lpShadowMap, "lpShadowMap" );

    const bool lbShadowPass = lpShadowMap->IsRenderingShadowMap() && lpShadowMap->IsUsingZOnlyRenderingPath();

    const f32 lfInvScaleSq = 1.0f / ( lfDrawDistanceScale * lfDrawDistanceScale );
    const u32 luNumEntities = lrVisibleEntities.GetLength();

    if ( lbShadowPass )
    {
        const f32 lfShadowLodModifier = lpShadowMap->CalcLodDistanceModifier().x;

        for ( u32 luEntity = 0; luEntity < luNumEntities; luEntity++ )
        {
            const CgsSceneManager::EntityId lEntityId = lrVisibleEntities.GetItem( luEntity );
            CGS_ASSERT( lEntityId.GetOwner() == KU_ENTITY_OWNER_WORLD,
                            "WorldEntityModule trying to render something that isn't world" );

            siCurrentTrackUnitIndex = lEntityId.GetEntityIndex();

            CgsGraphics::InstanceList* lpList =
                mWorldGraphicsStreamer.GetInstanceList( siCurrentTrackUnitIndex );

            CgsGraphics::Instance* lpInstance = lpList->GetInstance( lEntityId.GetPartIndex() );
            CGS_ASSERT( lpInstance, "lpInstance" );

            // The shadow pass biases the LOD distance by the shadow modifier.
            const f32 lfDistance =
                rw::math::vpu::Magnitude( lCameraPosition - lpInstance->mTransform.Pos() );
            const f32 lfBiased = lfDistance + lfShadowLodModifier;
            const f32 lfScaledDistanceSq = lfBiased * lfBiased * lfInvScaleSq;

            if ( lfScaledDistanceSq > lpInstance->mfMaxDrawDistanceSq )
            {
                continue;
            }

            RenderInstance( lpInstance, true, lCameraPosition, lfScaledDistanceSq,
                            liList, liSortLayer, liSortKey, lpDispatchFrame, lpShaderLodInfo );
        }
    }
    else
    {
        for ( u32 luEntity = 0; luEntity < luNumEntities; luEntity++ )
        {
            const CgsSceneManager::EntityId lEntityId = lrVisibleEntities.GetItem( luEntity );
            CGS_ASSERT( lEntityId.GetOwner() == KU_ENTITY_OWNER_WORLD,
                            "WorldEntityModule trying to render something that isn't world" );

            CgsGraphics::InstanceList* lpList =
                mWorldGraphicsStreamer.GetInstanceList( lEntityId.GetEntityIndex() );
            CGS_ASSERT( lpList, "lpList" );

            siCurrentTrackUnitIndex = lEntityId.GetEntityIndex();

            CgsGraphics::Instance* lpInstance = lpList->GetInstance( lEntityId.GetPartIndex() );
            CGS_ASSERT( lpInstance, "lpInstance" );

            const f32 lfScaledDistanceSq =
                rw::math::vpu::MagnitudeSquared( lCameraPosition - lpInstance->mTransform.Pos() )
                * lfInvScaleSq;

            if ( lfScaledDistanceSq > lpInstance->mfMaxDrawDistanceSq )
            {
                continue;
            }

            RenderInstance( lpInstance, false, lCameraPosition, lfScaledDistanceSq,
                            liList, liSortLayer, liSortKey, lpDispatchFrame, lpShaderLodInfo );
        }
    }

    lpInputBuffer->UnlockForRead();
}

// =============================================================================
// GenerateDispatchListsForEnvironmentMap  @ 0x822D7298  (cpp:1328)
//
// Environment-map feed: fixed LOD (miEnvironmentMapLOD), gated by that LOD's
// draw distance, technique from the shader-LOD info's env-map slot.
// =============================================================================
void
WorldEntityModule::GenerateDispatchListsForEnvironmentMap(
    const WorldEntityIO::InputBuffer_GenerateDispatchLists* lpInputBuffer,
    const Array<CgsSceneManager::EntityId, 4500u>& lrVisibleEntities,
    Matrix44::InParam lCameraViewProjection,
    Vector3::InParam lCameraPosition,
    const ShaderLodInfo* lpShaderLodInfo,
    s32 liList,
    s32 liSortLayer,
    s32 liSortKey )
{
    (void)lCameraViewProjection;

    lpInputBuffer->LockForRead();

    CgsGraphics::DispatchFrame* lpDispatchFrame = lpInputBuffer->GetDispatchFrame();
    const s32 liEnvironmentMapLOD = miEnvironmentMapLOD;

    const u32 luNumEntities = lrVisibleEntities.GetLength();

    for ( u32 luEntity = 0; luEntity < luNumEntities; luEntity++ )
    {
        const CgsSceneManager::EntityId lEntityId = lrVisibleEntities.GetItem( luEntity );
        CGS_ASSERT( lEntityId.GetOwner() == KU_ENTITY_OWNER_WORLD,
                        "WorldEntityModule trying to render something that isn't world" );

        CgsGraphics::InstanceList* lpList =
            mWorldGraphicsStreamer.GetInstanceList( lEntityId.GetEntityIndex() );
        CGS_ASSERT( lpList, "lpList != NULL" );

        CgsGraphics::Instance* lpInstance = lpList->GetInstance( lEntityId.GetPartIndex() );
        CGS_ASSERT( lpInstance, "lpInstance" );

        const f32 lfDistanceSq =
            rw::math::vpu::MagnitudeSquared( lCameraPosition - lpInstance->mTransform.Pos() );

        CgsGraphics::Model* lpModel = lpInstance->mpModel;
        CGS_ASSERT( lpModel, "lpModel" );

        if ( !lpModel->DoesStateExist( static_cast<CgsGraphics::Model::State>( liEnvironmentMapLOD ) ) )
        {
            continue;
        }

        const f32 lfLodDistance = lpModel->GetLodDistance( liEnvironmentMapLOD );
        if ( lfDistanceSq >= lfLodDistance * lfLodDistance )
        {
            continue;
        }

        const Renderable* lpRenderable =
            lpModel->GetRenderable( static_cast<CgsGraphics::Model::State>( liEnvironmentMapLOD ) );
        CGS_ASSERT( lpRenderable, "Missing renderable in a model" );
        CGS_ASSERT( lpDispatchFrame, "lpDispatchFrame" );

        CgsGraphics::DispatchList* lpDispatchList = lpDispatchFrame->GetList( liList );
        CGS_ASSERT( lpDispatchList, "lpDispatchList" );

        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 0, lpInstance->mTransform );

        const bool lbFirstInList = ( lpDispatchList->GetCount() & 0x7F ) == 0;

        // Argument values pinned from the X360 asm (@0x822D77A8): technique in the
        // list byte, exclude 1, preZ false, thread byte 0xFF, trailing zeroes.
        lpDispatchFrame->GetBin().BeginPacket();
        CgsGraphics::DrawRenderable::AddToBin(
            lpRenderable, lpDispatchFrame, lbFirstInList,
            static_cast<s8>( liSortLayer ), static_cast<s8>( liSortKey ),
            1, static_cast<u8>( lpShaderLodInfo->GetEnvMapTechnique() ),
            false, 0xFF, 0, 0, 0 );

        lpDispatchList->Submit( 0, lpDispatchFrame->GetBin().EndPacket() );
    }

    lpInputBuffer->UnlockForRead();
}

} // namespace BrnWorld
