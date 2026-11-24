// =============================================================================
// BrnWorld::WorldEntityModule -- GameSource/World/EntityModules/WorldEntityModule/
//                                BrnWorldEntityModule.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Function addresses are noted per body. Declaration shape from the DecFIGS
// DWARF; LegacySrc used for idiom/inlining shapes only.
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
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
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
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"

#include "GameShared/GameClasses/Geometric/Primitives/CgsFrustum.h"   // [DIAG] BRN_BACKDROP_DIAG plane batch

#include <xtl.h>
#include <cmath>
#include <cstdio>  // snprintf (the [skid-bind] surfacelist resolve probe)
#include <cstdlib> // getenv ([DIAG] BRN_BACKDROP_DIAG opt-in)

// The global runtime shader-constant register (X360 symbol mShaderConstantTable;
// bodied by the CgsShaderConstants TU). Slot 0 holds the per-draw world transform.
static const char KAC_ARTIST_ASSERT_FILE[] =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
    "../World/EntityModules/WorldEntityModule/BrnWorldEntityModule.cpp";

namespace CgsGraphics { extern ShaderConstantTable mShaderConstantTable; }

namespace BrnWorld
{

// BrnWorldEntityModule.cpp:109 (DWARF: WorldEntityModule::_mbAllowStreamStalling).
// X360 byte_82CDB588. When set, PostPhysicsUpdate only reports the world as
// streamed once the player + required zones are resident.
bool _mbAllowStreamStalling = true;

// X360 dword_82FAD9B4 -- UpdateStream's warm-up counter: the stream does not run
// for the first few frames after boot (static, file scope).
static s32 siUpdateStreamFrameCounter = 0;

// X360 dword_82FAD308 -- debug: the track-unit index currently being rendered
// (read by the instance-render debug path).
static s32 siCurrentTrackUnitIndex = 0;

// X360 byte_82CDB6B8 -- debug gate for the per-instance bounding-circle draw.
static bool sbDrawInstanceCircles = true;

// Scene-manager entity owner id for world instances (the X360 EntityId::Set owner 5).
static const u32 KU_ENTITY_OWNER_WORLD = 5;

// WorldEntityModule TU initializer @0x82C4B988: StringToKey("340654") is stored
// as a full 64-bit collection key and loaded by PrepareSurfaceList @0x822F9C58.
static const u64 gs_uSurfaceListKey = Attrib::StringToKey("340654");

// X360 rodata flt_82014460 == FLT_EPSILON. PrepareSurfaceList splats this scalar
// across the four lanes of the sample-colour comparison.
static const f32 KF_SURFACE_SANITY_EPSILON = 1.1920928955078125e-07f;

// ARTIST822D6588/65B0 first take the componentwise maximum of the
// three basis vectors, then its magnitude (two rsqrt refinements and zero select).
static f32 GetMaximumScale( const Matrix44Affine& lrTransform )
{
    const XMVECTOR right = __lvx(&lrTransform.xAxis, 0);
    const XMVECTOR up = __lvx(&lrTransform.yAxis, 0);
    const XMVECTOR at = __lvx(&lrTransform.zAxis, 0);
    const XMVECTOR maximum = __vmaxfp(__vmaxfp(right, up), at);
    const XMVECTOR square = __vmsum3fp(maximum, maximum);
    const XMVECTOR zero = XMVectorZero();
    const XMVECTOR one = XMVectorReplicate(1.0f);
    const XMVECTOR half = XMVectorReplicate(0.5f);
    XMVECTOR estimate = __vrsqrtefp(square);
    for (s32 i = 0; i < 2; ++i)
    {
        const XMVECTOR squaredEstimate = __vmulfp(estimate, estimate);
        const XMVECTOR scaledEstimate = __vmulfp(estimate, half);
        const XMVECTOR residual = __vnmsubfp(square, squaredEstimate, one);
        estimate = __vmaddfp(scaledEstimate, residual, estimate);
    }
    return __vsel(__vmulfp(square, estimate), zero, __vcmpeqfp(square, zero)).x;
}

// ARTIST822BB150; DecFIGS BrnWorldEntityModule.h:632. The Surfaces element
// is a 24-byte RefSpec; ld8 reads its full collection key, not the low word.
void WorldEntityModule::GetSurface(u8 luSurfaceId, Attrib::Gen::surface* lpOutSurface) const
{
    if (lpOutSurface == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpOutSurface != NULL",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\world\\EntityModules/WorldEntityModule/BrnWorldEntityModule.h",
            634);
        CgsDev::Assert::EndAssert();
    }
    const Attrib::RefSpec* lpRef =
        static_cast<const Attrib::RefSpec*>(mSurfaceList.Surfaces(luSurfaceId));
    if (lpRef == 0)
        lpRef = static_cast<const Attrib::RefSpec*>(Attrib::DefaultDataArea(0x18u));
    Attrib::Collection* lpCollection = Attrib::FindCollectionWithDefault(
        0x68428A3C7836CF50ull, lpRef->GetCollectionKey());
    lpOutSurface->Change(lpCollection);
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
    miNumMassiveSubscribersCreated = 0;
    mbMassiveSubscribersCreated = false;
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

