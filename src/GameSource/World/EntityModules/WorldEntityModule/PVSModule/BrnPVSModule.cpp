#include "GameSource/World/EntityModules/WorldEntityModule/PVSModule/BrnPVSModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"   // GameDataIO::GameDataAssetEvent

#include <cmath>     // sqrtf -- the scalar stand-in for the X360 vrsqrtefp/vrefp pipelines
#include <cstring>   // memset

// ============================================================================
// Reconstructed from BURNOUT_X360_ARTIST.XEX -- BrnWorld::PVSModule.
//
//   PVSModule()              @ 0x827E4CC8
//   Construct()              @ 0x822C3F50
//   Prepare()                @ 0x82302E00
//   Update()                 @ 0x822EE050
//   Release()                @ 0x822A8AD8
//   GetInputInterface()      @ 0x822BAF78
//   GetOutputInterface()     @ 0x822BAFF0
//   GetGameDataRequestInt()  @ 0x822BB068
//
// The three interface getters each read the base ModuleSingleBuffered's
// GetInputStructure()/GetOutputStructure() into r31 and, on null, fire TWO streamed
// asserts (the inlined template tripwire "lpBuffer != NULL"
// CgsModuleSingleBufferedTemplate.h:73/82, then the PVS-level "lpInputBuffer" /
// "lpOutputBuffer" BrnPVSModule.h:122/130/138). CGS_ASSERT supplies __FILE__/__LINE__
// itself; the baked file/line are not reproduced.
// ============================================================================

namespace BrnWorld
{
namespace
{
    // Update's rodata constants (read out of the X360 image at the addresses the asm cites).
    const f32 KF_ZERO_WEIGHT          = 0.0f;   // flt_82001CC0 -- every weight's seed
    const f32 KF_PLAYER_ZONE_WEIGHT   = 100.0f; // flt_82CDB58C -- weight of the zone the listener is in
    const f32 KF_REQUIRED_ZONE_WEIGHT = 10.0f;  // flt_82CDB590 -- base weight of an IMMEDIATE neighbour
    const f32 KF_OPTIONAL_ZONE_WEIGHT = 9.8f;   // flt_82CDB594 -- base weight of a non-IMMEDIATE neighbour
    const f32 KF_HEADING_WEIGHT_SCALE = 1.0f;   // flt_82CDB598 -- the heading dot-product's contribution

    // The X360 loads the request's Vector4 position/velocity and runs it through the
    // vperm control word at unk_82CDA450 == 00 01 02 03 | 18 19 1A 1B | 00 01 02 03 |
    // 00 01 02 03 with BOTH source registers equal, i.e. a pure within-vector shuffle
    // that produces (x, z, x, x): the PVS query runs on the world's GROUND PLANE, so
    // CgsSceneManager's 2D zone polygons take world X in .x and world Z in .y.
    inline rw::math::vpu::Vector2 GroundPlane( const Vector4& lrVector )
    {
        rw::math::vpu::Vector2 lResult;
        lResult.x = lrVector.x;
        lResult.y = lrVector.z;
        lResult.z = lrVector.x;   // the mask duplicates lane 0 into the two unused lanes
        lResult.w = lrVector.x;
        return lResult;
    }
}

    // X360 0x827E4CC8. The asm: set the base vtable, construct the two base RWMutexes
    // (this+0x10, this+0x118; RWMutex(NULL, true)), set the PVSModule vtable, clear the
    // flag byte (stb 0 @ +0x1B58), then run BaseResourcePtr's empty-state initialiser
    // (@ +0x1D78: three zero words then three self-pointers then a zero word). The base
    // and member constructors handle the mutexes, the buffers and the resource pointer.
    PVSModule::PVSModule()
        : mePrepareStage( E_PREPARESTAGE_START )
        , meReleaseStage( E_RELEASESTAGE_START )
        , mZoneList()
        , mReceiverQueue()
        , mbCurrentZoneOnly( false )
    {
        // The stage members' real seeds are written by Construct @0x822C3F50 (below); the
        // ctor itself only clears the flag byte and default-builds the resource pointer.
    }

