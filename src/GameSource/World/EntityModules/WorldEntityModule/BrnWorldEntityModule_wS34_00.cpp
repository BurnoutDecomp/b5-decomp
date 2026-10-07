// BrnWorld::WorldEntityModule -- the Massive (in-game advertising) slice: prepare (bundles,
// advert textures, lookup table), per-frame subscriber creation, and the per-instance impression pass.

#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h"

#include <cmath>   // std::fabs, std::sqrt
#include <cstdio>  // snprintf ([FLAG PC witness] BRN_MASSIVE_DIAG)
#include <cstdlib> // getenv ([FLAG PC witness] BRN_MASSIVE_DIAG)

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [FLAG PC witness] BRN_MASSIVE_DIAG
#include "GameShared/GameClasses/Graphics/CgsModel.h"
#include "GameShared/GameClasses/Graphics/Dispatch/Renderable.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"     // AcquireResourceResponse
#include "pc/gcm/renderengine/texture.h"                                     // renderengine::Texture
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
        Vector4 lClip;
        lClip.x = lrPoint.x * lrMatrix.xAxis.x + lrPoint.y * lrMatrix.yAxis.x + lrPoint.z * lrMatrix.zAxis.x + lrMatrix.wAxis.x;
        lClip.y = lrPoint.x * lrMatrix.xAxis.y + lrPoint.y * lrMatrix.yAxis.y + lrPoint.z * lrMatrix.zAxis.y + lrMatrix.wAxis.y;
        lClip.z = lrPoint.x * lrMatrix.xAxis.z + lrPoint.y * lrMatrix.yAxis.z + lrPoint.z * lrMatrix.zAxis.z + lrMatrix.wAxis.z;
        lClip.w = lrPoint.x * lrMatrix.xAxis.w + lrPoint.y * lrMatrix.yAxis.w + lrPoint.z * lrMatrix.zAxis.w + lrMatrix.wAxis.w;
        return lClip;
    }

    // Clip space -> the reference screen: ((clip / w) + 1) * 0.5 * (1280, 720).
    Vector3 ClipToScreen(const Vector4& lrClip)
    {
        const f32 lfInvW = 1.0f / lrClip.w;
        Vector3 lScreen;
        lScreen.x = (lrClip.x * lfInvW + 1.0f) * 0.5f * static_cast<f32>(KU_IMPRESSION_SCREEN_WIDTH);
        lScreen.y = (lrClip.y * lfInvW + 1.0f) * 0.5f * static_cast<f32>(KU_IMPRESSION_SCREEN_HEIGHT);
        lScreen.z = 0.0f;
        lScreen.w = 0.0f;
        return lScreen;
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

    if ( MassiveAdClient3::CMassiveClientCore::Instance() != nullptr &&
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
            if ( lpPrevious != nullptr && !( lpPrevious->miState == 3 || lpPrevious->miState == 2 ) )
            {
                lbCreateNext = false;
            }
        }

        if ( lbCreateNext && miNumMassiveSubscribersCreated < KI_NUM_MASSIVE_TEXTURES )
        {
            renderengine::Texture* lpTexture = mapDynamicAdvertTextures[ miNumMassiveSubscribersCreated ];

            // FLAG PC-platform: the console reads the level count and the pixel base address
            // out of the texture's GPU fetch constant; the PC raster keeps the level count
            // explicitly and its pixel storage is the D3D texture.
            const u32 luNumLevels = lpTexture->muNumMipLevels;
            const u32 luPixelDataSize = renderengine::Texture::GetPixelDataSize(
                renderengine::Texture::GetType( lpTexture ),
                renderengine::Texture::GetWidth( lpTexture ),
                renderengine::Texture::GetHeight( lpTexture ),
                renderengine::Texture::GetDepth( lpTexture ),
                renderengine::Texture::GetFormat( lpTexture ),
                luNumLevels );

            mMassive.CreateSubscriber( miNumMassiveSubscribersCreated, lpTexture->mpD3DTexture, luPixelDataSize );
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
    if ( lpItem == nullptr )
    {
        return;
    }

    BrnMassive::BrnMassiveSubscriber* lpSubscriber = lpItem->mpSubscriber;
    if ( lpSubscriber != nullptr )
    {
        lpSubscriber->SetImpressionData( false, 0.0f, 0, KU_IMPRESSION_SCREEN_WIDTH, KU_IMPRESSION_SCREEN_HEIGHT );
    }

    const f32 lfDistance = rw::math::vpu::Magnitude( lCameraPosition - lTransform.Pos() );

    // [FLAG PC witness] BRN_MASSIVE_DIAG=1: the first lookups of the impression pass.
    {
        static const bool sbDiag = ( std::getenv( "BRN_MASSIVE_DIAG" ) != 0 );
        static s32 siDiagCount = 0;
        if ( sbDiag && siDiagCount < 8 )
        {
            ++siDiagCount;
            char lacMsg[192];
            std::snprintf( lacMsg, sizeof( lacMsg ),
                "[FLAG PC witness] [massive] renderable=%u item IE=%d subscriber=%p distance=%.1f limit=%.1f safe=%d\n",
                static_cast<u32>( luRenderableIndex ), lpItem->miIEIndex, static_cast<void*>( lpSubscriber ),
                static_cast<double>( lfDistance ), static_cast<double>( mfMassiveImpressionDebugDistance ),
                mMassive.mbSafeToDownload ? 1 : 0 );
            CgsDev::Log::WriteToLog( lacMsg );
        }
    }

    if ( mbMassiveDistanceLinesDebug )
    {
        DEBUG_RenderAdvertLineTest( lfDistance, lTransform.Pos(), lCameraPosition, lCameraDirection );
    }

    if ( lfDistance > mfMassiveImpressionDebugDistance )
    {
        return;
    }

    if ( lpSubscriber == nullptr )
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
        laCorners[ liCorner ].x = ( liCorner & 1 ) ? lBoundingBoxMax.x : lBoundingBoxMin.x;
        laCorners[ liCorner ].y = ( liCorner & 2 ) ? lBoundingBoxMax.y : lBoundingBoxMin.y;
        laCorners[ liCorner ].z = ( liCorner & 4 ) ? lBoundingBoxMax.z : lBoundingBoxMin.z;
        laCorners[ liCorner ].w = 0.0f;
    }

    Vector3 lCentre = ( lBoundingBoxMax - lBoundingBoxMin ) * 0.5f + lBoundingBoxMin;

    const Matrix44 lWorldViewProjection = rw::math::vpu::Mult( lTransform, lCameraViewProjection );

    // The box centre must be inside the view volume.
    const Vector4 lCentreClip = TransformToClip( lWorldViewProjection, lCentre );
    const f32 lfCentreW = std::fabs( lCentreClip.w );
    if ( std::fabs( lCentreClip.x ) > lfCentreW ||
         std::fabs( lCentreClip.y ) > lfCentreW ||
         std::fabs( lCentreClip.z ) > lfCentreW )
    {
        return;
    }

    s32 liCornersInView = 0;
    for ( s32 liCorner = 0; liCorner < 8; ++liCorner )
    {
        const Vector4 lClip = TransformToClip( lWorldViewProjection, laCorners[ liCorner ] );
        const f32 lfW = std::fabs( lClip.w );
        if ( lfW >= std::fabs( lClip.x ) && lfW >= std::fabs( lClip.y ) && lfW >= std::fabs( lClip.z ) )
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

    const Vector3 lNormal = { 1.0f, 0.0f, 0.0f, 0.0f };
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