            // The three common-data bundle loads (PVS wave 2026-07-27: UN-GATED -- the world
            // units import their textures out of WORLDTEX.BIN, so with these skipped every
            // streamed TRK_UNIT bundle failed to resolve its imports).
            lpOutputBuffer->GetResourceRequestInterface()->LoadBundle(
                &mReceiverQueue, 1, 3 /* open-world graphics pool (GameDataModule pool table id 3) */, "WORLDTEX.BIN", false );
            lpOutputBuffer->GetResourceRequestInterface()->LoadBundle(
                &mReceiverQueue, 0, 3 /* open-world graphics pool (GameDataModule pool table id 3) */, "GLOBALPROPS.BIN", false );
            lpOutputBuffer->GetResourceRequestInterface()->LoadBundle(
                &mReceiverQueue, 2, 3 /* open-world graphics pool (GameDataModule pool table id 3) */, "GLOBALBACKDROPS.BNDL", false );
        }
        // fall through

        case E_PREPARESTAGE_COMMONDATA_LOADING:
        {
            mePrepareStage = E_PREPARESTAGE_COMMONDATA_LOADING;

            if ( mReceiverQueue.GetLength() < 3 )
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
            CGS_ASSERT(false, CgsDev::Assert::MessageAt(
                "Invalid Stage\n",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
                "../World/EntityModules/WorldEntityModule/BrnWorldEntityModule.cpp", 631));  // ARTIST 0x823029DC
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
        lRequest.mPosition = Vector4( mPositionUsedForPVS.mV );
        lRequest.miLookupIndex = mPlayerZoneResponse.GetLookupIndex();
        lRequest.mbUseVelocity = 1;
        lRequest.mVelocity = Vector4( lpRaceCarState->mLinearVelocity.mV );

        mPVSModule.GetInputInterface()->mZoneRequestQueue.AddEvent( lRequest );
    }
    else
    {
        mPositionUsedForPVS = lSimulatedCameraPosition;

        PVSIO::GetZoneRequest lRequest;
        lRequest.miPlayerIndex = 0;
        lRequest.mPosition = Vector4( mPositionUsedForPVS.mV );
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
    CGS_ASSERT( lpOutputBuffer,
        CgsDev::Assert::MessageAt("lpOutputBuffer", KAC_ARTIST_ASSERT_FILE, 1098 /* ARTIST 0x82307FE4 */));

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
    // X360: `ld r31,0x2628(this)` -- the WHOLE 64-bit zone id is both the MakeTrackUnitId
    // argument (which reads its low half) and the streamer user id.
    bool lbWorldStreamed = mWorldGraphicsStreamer.IsAssetLoaded(
        BrnResource::MakeTrackUnitId( mPlayerZoneResponse.GetZoneNumber( 0 ) ),
        mPlayerZoneResponse.GetZoneId( 0 ) );

    for ( u32 luZone = 1; luZone < static_cast<u32>( mPlayerZoneResponse.GetNumZones() ); luZone++ )
    {
        if ( !mPlayerZoneResponse.IsZoneRequired( luZone ) )
        {
            continue;
        }

        if ( !mWorldGraphicsStreamer.IsAssetLoaded(
                 BrnResource::MakeTrackUnitId( mPlayerZoneResponse.GetZoneNumber( luZone ) ),
                 mPlayerZoneResponse.GetZoneId( luZone ) ) )
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

    PVSIO::OutputInterface* lpPVSOutput = mPVSModule.GetOutputInterface();

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

        // X360: `ld r28,0(r25)` reads the whole 64-bit zone id; `clrlwi r3,r28,0` feeds its
        // low half to MakeTrackUnitId while the FULL id goes in as the streamer user id
        // (r6). The safe flag is mabZoneSafe[i] (`lbz r29,-0x60(r27)` off mabZoneRequired).
        mWorldGraphicsStreamer.AddEntry(
            BrnResource::MakeTrackUnitId( liZoneNumber ),
            mPlayerZoneResponse.IsZoneSafe( luZone ),
            mPlayerZoneResponse.GetZoneId( luZone ) );

        // `lbz r11,0(r27)` == mabZoneRequired[i] -- the IMMEDIATE-flag zones are the ones
        // whose prop instances have to be resident.
        if ( mPlayerZoneResponse.IsZoneRequired( luZone ) )
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

        const CgsGraphics::Model* lpModel = ( lpInstance != 0 ) ? lpInstance->mpModel.Get() : 0;
        CGS_ASSERT( lpModel, "lpModel" );

        CGS_ASSERT( lpModel->DoesStateExist( CgsGraphics::Model::E_STATE_LOD_0 ), "lpModel->DoesStateExist( CgsGraphics::Model::E_STATE_LOD_0 )" );

        const Renderable* lpRenderable =
            lpModel->GetRenderable( CgsGraphics::Model::E_STATE_LOD_0 );
        CGS_ASSERT( lpRenderable, "lpRenderable" );
        if ( lpRenderable == 0 )
        {
            continue;   // same degraded-load gate as above
        }

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

            // A STALE SURFACELIST.BIN makes this rebind die in Attrib::Collection::GetData
            // ("Cannot get non-array data from a non-zero index", then an AV): the vault must
            // come from the CURRENT attribsys-vault converter (build_game_data.py --only
            // "SURFACELIST.BIN" --force). Data from the pre-built drop predates it.
            mSurfaceList.ChangeWithDefault( gs_uSurfaceListKey );

            // Breaker loads the first four floats of surface element 1 as a genuine
            // Vector4 sample colour. The comparison threshold is the scalar epsilon
            // splatted by vspltw; CR6.EQ is set only when no lane exceeds it.
            void* lpSampleRefData = mSurfaceList.Surfaces( 1 );
            if ( !lpSampleRefData )
                lpSampleRefData = Attrib::DefaultDataArea( sizeof( Attrib::RefSpec ) );

            Attrib::RefSpec* lpSampleRef = static_cast<Attrib::RefSpec*>( lpSampleRefData );
            Attrib::Gen::surface lSampleSurface(
                const_cast<Attrib::Collection*>( lpSampleRef->GetCollection() ), 0 );
            const f32* lpfSampleColour =
                static_cast<const f32*>( lSampleSurface.GetAttributeData() );
            CGS_ASSERT( std::fabs( lpfSampleColour[0] ) > KF_SURFACE_SANITY_EPSILON ||
                            std::fabs( lpfSampleColour[1] ) > KF_SURFACE_SANITY_EPSILON ||
                            std::fabs( lpfSampleColour[2] ) > KF_SURFACE_SANITY_EPSILON ||
                            std::fabs( lpfSampleColour[3] ) > KF_SURFACE_SANITY_EPSILON,
                        "Surface list appears to be corrupt" );

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
    CGS_ASSERT( lpRequestInterface && lpSceneInterface,
        CgsDev::Assert::MessageAt(
            "lpRequestInterface && lpSceneInterface",
            KAC_ARTIST_ASSERT_FILE, 2117 /* ARTIST 0x8230692C */));

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
                    // The reply carries the number of handles written into the
                    // caller-owned array.
                    const CgsResource::Events::AcquireResourceListResponse* lpResponse =
                        reinterpret_cast<const CgsResource::Events::AcquireResourceListResponse*>( lpEventData );

                    if ( lpResponse->miNumHandles > 0 )
                    {
                        AddCollisionZoneToSceneManager(
                            lpRequestInterface, lpSceneInterface,
                            lpResponse->mpHandles, lpResponse->miNumHandles );
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
    Matrix44::InParam lCameraViewProjection,
    Vector3::InParam lCameraPosition,
    Vector3::InParam lCameraDirection,
    f32 lfScaledDistanceSq,
    s32 liList,
    s32 liSortLayer,
    s32 liSortKey,
    u8 lu8PreZList,
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
    if (!lbShadow)
    {
    }
    CGS_ASSERT( lpRenderable, "Missing renderable in a model" );
    CGS_ASSERT( lpDispatchFrame, "lpDispatchFrame" );

    CgsGraphics::DispatchList* lpDispatchList = lpDispatchFrame->GetList( liList );
    CGS_ASSERT( lpDispatchList, "lpDispatchList" );

    // Bind the instance transform for this draw's shader constants.
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 0, lpInstance->mTransform );

    const bool lbFirstInList = ( lpDispatchList->GetCount() & 0x7F ) == 0;

    // Argument values pinned from the X360 asm at both call sites (@0x822D64D0
    // shadow / @0x822D7054 camera). The four STACK arguments (AddToBin reads them
    // from its arg_57 / arg_5F / arg_64 / arg_6F slots @0x827FA21C..0x827FA230 and
    // stores them into the trailer as mu8PreZList / mu8PreZTechniqueIndex /
    // mi8InstanceCount / mu8ExcludeMeshBits) are:
    //   shadow  @0x822D64B0..0x822D64CC : preZList = 0xFF (literal), preZTechnique 0,
    //                                     instanceCount 0, excludeBits 0
    //   camera  @0x822D7010..0x822D7040 : preZList = the computed byte (see
    //                                     lu8PreZList), preZTechnique = 1 (li r6, 1),
    //                                     instanceCount 0, excludeBits 0
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
        // CORRECTED (world-pixels wave 2026-07-28) to AddToBin's asm-attested arg roles
        // (renderer_wave_log.md: renderable, frame, rebuildDirty, OPAQUE LIST s8,
        // TECHNIQUE s8, frustumEnable, TRANSPARENT LIST u8, zOnly, preZList,
        // preZTechnique, instanceCount, excludeBits). The previous mapping put liSortKey
        // in the technique slot and the LOD technique in the transparent-list slot, so a
        // transparent world mesh would have been routed into mesh list 0/1/2 -- the
        // SHADOW cascades. liSortLayer/liSortKey are the world opaque/transparent mesh
        // list ids (11/15); the LOD technique picked above is the technique byte.
        // ⭐ CORRECTED (race-car render wave 2026-07-31). The 5th/7th arguments were
        // swapped -- the technique went into the TRANSPARENT-LIST slot and the transparent
        // list id into the TECHNIQUE slot. Interpret's decode carried the mirror-image
        // swap, so the emitted word1 bytes (and therefore the pixels) were already right;
        // both sides are corrected together, so this call still emits exactly the same
        // word1 = 11<<24 | 15<<16 | technique<<8 | 1. Proof: the X360 camera-pass call site
        // @0x822D7010..0x822D7054 loads r6 = extsb(arg_6C) and r7 = extsb(arg_74) -- BOTH of
        // the world module's list-id parameters -- and r9 = the computed technique.
        CgsGraphics::DrawRenderable::AddToBin(
            lpRenderable, lpDispatchFrame, lbFirstInList,
            static_cast<s8>( liSortLayer ), static_cast<s8>( liSortKey ),
            1, static_cast<u8>( luTechnique ), false, lu8PreZList, 1, 0, 0 );
    }

    lpDispatchList->Submit( 0, lpDispatchFrame->GetBin().EndPacket() );

    if ( !lbShadow )
    {
        if ( mbMassiveGenerateImpressionData )
        {
            GenerateMassiveImpressionData( lpInstance->mTransform, lCameraViewProjection, lpModel,
                                           lCameraPosition, lCameraDirection );
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
    const CgsContainers::Array<CgsSceneManager::EntityId, 4500u>& lrVisibleEntities,
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
    (void)lbGenerateShadows;

    CGS_ASSERT( liPreZList < 256, "liPreZList < 256" );

    // @0x822D5B3C: the pre-Z list byte every camera-pass DRAWRENDERABLE trailer carries.
    // 255 (= "no pre-Z re-emit") unless the caller asked for a positive list id.
    const u8 lu8PreZList = ( liPreZList > 0 ) ? static_cast<u8>( liPreZList ) : 0xFFu;

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
        const f32 lfShadowLodModifier = lpShadowMap->CalcLodDistanceModifier().mV.x;

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

            // The shadow loop's own AddToBin sends a literal 0xFF pre-Z byte.
            RenderInstance( lpInstance, true, lCameraViewProjection, lCameraPosition, lCameraDirection,
                            lfScaledDistanceSq,
                            liList, liSortLayer, liSortKey, 0xFFu,
                            lpDispatchFrame, lpShaderLodInfo );
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

            RenderInstance( lpInstance, false, lCameraViewProjection, lCameraPosition, lCameraDirection,
                            lfScaledDistanceSq,
                            liList, liSortLayer, liSortKey, lu8PreZList,
                            lpDispatchFrame, lpShaderLodInfo );
        }
    }

    lpInputBuffer->UnlockForRead();
}