    // X360 0x822C3F50.
    //   ModuleSingleBuffered::Construct(this)
    //   *(this+0x1D74) = 2                    meReleaseStage  = E_RELEASESTAGE_DONE
    //   *(this+0x1D70) = 0                    mePrepareStage  = E_PREPARESTAGE_START
    //   *(recv+0x10) = 512; *(recv+0x14) = 16; *(recv+0x00) = recv+0x18; Clear(recv)
    //                                         == EventReceiverQueue<512,16>::Construct
    //   *(this+0x1FB0) = 0 (stb)              mbCurrentZoneOnly = false
    void PVSModule::Construct()
    {
        CgsModule::ModuleSingleBuffered::Construct();

        meReleaseStage = E_RELEASESTAGE_DONE;    // stw 2, 0x1D74
        mePrepareStage = E_PREPARESTAGE_START;   // stw 0, 0x1D70

        mReceiverQueue.Construct();              // miCapacity 512 / miAlignment 16 / buffer = +0x18, then Clear

        mbCurrentZoneOnly = false;               // stb 0, 0x1FB0
    }

    // X360 0x82302E00. The PVS data load stage machine.
    bool PVSModule::Prepare()
    {
        PVSIO::OutputBuffer* lpOutputBuffer = 0;

        // Past the module stage the output buffer is taken for write up-front and reset, so
        // that every early-out below leaves an empty response/request pair behind.
        if ( mePrepareStage > E_PREPARESTAGE_MODULE )
        {
            lpOutputBuffer = SafeLockOutputForWrite();
            lpOutputBuffer->mZoneResponseQueue.Clear();                      // stw 0, 8(buffer)
            lpOutputBuffer->mGameDataRequestInterface.mRequestQueue.Clear(); // VariableEventQueue<512,16>::Clear @ +0x1718
        }

        switch ( mePrepareStage )
        {
            case E_PREPARESTAGE_START:
            case E_PREPARESTAGE_MODULE:
            {
                mePrepareStage = E_PREPARESTAGE_MODULE;

                if ( !CgsModule::ModuleSingleBuffered::Prepare() )
                {
                    break;
                }
            }
            // fall through

            case E_PREPARESTAGE_REQUEST:
            {
                mePrepareStage = E_PREPARESTAGE_REQUEST;

                // The module stage did not take the lock; take it now (the X360 calls the
                // template's SafeLockOutputForWrite @0x822AA000 here) and reset the buffer.
                if ( lpOutputBuffer == 0 )
                {
                    lpOutputBuffer = SafeLockOutputForWrite();
                    lpOutputBuffer->mZoneResponseQueue.Clear();
                    lpOutputBuffer->mGameDataRequestInterface.mRequestQueue.Clear();
                }

                lpOutputBuffer->mGameDataRequestInterface.LoadPVS( &mReceiverQueue, 0, KI_PVS_POOL_ID );
                mReceiverQueue.Clear();
            }
            // fall through

            case E_PREPARESTAGE_WAITING:
            {
                mePrepareStage = E_PREPARESTAGE_WAITING;

                if ( mReceiverQueue.GetLength() < 1 )
                {
                    break;
                }

                {
                    const CgsModule::Event* lpEventData = 0;
                    s32 liEventSize = 0;
                    s32 liEventType = mReceiverQueue.GetFirstEvent( &lpEventData, &liEventSize );

                    while ( lpEventData != 0 )
                    {
                        if ( liEventType == KI_EVENT_GET_PVS )
                        {
                            const BrnResource::GameDataIO::GameDataAssetEvent* lpAssetEvent =
                                reinterpret_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>( lpEventData );

                            // ld r11,0x20(event) -> the acquired ZoneList's ResourceHandle.
                            const CgsResource::ResourceHandle lHandle = lpAssetEvent->mHandle;
                            mZoneList = lHandle;
                        }
                        else
                        {
                            CGS_ASSERT( false, "Invalid event received\n" );   // BrnPVSModule.cpp:155
                        }

                        liEventType = mReceiverQueue.GetNextEvent( lpEventData, &lpEventData, &liEventSize );
                    }
                }

                mReceiverQueue.Clear();
            }
            // fall through

            case E_PREPARESTAGE_DONE:
            {
                PVSIO::OutputBuffer* lpDoneBuffer = static_cast<PVSIO::OutputBuffer*>( LockOutputForWrite() );
                lpDoneBuffer->mZoneResponseQueue.Clear();
                lpDoneBuffer->mGameDataRequestInterface.mRequestQueue.Clear();
                lpDoneBuffer->muTotalZones = mZoneList->GetTotalZones();   // stw zoneList->+0x10, 0x1710(buffer)
                UnlockOutputForWrite();

                // The X360 leaves the machine ready to re-run from the module stage and arms
                // the release machine (stw 1,0x1D70 / stw 0,0x1D74).
                mePrepareStage = E_PREPARESTAGE_MODULE;
                meReleaseStage = E_RELEASESTAGE_START;

                UnlockOutputForWrite();   // the up-front lock taken at the top of the function
                return true;
            }

            default:
            {
                CGS_ASSERT( false, "Invalid Stage\n" );   // BrnPVSModule.cpp:200
                break;
            }
        }

        if ( lpOutputBuffer != 0 )
        {
            UnlockOutputForWrite();
        }
        return false;
    }

    // X360 0x822A8AD8.
    bool PVSModule::Release()
    {
        if ( meReleaseStage < E_RELEASESTAGE_DONE )
        {
            meReleaseStage = E_RELEASESTAGE_MODULE;

            if ( !CgsModule::ModuleSingleBuffered::Release() )
            {
                return false;
            }
        }
        else if ( meReleaseStage > E_RELEASESTAGE_DONE )
        {
            CGS_ASSERT( false, "Invalid Stage\n" );   // BrnPVSModule.cpp:257
            return false;
        }

        meReleaseStage = E_RELEASESTAGE_MODULE;
        mePrepareStage = E_PREPARESTAGE_START;
        return true;
    }

    // =========================================================================
    // X360 0x822EE050 -- the PVS query.
    //
    // For every GetZoneRequest queued this frame: resolve the request position to a Zone
    // through the loaded ZoneList (using the previous reply's zone index as the search
    // hint), then publish one GetZoneResponse describing the centre zone plus, unless
    // mbCurrentZoneOnly is set, all of its safe and unsafe neighbours. Each entry carries
    // the neighbour's 64-bit Zone id, whether it was reached over a SAFE edge, and the
    // edge's RENDER / IMMEDIATE flags. When the request carries a velocity hint every
    // non-centre zone additionally gets a heading weight: max(0, dot(unit(centre - pos),
    // unit(velocity))) added to a 10.0 / 9.8 base picked by the IMMEDIATE flag.
    //
    // PC deviations (all flagged in place): the X360 hand-vectorises the ground-plane
    // shuffle, the two Newton-Raphson normalisations and the dot product across VMX
    // registers; this is the scalar semantic-parity form over the committed
    // rw::math::vpu::Vector2 lanes BY NAME.
    // =========================================================================
    void PVSModule::Update()
    {
        PVSIO::OutputBuffer* lpOutputBuffer =
            static_cast<PVSIO::OutputBuffer*>( LockOutputForWrite() );
        PVSIO::InputBuffer* lpInputBuffer =
            static_cast<PVSIO::InputBuffer*>( LockInputForRead() );

        lpOutputBuffer->mZoneResponseQueue.Clear();                      // stw 0, 8(output)
        lpOutputBuffer->mGameDataRequestInterface.mRequestQueue.Clear(); // Clear @ output+0x1718

        // [FLAG PC boot gate] The console cannot reach Update before Prepare has bound the
        // zone list (Prepare only returns true once the type-58 reply lands), so the X360
        // dereferences the resource pointer unguarded. On this build a refused/failed PVS
        // bundle would turn that into an assert on the per-frame path, so report it once and
        // answer nothing. DELETE once the PVS load is proven to never fail.
        void* lpZoneListMemory = 0;
        mZoneList.GetResource( &lpZoneListMemory );
        if ( lpZoneListMemory == 0 )
        {
            static bool sbLoggedNoZoneList = false;
            if ( !sbLoggedNoZoneList && ( CgsDev::Message::gxMessageFilterFlags & 1 ) )
            {
                sbLoggedNoZoneList = true;
                *CgsDev::Log::gpDebugPrint
                    << "PVSModule::Update: no zone list bound -- the PVS resource never"
                       " arrived [FLAG PC boot gate]\n";
            }
            lpInputBuffer->mZoneRequestQueue.Clear();
            UnlockInputForRead();
            UnlockOutputForWrite();
            return;
        }

        // The X360 stack frame holds the response and the per-zone Zone* scratch array
        // (_DWORD v70[68]) ACROSS the whole request loop -- fields the not-found path does
        // not write keep whatever the previous request left behind. PC deviation: the
        // response is zero-initialised so a zone miss can never publish stale/garbage zone
        // ids into the world graphics streamer.
        PVSIO::GetZoneResponse lResponse;
        memset( &lResponse, 0, sizeof( lResponse ) );

        const CgsSceneManager::Zone* lapZones[ PVSIO::GetZoneResponse::KI_MAX_ZONES ];

        for ( s32 liRequest = 0; liRequest < lpInputBuffer->mZoneRequestQueue.GetLength(); ++liRequest )
        {
            const PVSIO::GetZoneRequest& lrRequest =
                lpInputBuffer->mZoneRequestQueue.GetEvent( liRequest );

            const rw::math::vpu::Vector2 lQueryPoint = GroundPlane( lrRequest.mPosition );

            const CgsSceneManager::Zone* lpCentreZone =
                mZoneList->GetFirstZoneForPoint( lQueryPoint, lrRequest.miLookupIndex );

            if ( lpCentreZone == 0 )
            {
                // Zone miss: echo the player index, report no zones and clear the search hint.
                lResponse.miPlayerIndex = lrRequest.miPlayerIndex;
                lResponse.miNumZones    = 0;
                lResponse.miLookupIndex = -1;

                lpOutputBuffer->mZoneResponseQueue.AddEvent( lResponse );
                continue;
            }

            lapZones[0] = lpCentreZone;

            lResponse.miPlayerIndex          = lrRequest.miPlayerIndex;   // lwz 0(request) -> stw 0(response)
            lResponse.maZoneIds[0]           = lpCentreZone->GetId();     // ld 0x10(zone)  -> std 8(response)
            lResponse.mabZoneSafe[0]         = 1;
            lResponse.mabZoneRender[0]       = 1;
            lResponse.mabZoneRequired[0]     = 1;
            lResponse.mafZoneWeights[0]      = KF_ZERO_WEIGHT;
            // (zone - zoneList->GetZones()) -- the X360 divides the byte difference by 48.
            lResponse.miLookupIndex =
                static_cast<s32>( lpCentreZone - mZoneList->GetZones() );

            if ( mbCurrentZoneOnly )
            {
                lResponse.miNumZones = 1;
            }
            else
            {
                lResponse.miNumZones = lpCentreZone->GetNumSafeNeighbours()
                                     + lpCentreZone->GetNumUnsafeNeighbours()
                                     + 1;

                s32 liZone = 1;

                for ( s16 liSafeIndex = 0;
                      liSafeIndex < lpCentreZone->GetNumSafeNeighbours();
                      ++liSafeIndex )
                {
                    const CgsSceneManager::Neighbour* lpNeighbour =
                        lpCentreZone->GetSafeNeighbour( liSafeIndex );

                    const CgsSceneManager::Zone* lpNeighbourZone = lpNeighbour->GetZone();
                    const u64 luZoneId   = lpNeighbourZone->GetId();
                    const u8  lbRequired = lpNeighbour->IsFlagSet(
                                               CgsSceneManager::Neighbour::E_NEIGHBOURFLAG_IMMEDIATE );
                    const u8  lbRender   = lpNeighbour->IsFlagSet(
                                               CgsSceneManager::Neighbour::E_NEIGHBOURFLAG_RENDER );

                    CGS_ASSERT( ( liZone >= 0 ) && ( liZone < PVSIO::GetZoneResponse::KI_MAX_ZONES ),
                                "(liIndex >= 0) && (liIndex < KI_MAX_ZONES)" );  // BrnPVSModuleEvents.h:230
                    if ( liZone >= PVSIO::GetZoneResponse::KI_MAX_ZONES )
                    {
                        break;   // PC safety: the X360 assert is non-gating and overruns the reply
                    }

                    lapZones[liZone]                 = lpNeighbourZone;
                    lResponse.maZoneIds[liZone]      = luZoneId;
                    lResponse.mabZoneSafe[liZone]    = 1;
                    lResponse.mabZoneRender[liZone]  = lbRender;
                    lResponse.mabZoneRequired[liZone] = lbRequired;
                    lResponse.mafZoneWeights[liZone] = KF_ZERO_WEIGHT;
                    ++liZone;
                }

                for ( s16 liUnsafeIndex = 0;
                      liUnsafeIndex < lpCentreZone->GetNumUnsafeNeighbours();
                      ++liUnsafeIndex )
                {
                    const CgsSceneManager::Neighbour* lpNeighbour =
                        lpCentreZone->GetUnsafeNeighbour( liUnsafeIndex );

                    const CgsSceneManager::Zone* lpNeighbourZone = lpNeighbour->GetZone();
                    const u64 luZoneId   = lpNeighbourZone->GetId();
                    const u8  lbRequired = lpNeighbour->IsFlagSet(
                                               CgsSceneManager::Neighbour::E_NEIGHBOURFLAG_IMMEDIATE );
                    const u8  lbRender   = lpNeighbour->IsFlagSet(
                                               CgsSceneManager::Neighbour::E_NEIGHBOURFLAG_RENDER );

                    CGS_ASSERT( ( liZone >= 0 ) && ( liZone < PVSIO::GetZoneResponse::KI_MAX_ZONES ),
                                "(liIndex >= 0) && (liIndex < KI_MAX_ZONES)" );  // BrnPVSModuleEvents.h:230
                    if ( liZone >= PVSIO::GetZoneResponse::KI_MAX_ZONES )
                    {
                        break;   // PC safety (see above)
                    }

                    lapZones[liZone]                  = lpNeighbourZone;
                    lResponse.maZoneIds[liZone]       = luZoneId;
                    lResponse.mabZoneSafe[liZone]     = 0;
                    lResponse.mabZoneRender[liZone]   = lbRender;
                    lResponse.mabZoneRequired[liZone] = lbRequired;
                    lResponse.mafZoneWeights[liZone]  = KF_ZERO_WEIGHT;
                    ++liZone;
                }

                if ( lResponse.miNumZones > liZone )
                {
                    lResponse.miNumZones = liZone;   // PC safety clamp (see the asserts above)
                }
            }

            // ---- the velocity-weighted zone ordering -------------------------------
            Vector4 lVelocity;
            bool    lbUseVelocity = false;
            lrRequest.GetVelocity( &lVelocity, &lbUseVelocity );

            const rw::math::vpu::Vector2 lVelocity2 = GroundPlane( lVelocity );

            if ( lbUseVelocity )
            {
                lResponse.mafZoneWeights[0] = KF_PLAYER_ZONE_WEIGHT;

                // unit(velocity) -- vrsqrtefp + two Newton refinements, then a vsel that
                // yields the zero vector when the length is not > 0.
                const f32 lfSpeedSq = ( lVelocity2.x * lVelocity2.x ) + ( lVelocity2.y * lVelocity2.y );
                const f32 lfSpeed   = ( lfSpeedSq == 0.0f ) ? 0.0f : sqrtf( lfSpeedSq );

                f32 lfHeadingX = 0.0f;
                f32 lfHeadingY = 0.0f;
                if ( lfSpeed > 0.0f )
                {
                    lfHeadingX = lVelocity2.x / lfSpeed;
                    lfHeadingY = lVelocity2.y / lfSpeed;
                }

                for ( s32 liZone = 1; liZone < lResponse.miNumZones; ++liZone )
                {
                    CGS_ASSERT( ( liZone >= 0 ) && ( liZone < PVSIO::GetZoneResponse::KI_MAX_ZONES ),
                                "(liIndex >= 0) && (liIndex < KI_MAX_ZONES)" );  // BrnPVSModuleEvents.h:306

                    const f32 lfBaseWeight = lResponse.mabZoneRequired[liZone]
                                                 ? KF_REQUIRED_ZONE_WEIGHT
                                                 : KF_OPTIONAL_ZONE_WEIGHT;

                    const rw::math::vpu::Vector2 lCentre = lapZones[liZone]->CalculateCentre();

                    const f32 lfToZoneX = lCentre.x - lQueryPoint.x;
                    const f32 lfToZoneY = lCentre.y - lQueryPoint.y;

                    const f32 lfDistanceSq = ( lfToZoneX * lfToZoneX ) + ( lfToZoneY * lfToZoneY );
                    const f32 lfDistance   = ( lfDistanceSq == 0.0f ) ? 0.0f : sqrtf( lfDistanceSq );

                    f32 lfDot = 0.0f;
                    if ( lfDistance > 0.0f )
                    {
                        lfDot = ( ( lfToZoneX / lfDistance ) * lfHeadingX )
                              + ( ( lfToZoneY / lfDistance ) * lfHeadingY );
                    }
                    if ( lfDot < 0.0f )
                    {
                        lfDot = 0.0f;   // vmaxfp against the zero vector
                    }

                    CGS_ASSERT( ( liZone >= 0 ) && ( liZone < PVSIO::GetZoneResponse::KI_MAX_ZONES ),
                                "(liIndex >= 0) && (liIndex < KI_MAX_ZONES)" );  // BrnPVSModuleEvents.h:241

                    lResponse.mafZoneWeights[liZone] =
                        ( lfDot * KF_HEADING_WEIGHT_SCALE ) + lfBaseWeight;
                }
            }

            lpOutputBuffer->mZoneResponseQueue.AddEvent( lResponse );
        }

        // Republish the world's total zone count and drain the request queue.
        lpOutputBuffer->muTotalZones = mZoneList->GetTotalZones();   // stw zoneList->+0x10, 0x1710(output)
        lpInputBuffer->mZoneRequestQueue.Clear();                    // stw 0, 8(input)

        UnlockInputForRead();
        UnlockOutputForWrite();
    }