// =============================================================================
// (GenerateDispatchListsFromStreamer DELETED 2026-07-29, culling wave: the real
//  frustum-test path is live -- the scene manager registers every streamed
//  instance, LooseOctree::StartFrustumTestJobs answers the camera query and
//  WorldModule::FilterFrustumTestResults hands the visible ids to
//  GenerateDispatchLists above. The streamer-walking stand-in that fed the real
//  feed while the scene manager was an inert gate has no callers left.)
// =============================================================================

// =============================================================================
// GenerateDispatchListsForEnvironmentMap  @ 0x822D7298  (cpp:1328)
//
// Environment-map feed: fixed miEnvironmentMapLOD and its authored distance;
// technique comes from the shader-LOD info env-map slot.
// =============================================================================
void
WorldEntityModule::GenerateDispatchListsForEnvironmentMap(
    const WorldEntityIO::InputBuffer_GenerateDispatchLists* lpInputBuffer,
    const CgsContainers::Array<CgsSceneManager::EntityId, 4500u>& lrVisibleEntities,
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
            continue;
        const f32 lfLodDistance = lpModel->GetLodDistance( static_cast<u32>(liEnvironmentMapLOD) );
        if ( !(lfDistanceSq < lfLodDistance * lfLodDistance) )
            continue;

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

// FOLDED FROM BrnWorldEntityModule_wS34_00.cpp
// BrnWorld::WorldEntityModule -- the Massive (in-game advertising) slice: prepare (bundles,
// advert textures, lookup table), per-frame subscriber creation, and the per-instance impression pass.

#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/texture.h"

#include <cmath>   // std::fabs, std::sqrt
#include <cstdio>
#include <cstdlib>
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Graphics/CgsModel.h"
#include "GameShared/GameClasses/Graphics/Dispatch/Renderable.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"     // AcquireResourceResponse
#include "rw/math/vpu/matrix44_operation.h"                                  // Mult(Matrix44Affine, Matrix44)
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ClientCore.h"              // CMassiveClientCore::Instance

namespace BrnWorld
{

namespace
{
    // The game-data pool the Massive bundles and resources live in.
    const s32 KI_MASSIVE_POOL = 10;

    // The advert textures (one per impression event) acquired from MASSIVETEXTUREDICTIONARY.BIN.
    const s32 KI_NUM_MASSIVE_TEXTURES = BrnMassive::KI_NUM_IMPRESSION_EVENTS;
    const char* const gapcMassiveTextureNames[KI_NUM_MASSIVE_TEXTURES] =
    {
        "gamedb://burnout5/Burnout/Content_World/Images_Final/Massive/billboard_landscape01.TextureConfig2d?ID=438178",
        "gamedb://burnout5/Burnout/Content_World/Images_Final/Massive/billboard_landscape02.TextureConfig2d?ID=438180",
        "gamedb://burnout5/Burnout/Content_World/Images_Final/Massive/billboard_landscape03.TextureConfig2d?ID=438061",
        "gamedb://burnout5/Burnout/Content_World/Images_Final/Massive/billboard_landscape04.TextureConfig2d?ID=438063",
        "gamedb://burnout5/Burnout/Content_World/Images_Final/Massive/billboard_landscape06.TextureConfig2d?ID=438067",
        "gamedb://burnout5/Burnout/Content_World/Images_Final/Massive/billboard_landscape07.TextureConfig2d?ID=557797",
        "gamedb://burnout5/Burnout/Content_World/Images_Final/Massive/billboard_landscape08.TextureConfig2d?ID=557799",
        "gamedb://burnout5/Burnout/Content_World/Images_Final/Massive/billboard_landscape09.TextureConfig2d?ID=557801",
        "gamedb://burnout5/Burnout/Content_World/Images_Final/Massive/billboard_landscape10.TextureConfig2d?ID=557803",
    };

    // The renderable massive index of a world renderable that draws no advert.
    const u8 KU_NO_MASSIVE_INDEX = 0xFF;

    // The reference screen the impression's projected size is measured on.
    const u16 KU_IMPRESSION_SCREEN_WIDTH  = 1280;
    const u16 KU_IMPRESSION_SCREEN_HEIGHT = 720;

    // A lookup-table item is reported only when at least this many of its eight box corners
    // lie inside the view volume.
    const s32 KI_MIN_CORNERS_IN_VIEW = 4;

    // Massive is paused outside the in-game update set (bit 3) and whenever bit 0 is set.
    const BrnUpdateSet KU_UPDATE_SET_MASSIVE_PAUSE = 0x1;
    const BrnUpdateSet KU_UPDATE_SET_IN_GAME       = 0x8;

    const CgsDev::RGBA KU_ADVERT_DEBUG_COLOUR      = 0xFFFF0000u;
    const CgsDev::RGBA KU_ADVERT_OUT_OF_RANGE_COLOUR = 0xFF0000FFu;

    // The header helper that requests a fixed table of named resources and collects the
    // responses by event id.
    template <s32 N>
    struct ResourceLoaderHelper
    {
        explicit ResourceLoaderHelper(const char* const* lpapcNames) : mpapcNames(lpapcNames) {}

        void RequestResource(s32 liIndex, CgsModule::BaseEventReceiverQueue* lpReceiverQueue,
                             WorldEntityIO::ResourceRequestInterface* lpRequestInterface, s32 liPoolId)
        {
            CGS_ASSERT(liIndex < N, "ResourceLoaderHelper: resource request out of range");
            lpRequestInterface->AcquireResource(lpReceiverQueue, liIndex, liPoolId, mpapcNames[liIndex]);
        }

        void SetResource(s32 liIndex, void* lpResource)
        {
            CGS_ASSERT(lpResource, "ResourceLoaderHelper::SetResource trying to set a null resource pointer");
            CGS_ASSERT(liIndex < N, "ResourceLoaderHelper::SetResource resource index out of range");
            mapResources[liIndex] = lpResource;
        }

        void* GetResource(s32 liIndex) const
        {
            CGS_ASSERT(liIndex < N, "liIndex < NumEntries");
            return mapResources[liIndex];
        }

        void*              mapResources[N];
        const char* const* mpapcNames;
    };

    // Row-vector point transform through a full 4x4 (w = 1).
    Vector4 TransformToClip(Matrix44::InParam lrMatrix, Vector3::InParam lrPoint)
    {
        const f32 lfPointX = lrPoint.GetX();
        const f32 lfPointY = lrPoint.GetY();
        const f32 lfPointZ = lrPoint.GetZ();
        return Vector4(
            lfPointX * lrMatrix.xAxis.GetX() + lfPointY * lrMatrix.yAxis.GetX() +
                lfPointZ * lrMatrix.zAxis.GetX() + lrMatrix.wAxis.GetX(),
            lfPointX * lrMatrix.xAxis.GetY() + lfPointY * lrMatrix.yAxis.GetY() +
                lfPointZ * lrMatrix.zAxis.GetY() + lrMatrix.wAxis.GetY(),
            lfPointX * lrMatrix.xAxis.GetZ() + lfPointY * lrMatrix.yAxis.GetZ() +
                lfPointZ * lrMatrix.zAxis.GetZ() + lrMatrix.wAxis.GetZ(),
            lfPointX * lrMatrix.xAxis.GetW() + lfPointY * lrMatrix.yAxis.GetW() +
                lfPointZ * lrMatrix.zAxis.GetW() + lrMatrix.wAxis.GetW());
    }

    // Clip space -> the reference screen: ((clip / w) + 1) * 0.5 * (1280, 720).
    Vector3 ClipToScreen(const Vector4& lrClip)
    {
        const f32 lfInvW = 1.0f / lrClip.GetW();
        return Vector3(
            (lrClip.GetX() * lfInvW + 1.0f) * 0.5f * static_cast<f32>(KU_IMPRESSION_SCREEN_WIDTH),
            (lrClip.GetY() * lfInvW + 1.0f) * 0.5f * static_cast<f32>(KU_IMPRESSION_SCREEN_HEIGHT),
            0.0f);
    }
}

// =============================================================================
// PrepareMassive -- load the Massive bundles, acquire the nine advert textures into
// mapDynamicAdvertTextures, then acquire the lookup table and prepare BrnMassive with it.
// =============================================================================
bool
WorldEntityModule::PrepareMassive( WorldEntityIO::OutputBuffer_Prepare* lpOutputBuffer )
{
    ResourceLoaderHelper<KI_NUM_MASSIVE_TEXTURES> lTextureLoader( gapcMassiveTextureNames );

    switch ( meMassivePrepareStage )
    {
        case E_EMASSIVEPREPARESTAGE_START:
        case E_LOADBUNDLES:
        {
            meMassivePrepareStage = E_LOADBUNDLES;
            mReceiverQueue.Clear();
            lpOutputBuffer->GetResourceRequestInterface()->LoadBundle(
                &mReceiverQueue, 1, KI_MASSIVE_POOL, "MASSIVETABLE.BIN", false );
            lpOutputBuffer->GetResourceRequestInterface()->LoadBundle(
                &mReceiverQueue, 0, KI_MASSIVE_POOL, "MASSIVETEXTUREDICTIONARY.BIN", false );
        }
        // fall through

        case E_WLOADBUNDLES:
        {
            meMassivePrepareStage = E_WLOADBUNDLES;
            if ( mReceiverQueue.GetLength() < 2 )
            {
                return false;
            }
        }
        // fall through

        case E_REQUESTTEXTURES:
        {
            meMassivePrepareStage = E_REQUESTTEXTURES;
            mReceiverQueue.Clear();
            for ( s32 liTexture = 0; liTexture < KI_NUM_MASSIVE_TEXTURES; ++liTexture )
            {
                lTextureLoader.RequestResource( liTexture, &mReceiverQueue,
                                                lpOutputBuffer->GetResourceRequestInterface(),
                                                KI_MASSIVE_POOL );
            }
        }
        // fall through

        case E_WREQUESTTEXTURES:
        {
            meMassivePrepareStage = E_WREQUESTTEXTURES;
            if ( mReceiverQueue.GetLength() < KI_NUM_MASSIVE_TEXTURES )
            {
                return false;
            }

            if ( mReceiverQueue.GetLength() > 0 )
            {
                const CgsModule::Event* lpEventData = 0;
                s32 liSize = 0;
                mReceiverQueue.GetFirstEvent( &lpEventData, &liSize );
                while ( lpEventData != 0 )
                {
                    const CgsResource::Events::AcquireResourceResponse* lpResponse =
                        reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>( lpEventData );
                    lTextureLoader.SetResource( lpResponse->miEventId, lpResponse->mpResourceMemory );
                    CGS_ASSERT( lpResponse->miEventId < KI_NUM_MASSIVE_TEXTURES,
                                "Unexpected Event Id in BrnWorld::WorldEntityModule::Prepare" );
                    mReceiverQueue.GetNextEvent( lpEventData, &lpEventData, &liSize );
                }
            }

            for ( s32 liTexture = 0; liTexture < KI_NUM_MASSIVE_TEXTURES; ++liTexture )
            {
                CGS_ASSERT( liTexture < KU_MAX_NUM_DYNAMIC_ADVERTS,
                            "liResourceCount < KU_MAX_NUM_DYNAMIC_ADVERTS" );
                mapDynamicAdvertTextures[ liTexture ] =
                    *static_cast<renderengine::Texture**>( lTextureLoader.GetResource( liTexture ) );
            }
        }
        // fall through

        case E_REQUESTMASSIVETABLE:
        {
            meMassivePrepareStage = E_REQUESTMASSIVETABLE;
            mReceiverQueue.Clear();
            lpOutputBuffer->GetResourceRequestInterface()->AcquireResource(
                &mReceiverQueue, 0, KI_MASSIVE_POOL, "MassiveTable" );
        }
        // fall through

        case E_WREQUESTMASSIVETABLE:
        {
            meMassivePrepareStage = E_WREQUESTMASSIVETABLE;
            if ( mReceiverQueue.GetLength() < 1 )
            {
                return false;
            }

            const CgsModule::Event* lpEventData = 0;
            s32 liSize = 0;
            mReceiverQueue.GetFirstEvent( &lpEventData, &liSize );
            const CgsResource::Events::AcquireResourceResponse* lpResponse =
                reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>( lpEventData );
            if ( !mMassive.Prepare( *static_cast<BrnMassive::MassiveLookupTable**>( lpResponse->mpResourceMemory ) ) )
            {
                return false;
            }
        }
        // fall through

        case E_EMASSIVEPREPARESTAGE_DONE:
        {
            meMassivePrepareStage = E_EMASSIVEPREPARESTAGE_DONE;
            return true;
        }

        default:
        {
            return false;
        }
    }
}

// =============================================================================
// UpdateMassive -- tick BrnMassive, mirror the pause state, and, while the client is live
// and downloads are allowed, create the next advert's subscriber once the previous one
// has its ad (or timed out). Re-arms the per-frame impression latch.
// =============================================================================
void
WorldEntityModule::UpdateMassive( BrnUpdateSet lUpdateSet )
{
    mMassive.Update();
    mMassive.SetIsPaused( ( lUpdateSet & KU_UPDATE_SET_IN_GAME ) == 0 ||
                          ( lUpdateSet & KU_UPDATE_SET_MASSIVE_PAUSE ) != 0 );

    if ( MassiveAdClient3::CMassiveClientCore::Instance() != 0 &&
         !mbMassiveSubscribersCreated &&
         mbDownloadMassiveTextures &&
         mMassive.IsSafeToDownload() )
    {
        bool lbCreateNext = true;

        if ( miNumMassiveSubscribersCreated - 1 >= 0 )
        {
            const BrnMassive::BrnMassiveSubscriber* lpPrevious =
                mMassive.GetSubscriberAtIndex( miNumMassiveSubscribersCreated - 1 );
            CGS_ASSERT( lpPrevious, "Invalid Subscriber has been created" );
            if ( lpPrevious != 0 && !( lpPrevious->miState == 3 || lpPrevious->miState == 2 ) )
            {
                lbCreateNext = false;
            }
        }

        if ( lbCreateNext && miNumMassiveSubscribersCreated < KI_NUM_MASSIVE_TEXTURES )
        {
            renderengine::Texture* lpTexture = mapDynamicAdvertTextures[ miNumMassiveSubscribersCreated ];

            // ARTIST 0x822A83B8..8464: read mip count, then format/depth/height/width/type.
            const u32 luNumLevels = lpTexture->Format.MaxMipLevel + 1;
            const renderengine::PixelFormat leFormat = lpTexture->GetFormat();
            const u32 luDepth = lpTexture->GetDepth();
            const u32 luHeight = lpTexture->GetHeight();
            const u32 luWidth = lpTexture->GetWidth();
            const renderengine::Texture::Type leType = lpTexture->GetType();
            const u32 luPixelDataSize = renderengine::Texture::GetPixelDataSize(
                leType, luWidth, luHeight, luDepth, leFormat, luNumLevels);
            void* lpPixelData = reinterpret_cast<void*>(lpTexture->Format.BaseAddress << 12);
            mMassive.CreateSubscriber(miNumMassiveSubscribersCreated, lpPixelData, luPixelDataSize);
            ++miNumMassiveSubscribersCreated;
        }

        if ( miNumMassiveSubscribersCreated == KI_NUM_MASSIVE_TEXTURES )
        {
            mbMassiveSubscribersCreated = true;
        }
    }

    mMassive.mbImpressionFrameStarted = false;
}

// =============================================================================
// GenerateMassiveImpressionData -- for a dispatched world instance whose renderable draws
// an advert: when the advert is within the impression distance, project its lookup-table
// box and feed the subscriber the in-view flag, the facing term and the projected size.
// An advert in range whose subscriber does not exist yet blocks subscriber creation.
// =============================================================================
void
WorldEntityModule::GenerateMassiveImpressionData( Matrix44Affine::InParam lTransform,
                                                  Matrix44::InParam lCameraViewProjection,
                                                  CgsGraphics::Model* lpModel,
                                                  Vector3::InParam lCameraPosition,
                                                  Vector3::InParam lCameraDirection )
{
    if ( !mMassive.mbImpressionFrameStarted )
    {
        mMassive.mbSafeToDownload = true;
        mMassive.mbImpressionFrameStarted = true;
    }

    const Renderable* lpRenderable = lpModel->GetRenderable( CgsGraphics::Model::E_STATE_LOD_0 );
    CGS_ASSERT( lpRenderable, "lpRenderable" );

    // The renderable's massive index is the high byte of its flags.
    const u8 luRenderableIndex = static_cast<u8>( lpRenderable->mu16Flags >> 8 );
    if ( luRenderableIndex == KU_NO_MASSIVE_INDEX )
    {
        return;
    }

    BrnMassive::MassiveLookupTableItem* lpItem = mMassive.GetSubscriberDataAtIndex( luRenderableIndex );
    if ( lpItem == 0 )
    {
        return;
    }

    BrnMassive::BrnMassiveSubscriber* lpSubscriber = lpItem->mpSubscriber;
    if ( lpSubscriber != 0 )
    {
        lpSubscriber->SetImpressionData( false, 0.0f, 0, KU_IMPRESSION_SCREEN_WIDTH, KU_IMPRESSION_SCREEN_HEIGHT );
    }

    const f32 lfDistance = rw::math::vpu::Magnitude( lCameraPosition - lTransform.Pos() );

    if ( mbMassiveDistanceLinesDebug )
    {
        DEBUG_RenderAdvertLineTest( lfDistance, lTransform.Pos(), lCameraPosition, lCameraDirection );
    }

    if ( lfDistance > mfMassiveImpressionDebugDistance )
    {
        return;
    }

    if ( lpSubscriber == 0 )
    {
        mMassive.mbImpressionFrameStarted = true;
        mMassive.mbSafeToDownload = false;
        return;
    }

    const Vector3 lBoundingBoxMin = lpItem->mBoundingBoxMin;
    const Vector3 lBoundingBoxMax = lpItem->mBoundingBoxMax;

    // The eight box corners (bit 0 = x, bit 1 = y, bit 2 = z take the maximum).
    Vector3 laCorners[8];
    for ( s32 liCorner = 0; liCorner < 8; ++liCorner )
    {
        laCorners[ liCorner ] = Vector3(
            static_cast<f32>(( liCorner & 1 ) ? lBoundingBoxMax.GetX() : lBoundingBoxMin.GetX()),
            static_cast<f32>(( liCorner & 2 ) ? lBoundingBoxMax.GetY() : lBoundingBoxMin.GetY()),
            static_cast<f32>(( liCorner & 4 ) ? lBoundingBoxMax.GetZ() : lBoundingBoxMin.GetZ()));
    }

    Vector3 lCentre = ( lBoundingBoxMax - lBoundingBoxMin ) * 0.5f + lBoundingBoxMin;

    const Matrix44 lWorldViewProjection = rw::math::vpu::Mult( lTransform, lCameraViewProjection );

    // The box centre must be inside the view volume.
    const Vector4 lCentreClip = TransformToClip( lWorldViewProjection, lCentre );
    const f32 lfCentreW = std::fabs( lCentreClip.GetW() );
    if ( std::fabs( lCentreClip.GetX() ) > lfCentreW ||
         std::fabs( lCentreClip.GetY() ) > lfCentreW ||
         std::fabs( lCentreClip.GetZ() ) > lfCentreW )
    {
        return;
    }

    s32 liCornersInView = 0;
    for ( s32 liCorner = 0; liCorner < 8; ++liCorner )
    {
        const Vector4 lClip = TransformToClip( lWorldViewProjection, laCorners[ liCorner ] );
        const f32 lfW = std::fabs( lClip.GetW() );
        if ( lfW >= std::fabs( lClip.GetX() ) && lfW >= std::fabs( lClip.GetY() ) && lfW >= std::fabs( lClip.GetZ() ) )
        {
            ++liCornersInView;
        }
    }

    if ( liCornersInView < KI_MIN_CORNERS_IN_VIEW )
    {
        return;
    }

    // Facing term: how directly the advert's At axis points at the camera, clamped to [0, 1].
    const Vector3 lWorldCentre = rw::math::vpu::TransformPoint( lTransform, lCentre );
    const Vector3 lToCamera = lCameraPosition - lWorldCentre;
    const Vector3 lAt = lTransform.At();
    const Vector3 lAtDirection = lAt * ( 1.0f / std::sqrt( rw::math::vpu::MagnitudeSquared( lAt ) ) );
    const Vector3 lToCameraDirection =
        lToCamera * ( 1.0f / std::sqrt( rw::math::vpu::MagnitudeSquared( lToCamera ) ) );
    f32 lfAngle = rw::math::vpu::Dot( lToCameraDirection, lAtDirection );
    lfAngle = ( lfAngle > 0.0f ) ? lfAngle : 0.0f;
    lfAngle = ( lfAngle < 1.0f ) ? lfAngle : 1.0f;
    const bool lbInView = lfAngle > 0.0f;

    // Projected size: the screen distance between the box's (min.x, min.y, max.z) corner and
    // its maximum corner on the reference screen.
    const Vector3 lScreenSpaceBBoxMin = ClipToScreen( TransformToClip( lWorldViewProjection, laCorners[ 4 ] ) );
    const Vector3 lScreenSpaceBBoxMax = ClipToScreen( TransformToClip( lWorldViewProjection, lBoundingBoxMax ) );
    CGS_ASSERT( rw::math::vpu::Magnitude( lScreenSpaceBBoxMax - lScreenSpaceBBoxMin ) >= 0.0f,
                "RwMathVPU::Magnitude( lScreenSpaceBBoxMax - lScreenSpaceBBoxMin ) >= 0.0f" );
    const s32 liScreenSize =
        static_cast<s32>( rw::math::vpu::Magnitude( lScreenSpaceBBoxMax - lScreenSpaceBBoxMin ) );

    if ( mbMassive3dDebug )
    {
        DEBUG_RenderAdvert( lTransform, lBoundingBoxMin, lBoundingBoxMax );
    }

    lpSubscriber->SetImpressionData( lbInView, lfAngle, static_cast<u32>( liScreenSize ),
                                     KU_IMPRESSION_SCREEN_WIDTH, KU_IMPRESSION_SCREEN_HEIGHT );
}

// =============================================================================
// DEBUG_RenderAdvert -- the advert's box (pushed a quarter unit along its At axis) and a
// unit circle at its origin.
// =============================================================================
void
WorldEntityModule::DEBUG_RenderAdvert( Matrix44Affine::InParam lTransform,
                                       Vector3::InParam lBoundingBoxMin, Vector3::InParam lBoundingBoxMax )
{
    CgsDev::DebugInterface lDebugInterface;
    CgsDev::DebugRender& lrRender = lDebugInterface.GetRender();

    Matrix44Affine lBoxTransform = lTransform;
    lBoxTransform.Pos() = lTransform.At() * 0.25f + lTransform.Pos();
    lrRender.DrawBox( lBoundingBoxMin, lBoundingBoxMax, lBoxTransform, KU_ADVERT_DEBUG_COLOUR );

    const Vector3 lNormal(1.0f, 0.0f, 0.0f);
    lrRender.DrawCircle( lTransform.Pos(), lNormal, 1.0f, KU_ADVERT_DEBUG_COLOUR );
}

// =============================================================================
// DEBUG_RenderAdvertLineTest -- a line from just in front of the camera to the advert,
// coloured by whether the advert is beyond the impression distance.
// =============================================================================
void
WorldEntityModule::DEBUG_RenderAdvertLineTest( f32 lfDistance, Vector3::InParam lAdvertPosition,
                                               Vector3::InParam lCameraPosition, Vector3::InParam lCameraDirection )
{
    CgsDev::DebugInterface lDebugInterface;
    CgsDev::DebugRender& lrRender = lDebugInterface.GetRender();

    const CgsDev::RGBA lColour = ( lfDistance > mfMassiveImpressionDebugDistance )
                               ? KU_ADVERT_OUT_OF_RANGE_COLOUR
                               : KU_ADVERT_DEBUG_COLOUR;
    lrRender.DrawLine( lCameraPosition + lCameraDirection, lAdvertPosition, lColour );
}

} // namespace BrnWorld