    // X360 0x822BAF78. r31 = GetInputStructure(); if null, fire the two tripwires; return r31.
    PVSIO::InputBuffer* PVSModule::GetInputInterface()
    {
        // SafeGetInputStructure() inline-expands the first tripwire CGS_ASSERT(lpBuffer,
        // "lpBuffer != NULL") (template line 73); then the PVS-level assert (.h:122).
        PVSIO::InputBuffer* lpInputBuffer = SafeGetInputStructure();
        CGS_ASSERT(lpInputBuffer != nullptr, "lpInputBuffer");
        return lpInputBuffer;
    }

    // X360 0x822BAFF0. r31 = GetOutputStructure(); if null, fire the two tripwires; return r31.
    PVSIO::OutputBuffer* PVSModule::GetOutputInterface()
    {
        // SafeGetOutputStructure() inline-expands the first "lpBuffer != NULL" tripwire
        // (template line 82); then the PVS-level assert (.h:130).
        PVSIO::OutputBuffer* lpOutputBuffer = SafeGetOutputStructure();
        CGS_ASSERT(lpOutputBuffer != nullptr, "lpOutputBuffer");
        return lpOutputBuffer;
    }

    // X360 0x822BB068. r31 = GetOutputStructure(); if null, fire the two tripwires; return
    // r31 + 0x1718 (the embedded game-data request interface, &mGameDataRequestInterface).
    BrnResource::GameDataIO::RequestInterface<512>* PVSModule::GetGameDataRequestInt()
    {
        // SafeGetOutputStructure() inline-expands the first "lpBuffer != NULL" tripwire
        // (template line 82); then the PVS-level assert (.h:138).
        PVSIO::OutputBuffer* lpOutputBuffer = SafeGetOutputStructure();
        CGS_ASSERT(lpOutputBuffer != nullptr, "lpOutputBuffer");
        return &lpOutputBuffer->mGameDataRequestInterface;   // addi r3, OutputStructure, 0x1718
    }
}

// ============================================================================
// Explicit instantiation of the module's SafeLockOutputForWrite() @ 0x822AA000.
//   The X360 build emitted this template method out-of-line (IDA symbol truncated
//   to "In"). asm: r31 = LockOutputForWrite(); if null, fire the single
//   "lpBuffer != NULL" tripwire (CgsModuleSingleBufferedTemplate.h:64 baked path);
//   return r31 reinterpret_cast<PVSIO::OutputBuffer*>. Called by
//   BrnWorld::PVSModule::Prepare. The generic body is inline in
//   CgsModuleSingleBufferedTemplate.h (SafeLockOutputForWrite == LockOutputForWrite +
//   one CGS_ASSERT(lpBuffer,"lpBuffer != NULL") + return); this TU forces the
//   per-instantiation emission for <PVSIO::InputBuffer, PVSIO::OutputBuffer>.
// ============================================================================
template BrnWorld::PVSIO::OutputBuffer*
CgsModule::ModuleSingleBufferedTemplate<BrnWorld::PVSIO::InputBuffer,
                                        BrnWorld::PVSIO::OutputBuffer>::SafeLockOutputForWrite();
