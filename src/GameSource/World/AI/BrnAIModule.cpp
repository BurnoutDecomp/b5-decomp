#include "BrnAIModule.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"         // AcquireResourceResponse
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"            // AIModuleIO::OutputBuffer
#include "SharedClasses/AI/AISectionsResourceType.h"                            // BrnAI::AISectionsData

// =================================================================================================
// GameSource/World/AI/BrnAIModule.cpp  --  BrnAI::AIModule's own lifecycle spine.
//
//   AIModule::AIModule   @0x827E3780
//   AIModule::Construct  @0x82794D08  (398 insns)
//   AIModule::Prepare    @0x82798070  (224 insns)
//   AIModule::LoadMapData@0x82795340  (167 insns)
//   BrnAI::operator++(AIModule::EPrepareStage&, int) @0x82765A10
//
// =================================================================================================
// ⭐⭐⭐ WHY THIS FILE IS WHAT A CRASHED CAR HAS BEEN WAITING FOR
// =================================================================================================
// The crash exit lands `mbToBeResetOnTrack` on the RaceCar, and "CHOOSE WHERE TO PUT THE CAR"
// comes from BrnAI::ResetOnTrackManager reading the AI road network. That manager is an EMBEDDED
// MEMBER of this class, its ONLY constructor call site in the whole image is Prepare stage 3
// below, and the AI road network itself (AI.dat, "WorldMapData") is loaded by LoadMapData --
// stage 2 below.
//
// ⚠️ What is still parked is flagged individually at its site (see the Construct banner): the
// AIDebugComponent, the RouteMapDebugComponent, the six perf monitors and the ContactSpyInterface.
// =================================================================================================

namespace BrnAI
{

// X360 0x82765A10 -- BrnAI::operator++(AIModule::EPrepareStage&, int)
// DWARF BrnAIModule.h:417. Post-increment over the AI prepare-stage enum: caches
// the current stage, advances it by one, asserts the result has not walked past
// E_PREPARESTAGE_DONE (5), and returns the pre-increment value. Called by
// AIModule::Prepare to step through the multi-frame prepare state machine.
AIModule::EPrepareStage operator++(AIModule::EPrepareStage& leEnumIndex, int)
{
    AIModule::EPrepareStage leOldValue = leEnumIndex;
    leEnumIndex = static_cast<AIModule::EPrepareStage>(static_cast<int>(leEnumIndex) + 1);
    CGS_ASSERT(leEnumIndex <= AIModule::E_PREPARESTAGE_DONE,
        CGS_ASSERT_MESSAGE_AT(
            "leEnumIndex <= AIModule::E_PREPARESTAGE_DONE",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\world\\ai\\BrnAIModule.h", 418 /* ARTIST 0x82765A3C */));
    return leOldValue;
}

void AIModule::AssertNativeLayout()
{
    CGS_STATIC_ASSERT((offsetof(AIModule, maAICars) == 0x230), "maAICars native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, maAIDrivers) == 0x2EE50), "maAIDrivers native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mRaceBalancingManager) == 0x3D9D0), "mRaceBalancingManager native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mRouteRequestManager) == 0x42268), "mRouteRequestManager native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mAIDebugComponent) == 0x424AC), "mAIDebugComponent native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mResetOnTrackManager) == 0x45DB0), "mResetOnTrackManager native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mMasterRoute) == 0x46B60), "mMasterRoute native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mePrepareStage) == 0x47F6C), "mePrepareStage native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, meReleaseStage) == 0x47F70), "meReleaseStage native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, meLoadMapDataStage) == 0x47F74), "meLoadMapDataStage native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mRandom) == 0x47F80), "mRandom native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mResourceReceiverQueue) == 0x47FB0), "mResourceReceiverQueue native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mMapDataHandle) == 0x483C8), "mMapDataHandle native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mLuaDataHandle) == 0x483D0), "mLuaDataHandle native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mRouteMapModule) == 0x483D8), "mRouteMapModule native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mePlayerActiveRaceCarIndex) == 0x4E9F8), "mePlayerActiveRaceCarIndex native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mCamera) == 0x4EA00), "mCamera native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, miLineUpdateTokenCounter) == 0x4EB60), "miLineUpdateTokenCounter native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mfProgressionRankAsRatio) == 0x4EB64), "mfProgressionRankAsRatio native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mContactSpyInterface) == 0x4EB68), "mContactSpyInterface native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, muNumAggressiveCars) == 0x4EB78), "muNumAggressiveCars native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, maWayPointDistance) == 0x4EB84), "maWayPointDistance native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, meCurrentRoundRobin) == 0x4EBC8), "meCurrentRoundRobin native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, meAggressorIndex) == 0x4EBD0), "meAggressorIndex native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, meVictimIndex) == 0x4EBD4), "meVictimIndex native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, meProximityGlobalRaceCarIndexRoundRobin) == 0x4EBD8), "meProximityGlobalRaceCarIndexRoundRobin native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mBuzzBy) == 0x4EBDC), "mBuzzBy native offset");
    CGS_STATIC_ASSERT((offsetof(AIModule, mpClosestCar) == 0x4ED10), "mpClosestCar native offset");
    CGS_STATIC_ASSERT((sizeof(AIModule) == 0x4ED20), "AIModule native size");
}


s32 AIModule::miUpdatePM = 0;
s32 AIModule::miUpdateCarsPM = 0;
s32 AIModule::miUpdateDriversPM = 0;
s32 AIModule::miUpdateDriverPM = 0;
s32 AIModule::miUpdateDriversRoundRobinPM = 0;
s32 AIModule::miResetOnTrackPM = 0;

AIModule::AIModule()
{
}

// =================================================================================================
// Construct @0x82794D08   (398 insns)
//
//   0x82794D14  bl  CgsModule::ModuleSingleBuffered::Construct(this)
//   ...         stw 0,  0x47F6C(this)   mePrepareStage     = E_PREPARESTAGE_START
//   ...         stw 0,  0x47F74(this)   meLoadMapDataStage = 0
//   ...         stw 3,  0x47F70(this)   meReleaseStage     = E_RELEASESTAGE_DONE
//   ...         (the +270908..+270948 AIDebugComponent seed block + its Register)
//   ...         bl  BrnAI::RouteRequestManager::Construct(this + 270952)
//   ...         stw 1024, 0x47FC0 / stw 16, 0x47FC4 / stw this+0x47FC8, 0x47FB0
//                                      == EventReceiverQueue<1024,16>::Construct
//   ...         bl  CgsModule::BaseEventReceiverQueue::Clear(this + 294832)
//   ...         bl  CgsModule::ModuleSingleBuffered::Construct(this + 295896)   (mRouteMapModule)
//   ...         bl  BrnAI::RouteMapDebugComponent::Construct(this+321948, this+295896, this)
//   ...         (the this+294784 CgsNumeric::Random prime, unrolled)
//   ...         six CgsDev::PerfMonCpu::AddMonitor("AI Module, ...") + their >= 0 asserts
//   ...         bl  BrnAI::AIDebugComponent::Construct(this + 271532, this)
//   ...         35x AICar::Construct  /  8x AIDriver::Construct
//   0x82795...  stw 1,  4(this)        Module::mbIsNewModule = true
//   ...         the +322400..+322435 flag block, +322040/+322044, ContactSpyInterface::Construct
//
// ⭐ `mbIsNewModule = true` IS LOAD-BEARING AND IT IS THE LAST THING THE CONSOLE DOES. Every
// data-structure arm of CgsModule::ModuleSingleBuffered::Prepare is guarded by `if
// (!mbIsNewModule)`, and those arms Create/Prepare data structures this module does not have.
// The crash-exit wave measured exactly this: its module's Prepare gate blamed "the call resolves
// to the BASE ModuleSingleBuffered::Prepare" when the real defect was the derived Construct
// never running to set this flag. Without this store, stage 1 below would take the allocation
// path and fail.
//
void AIModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();

    mePrepareStage     = E_PREPARESTAGE_START;
    meLoadMapDataStage = E_LOADMAPDATA_REQUEST_BUNDLE;
    meReleaseStage     = E_RELEASESTAGE_DONE;

    mRaceBalancingManager.Construct(this);
    mRouteRequestManager.Construct();
    mResourceReceiverQueue.Construct();

    // ARTIST inlines RouteMapModule::Construct(this) here: base init, ids,
    // embedded Route Map debug owner, then the module's new-module flag.
    mRouteMapModule.Construct(this);

    mRandom.Construct();
    // ARTIST 0x82794FEC..0x82795198: six registrations, then five checks.
    miUpdatePM = CgsDev::PerfMonCpu::AddMonitor("AI Module, Update", CgsDev::E_PMP_7, false, 5.0f, true);
    miUpdateCarsPM = CgsDev::PerfMonCpu::AddMonitor("AI Module, Update Cars", CgsDev::E_PMP_7, false, 5.0f, true);
    miUpdateDriversPM = CgsDev::PerfMonCpu::AddMonitor("AI Module, Update Drivers", CgsDev::E_PMP_7, false, 5.0f, true);
    miUpdateDriverPM = CgsDev::PerfMonCpu::AddMonitor("Driver, Update", CgsDev::E_PMP_7, false, 3.0f, true);
    miUpdateDriversRoundRobinPM = CgsDev::PerfMonCpu::AddMonitor("AI Module, Round Robin ", CgsDev::E_PMP_7, false, 5.0f, true);
    miResetOnTrackPM = CgsDev::PerfMonCpu::AddMonitor("AI Module, Reset On Track", CgsDev::E_PMP_7, false, 2.0f, true);
    CGS_ASSERT(miUpdatePM >= 0, "miUpdatePM >= 0");
    CGS_ASSERT(miUpdateCarsPM >= 0, "miUpdateCarsPM >= 0");
    CGS_ASSERT(miUpdateDriversPM >= 0, "miUpdateDriversPM >= 0");
    CGS_ASSERT(miUpdateDriverPM >= 0, "miUpdateDriverPM >= 0");
    CGS_ASSERT(miResetOnTrackPM >= 0, "miResetOnTrackPM >= 0");
    mAIDebugComponent.Construct(this);

    // THE 35 AI CARS. AICar::Construct @0x82792620 NOW RUNS (2026-09-04, R1 lane): the
    // function was parked here as an "ARTIST export HOLE", which it is -- it has no JSON in
    // the export set and is named only through THIS function's xrefs_from -- but the body is
    // recoverable from image.bin and has been, store for store, into BrnAICar_Update.cpp.
    //
    // The console's loop, 0x827951BC..0x827951F8:
    //     mr r4,r29 ; mr r3,r31 ; bl AIModule::GetAICar ; mr r4,r29 ; bl AICar::Construct
    //     addi r29,r29,1 ; cmpwi r29,0x23 ; <the BurnoutConstants.h:84 operator++ range
    //     assert "leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT"> ; blt
    // i.e. `for (EGlobalRaceCarIndex i = 0; i < COUNT; i++) GetAICar(i)->Construct(i);`, and
    // it runs BEFORE the 8 AIDriver::Constructs below (0x82795208..0x82795240), which is the
    // order kept here.
    //
    // WHAT THE PARK COST, and why the two hand-rolled stores it left behind are gone: the
    // console's Construct is the ONLY writer of miRaceCarIndex @+0x14C4 (DWARF :676
    // meGlobalRaceCarIndex) -- `stw r28,0x14C4(r31)` with r28 == the loop index. Parked, all
    // 35 cars read slot 0, so every route request went out stamped owner 0 (RouteRequest
    // Manager keys on GetRaceCarIndex()), every RouteResponse came back and was applied to
    // car 0 by `GetAICar(lpRouteResponse->GetEventId())`, the `[rival] slot S global G`
    // witness printed `global 0` for every slot, and HandleManagementEvents' assert
    // `lpDriver->GetGlobalRaceCarIndex() == leGlobalRaceCarIndex` failed for every slot but 0.
    // The two stand-ins that stood here (meCarState = INACTIVE and miRaceCarIndex = 0) are
    // both subsumed: Construct writes `stw r7(2),0x14C8` (INACTIVE) and the real index.
    for( EGlobalRaceCarIndex leCar = E_GLOBAL_RACE_CAR_INDEX_0;
         leCar < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leCar++ )
    {
        maAICars[leCar].Construct(leCar);
    }


    // The rest of the console Construct: the +322400..+322435 flag block, the +294784 Random prime,
    // RouteRequestManager::Construct(+270952) and the 8x AIDriver::Construct.
    miLineUpdateTokenCounter = 0;
    muNumAggressiveCars      = 3;
    mbDoInRangeCatchup       = true;
    mbDoOutOfRangeCatchup    = true;
    mbDoAggressiveDriving    = true;
    mbEnableDrivingInput     = true;
    mbIsInOnlineGameMode     = false;
    mbIsInGameMode           = false;
    mbFullRollingStart       = false;
    mbHighTakenDownPenalty   = false;
    mbAIDrivesPlayer         = false;
    mbAIPlayerInvulnerable   = true;
    for (s32 liDriver = 0; liDriver < KI_MAX_ACTIVE_RACE_CARS; ++liDriver)
    {
        maAIDrivers[liDriver].Construct();
    }

    mbIsNewModule = true;
    mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    mePlayerGlobalRaceCarIndex = E_GLOBAL_RACE_CAR_INDEX_0;
    meSpeedSelectionMethod = static_cast<EAISpeedSelectionMethod>(0);
    meDefaultPlayerRouteFindingStyle = static_cast<ERouteFindingStyle>(0);
    meDefaultAIRouteFindingStyle = static_cast<ERouteFindingStyle>(0);
    mContactSpyInterface.Construct();
    mMasterRoute.meStatus = Route::E_STATUS_UNINITIALISED;
    mMasterRoute.miNodeCount = 0;
    mMasterRoute.miDefaultStartNode = 0;
}

// =================================================================================================
// Destruct @0x8276E380 (vtable 0x820D0D98 slot 3). Its only dispatch is WorldModule::Destruct
// @0x827BD108..0x827BD11C (vtbl+0xC on mAIModule) <- BrnGameModule::Destruct @0x823BC868 <- the tail
// of main (0x827E6640); the PC's BrnMain.cpp GameRelease leaves that chain gated, so nothing on this
// build reaches it yet.
//
//   0x8276E398              ModuleSingleBuffered::Destruct()
//   0x8276E39C..0x8276E3A4  mResourceReceiverQueue.Clear()                         (this + 0x47FB0)
//   0x8276E3A8..0x8276E3BC  vtbl+0xC on mRouteMapModule (this + 0x483D8) == RouteMapModule::Destruct
//                           @0x827750D0; the tree has no override, so ModuleSingleBuffered::Destruct
//                           -- the non-debug half of 0x827750D0
//   0x8276E3C0..0x8276E400  the AIDebugComponent (this + 0x424AC) Destruct, inlined: assert
//                           `mpAIModule != NULL` (BrnAIDebugComponent.cpp:265), mpAIModule = NULL,
//                           then the ICF-folded empty base Destruct
//   0x8276E404..0x8276E40C  mContactSpyInterface.Construct()                      (this + 0x4EB68)
//
// It is reached only at process teardown, never per frame.
// =================================================================================================
void AIModule::Destruct()
{
    CgsModule::ModuleSingleBuffered::Destruct();
    mResourceReceiverQueue.Clear();
    mRouteMapModule.Destruct();
    mAIDebugComponent.Destruct();
    mContactSpyInterface.Construct();
}

// =================================================================================================
// Prepare @0x82798070   (224 insns)   -- a 6-stage machine over mePrepareStage (+294764),
// re-entered once per frame from WorldModule::Prepare stage eWorldPrepareAI until it returns true.
// The console's switch falls THROUGH from each case into the next, so one frame can complete
// several stages; the jump table at 0x8279810C is the resume-point dispatch.
//
//   case 0 START      ++stage; mResourceReceiverQueue.Clear(); DebugComponent::Register(+271532)
//   case 1 MANAGER    if (!ModuleSingleBuffered::Prepare(this)) return false; ++stage
//   case 2 RESOURCES  if (!LoadMapData(this, lpOutputBuffer)) return false;   ++stage
//   case 3 ROUTEMAP   if ((*(vtbl(this+295896) + 64))(this+295896, *(u64*)(this+295880)))
//                     { +322500 = 0;
//                       ResetOnTrackManager::Construct(this+286128,
//                                                      ResourcePtr<AISectionsData>(this+295880),
//                                                      this+560);
//                       ++stage; ... } else return false
//   case 4 AICARS     AIDriver::Prepare(GetAIDriver(i), GetAISectionsData(), i, this+294784) x8
//                     + the +322400..+322832 owner/flag block + the flt_8300D5A0.. rodata seeds
//   case 5 DONE       meReleaseStage = E_RELEASESTAGE_START; return true
//
// ⭐ vtable slot 64/4 == 16 is one past ModuleSingleBuffered's last virtual, i.e.
// RouteMapModule's OWN first -- which is RouteMapModule::Prepare @0x8277FDE8. Written by name.
// ⭐ `ld r4, 0(this+295880)` is ONE 64-bit load: the console passes the whole 8-byte
// ResourceHandle by value in a single GPR.
//
// Stage 4 is whole since 2026-09-22 (crash parity G05-D1): AIDriver::Prepare x8, the cursor /
// closest-car / take-down-penalty stores, and the inlined BuzzBy::Prepare -- including the
// Construct of BuzzBy's file-static Random (the flt_8300D5A0.. words, which are that Random's ring,
// not independent globals).
// =================================================================================================
bool AIModule::Prepare( BrnResource::GameDataIO::AllocatorList* lpAllocatorList,
                        AIModuleIO::OutputBuffer* lpOutputBuffer )
{
    CGS_ASSERT( lpAllocatorList != 0, "lpAllocatorList != NULL" );
    CGS_ASSERT( lpOutputBuffer != 0, "lpOutputBuffer != NULL" );

    switch ( mePrepareStage )
    {
        case E_PREPARESTAGE_START:
        {
            mePrepareStage++;
            mResourceReceiverQueue.Clear();
            mAIDebugComponent.Register();
        }
        // fall through

        case E_PREPARESTAGE_MANAGER:
        {
            if ( !CgsModule::ModuleSingleBuffered::Prepare() )
            {
                return false;
            }
            mePrepareStage++;
        }
        // fall through

        case E_PREPARESTAGE_RESOURCES:
        {
            if ( !LoadMapData( lpOutputBuffer ) )
            {
                return false;
            }
            mePrepareStage++;
        }
        // fall through

        case E_PREPARESTAGE_ROUTEMAP:
        {
            if ( !mRouteMapModule.Prepare( mMapDataHandle ) )
            {
                return false;
            }

            mePrepareStage++;
        }
        // fall through

        case E_PREPARESTAGE_AICARS:
        {
            // ⭐ 2026-09-03 (aiwave): stage 4 un-parked -- AIDriver::Prepare over the 8 active race cars
            // (BrnAIDriver.cpp is mounted), the round-robin / proximity cursors, the closest-car pair and
            // BuzzBy::Prepare (lane A1's member map of the +322400..+322832 block).
            meAggressorIndex = E_ACTIVE_RACE_CAR_INDEX_0;
            meVictimIndex = E_ACTIVE_RACE_CAR_INDEX_0;
            for (s32 liDriver = 0; liDriver < KI_MAX_ACTIVE_RACE_CARS; ++liDriver)
            {
                maAIDrivers[liDriver].Prepare(GetAISectionsData(), liDriver, &mRandom);
            }
            // The stage-4 tail, 0x8279827C..0x8279838C (every store r30 == 0 unless noted):
            miLineUpdateTokenCounter = 0;                                   // 0x8279829C stwx +0x4EB60
            meCurrentRoundRobin[0] = E_ACTIVE_RACE_CAR_INDEX_0;             // 0x827982BC (this+0x4EBC8)
            meCurrentRoundRobin[1] = E_ACTIVE_RACE_CAR_INDEX_0;             // 0x827982C4
            meProximityGlobalRaceCarIndexRoundRobin = E_GLOBAL_RACE_CAR_INDEX_0;   // 0x827982E0 +0x4EBD8
            mBuzzBy.Prepare(maAICars, &mResetOnTrackManager);               // 0x827982EC..0x82798380 (inlined)
            mfClosestDistance = 3.4028234663852886e+38f;                              // 0x82798384 flt_8204F664 (FLT_MAX)
            mpClosestCar      = 0;                                          // 0x82798388
            mbHighTakenDownPenalty = false;                                 // 0x8279838C stbx +0x4EB81
            mePrepareStage++;
        }
        // fall through

        case E_PREPARESTAGE_DONE:
        {
            meReleaseStage = E_RELEASESTAGE_START;
            return true;
        }

        default:
        {
            CGS_ASSERT( false, "0" );
            return false;
        }
    }
}

// =================================================================================================
// LoadMapData @0x82795340   (167 insns)   -- Prepare stage 2. A 5-state sub-machine over
// meLoadMapDataStage (+294772) that loads the AI bundle and then acquires the road network out of
// it. THIS IS THE FUNCTION THAT MAKES THE AI ROAD NETWORK EXIST.
//
//   case 0  LockForWrite(out); iface = out->GetAIResourceRequestInterface();
//           assert(iface); iface->LoadBundle(&mResourceReceiverQueue, 1, 5, "AI.dat", false);
//           UnlockForWrite(out); stage = 1
//   case 1  if (mResourceReceiverQueue count < 1) return false;
//           mResourceReceiverQueue.Clear(); stage = 2
//   case 2  LockForWrite(out); iface = out->GetAIResourceRequestInterface(); assert(iface);
//           record { mpUser = &mResourceReceiverQueue, miEventId = 1, miPoolId = 5,
//                    mResourceId = ID::HashString("WorldMapData") };
//           VariableEventQueue<4096,16>::AddEvent(iface, &record, 4, 24);
//           UnlockForWrite(out); stage = 3
//   case 3  if (count < 1) return false;
//           walk the receiver queue; for each event with miEventId == 1 copy the reply's
//           {mpResourceMemory, mpSourceEntry} pair into mMapDataHandle; return true
//   case 4  return true
//
// ⭐ THE `| 0x500000000` IS THE KNOWN HEX-RAYS FUSION ARTIFACT, NOT PART OF THE ID. The
// decompiler fuses the interleaved `li r10, 5` (which is the SEPARATE miPoolId store at +0x08)
// into the `std` of the hash. The tree already documents this family in
// BrnGameDataRequestQueueImpl.h ("TAGGED-ID CORRECTION"): CgsResource::ID::HashString ends
// `clrldi r3,r11,32` and Pool::FindResource compares the whole 64-bit value, so an id with the
// pool packed into the top bits matches NOTHING and the request parks on the receiver queue for
// ever. The case-2 record is therefore spelled through the committed
// RequestInterface<N>::AcquireResource builder, which builds exactly that record (mpUser,
// miEventId, miPoolId and the untagged 64-bit resource id) and posts it with type 4.
//
// ⭐ THE REPLY IS READ BY MEMBER, NEVER AT THE CONSOLE'S LITERAL +0x18/+0x1C. The X360 does
// `this[73970] = v9[6]; this[73971] = v9[7]`, i.e. the pair at payload +24/+28 -- which IS
// CgsResource::Events::AcquireResourceResponse::{mpResourceMemory, mpSourceEntry}
// (PoolModule::DoAcquireResourceRequest @0x828FCD48 builds it). The host handle is 16 bytes where
// the console's is 8 and every literal past it shifts, so the read goes through the member names.
// Same idiom as StuntManager's district-map acquire and StreetManager::LoadDistrictMap.
//
// ⚠️ FAITHFUL, NOT "FIXED": case 3 never advances the stage to 4. It does not need to -- Prepare
// only calls this while ITS stage is RESOURCES, and the moment this returns true Prepare steps
// past it and never calls again. Left exactly as the console has it.
// =================================================================================================
bool AIModule::LoadMapData( AIModuleIO::OutputBuffer* lpOutputBuffer )
{
    CGS_ASSERT( lpOutputBuffer != 0, "lpOutputBuffer != NULL" );

    switch ( meLoadMapDataStage )
    {
        case E_LOADMAPDATA_REQUEST_BUNDLE:
        {
            lpOutputBuffer->LockForWrite();

            BrnResource::GameDataIO::RequestInterface<4096>* lpResourceRequestInterface =
                lpOutputBuffer->GetAIResourceRequestInterface();
            CGS_ASSERT( lpResourceRequestInterface != 0, "lpResourceRequestInterface != NULL" );

            lpResourceRequestInterface->LoadBundle( &mResourceReceiverQueue, 1, 5, "AI.dat", false );

            lpOutputBuffer->UnlockForWrite();

            if ( CgsDev::Message::gxMessageFilterFlags & 1 )
            {
                *CgsDev::Log::gpDebugPrint << "[ai] LoadMapData: requested bundle AI.dat (pool 5)\n";
            }

            meLoadMapDataStage = E_LOADMAPDATA_AWAIT_BUNDLE;
        }
        // fall through

        case E_LOADMAPDATA_AWAIT_BUNDLE:
        {
            if ( mResourceReceiverQueue.GetCount() < 1 )
            {
                return false;
            }
            mResourceReceiverQueue.Clear();
            meLoadMapDataStage = E_LOADMAPDATA_REQUEST_MAPDATA;
        }
        // fall through

        case E_LOADMAPDATA_REQUEST_MAPDATA:
        {
            lpOutputBuffer->LockForWrite();

            BrnResource::GameDataIO::RequestInterface<4096>* lpResourceRequestInterface =
                lpOutputBuffer->GetAIResourceRequestInterface();
            CGS_ASSERT( lpResourceRequestInterface != 0, "lpResourceRequestInterface != NULL" );

            lpResourceRequestInterface->AcquireResource( &mResourceReceiverQueue, 1, 5,
                                                         "WorldMapData" );

            lpOutputBuffer->UnlockForWrite();

            if ( CgsDev::Message::gxMessageFilterFlags & 1 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai] LoadMapData: AI.dat bundle landed; acquiring WorldMapData\n";
            }

            meLoadMapDataStage = E_LOADMAPDATA_AWAIT_MAPDATA;
        }
        // fall through

        case E_LOADMAPDATA_AWAIT_MAPDATA:
        {
            if ( mResourceReceiverQueue.GetCount() < 1 )
            {
                return false;
            }

            const CgsModule::Event* lpEvent = 0;
            s32                     liSize  = 0;
            mResourceReceiverQueue.GetFirstEvent( &lpEvent, &liSize );

            while ( lpEvent != 0 )
            {
                // reinterpret_cast, not static_cast: CgsResource::Events::Event and
                // CgsModule::Event are unrelated roots and the receiver queue hands out the
                // module one.
                const CgsResource::Events::AcquireResourceResponse* lpResponse =
                    reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>( lpEvent );

                if ( lpResponse->miEventId == 1 )
                {
                    mMapDataHandle.mpResourceMemory = lpResponse->mpResourceMemory;
                    mMapDataHandle.mpSourceEntry    = lpResponse->mpSourceEntry;
                }
                else
                {
                    CGS_ASSERT( false,
                        "AIModule::LoadMapData, have received a resource with an ID that wasn't requested" );
                }

                const CgsModule::Event* lpNextEvent = 0;
                mResourceReceiverQueue.GetNextEvent( lpEvent, &lpNextEvent, &liSize );
                lpEvent = lpNextEvent;
            }

            if ( CgsDev::Message::gxMessageFilterFlags & 1 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ai] LoadMapData: WorldMapData handle "
                    << ( ( mMapDataHandle.mpResourceMemory != 0 ) ? "RESOLVED" : "NULL" )
                    << "\n";
            }

            return true;
        }

        case E_LOADMAPDATA_DONE:
        {
            return true;
        }

        default:
        {
            CGS_ASSERT( false, "AIModule::LoadMapData in a weird state" );
            return true;
        }
    }
}

// The loaded AI road network. The X360's GetAISectionsData @0x8277BC00 builds a temporary
// ResourcePtr<AISectionsData> from mMapDataHandle, reads its memory resource, asserts it non-null
// ("lpAISectionsData != NULL", BrnAIModule.cpp:2485) and unlinks the temporary. This is that read
// without the temporary's intrusive-list churn -- the route map module already holds a bound
// ResourcePtr onto the same resource from its own Prepare.
const AISectionsData* AIModule::GetLoadedAISectionsData() const
{
    if ( !mRouteMapModule.HasAISectionsData() )
    {
        return 0;
    }
    return mRouteMapModule.GetAISectionsData();
}

}

// FOLDED FROM BrnAIModule_Events.cpp
// =================================================================================================
// BrnAIModule_Events.cpp -- the ACTIVATION half of BrnAI::AIModule's per-frame spine
// (aiwave lane A7, 2026-09-03). Partfile of BrnAIModule.cpp.
//
//   BrnAI::AIModule::HandleManagementEvents      @0x82798620  (586 insns; whole)
//   BrnAI::AIModule::HandleGameActions           @0x82791FD0  (280 insns; whole)
//   BrnAI::AIModule::OnRollingStart              @0x8276E5C8  (37 insns;  whole)
//   BrnAI::AIModule::OnModeStartRacing           @0x8276E4B0  (69 insns;  whole)
//   BrnAI::AIModule::OnModeFinished              @0x8277B970  (67 insns;  whole)
//   BrnAI::AIModule::OnRaceCarReachedFinish      @0x8277B8D0  (40 insns;  whole)
//   BrnAI::AIModule::OnModeStart                 @0x82791DB8  (133 insns; whole since 2026-09-23)
//   BrnAI::AIModule::OnModeEnd                   @0x8277BA80  (96 insns;  whole since 2026-09-22)
//  BrnAI::AIModule::OnPlayerTakedown  (34 insns;  whole)
//   BrnAI::AIModule::OnRaceCarReachedCheckpoint  @0x8278A658  (50 insns;  whole -- export hole, ppcdis; CC-10)
//  BrnAI::AIModule::SetupRaceBalancingManager  (125 insns; whole)
//
// =================================================================================================
// ⭐⭐⭐ WHY THIS FILE EXISTS: NOTHING ELSE ON THIS BUILD ACTIVATES AN AI DRIVER.
//
// Lane A1 landed every drive leg of AIModule::Update (BrnAIModule_Drive.cpp) and found all of them
// silent no-ops: UpdateDrivers skips a slot whose AIDriver::mbIsActive is clear, StoreDrivenCarData
// skips a driver with no AICar, ProcessAIVehicleInputs emits no record for either. `mbIsActive` has
// exactly ONE writer that turns it ON in the whole image -- AIDriver::SetAICar @0x827963C8 -- and
// exactly ONE caller of that: HandleManagementEvents' E_EVENT_ACTIVATE_RACE_CAR arm, right here
// (asm 0x827989C0). Two writers turn it OFF, both also here (the DEACTIVATE arm @0x82798B68).
//
// THE ACTIVATION SEQUENCE, end to end (all four hops are in the tree today):
//
//  1. PRODUCER -- RaceCarEntityModule, writing its OWN OutputBuffer_PreScene's RaceCarAIInterface:
//       RCEM::SpawnRaceCar @0x822FE5D8    -> mManagementQueue.AddEvent<AttachAIControlEvent>(ev, 0)
//                                            (BrnRaceCarEntityModule.cpp:2216..:2222)
//       RCEM::AttachAIControl             -> RaceCarAIInterface::ActivateRaceCar(global, active)
//                                            == AddEvent<ActivateRaceCarEvent>(ev, 1)
//       RCEM::RemoveRaceCar @~0x82304564  -> ::DeactivateRaceCar(global, isInAMode)  (type 2)
//                                            (BrnRaceCarEntityModule.cpp:1981)
//       RCEM::DetachAIControl             -> ::DetachAIControl(global)               (type 3)
//                                            (BrnRaceCarEntityModule.cpp:2084)
//       RCEM::SetUpAIForMode @0x82301620  -> ::SetUpOutOfRangeRaceCar(...)           (type 5)
//                                          + AddEvent<AddCarToCurrentModeEvent>(ev, 6)
//                                            (BrnRaceCarEntityModule_Rivals.cpp:731/:742..:751 and
//                                             BrnRaceCarEntityModule_ModeArming.cpp:731/:742..:751)
//       (types 4 PlayerControlChangedEvent and 7 RemoveCarFromCurrentModeEvent have their typed
//        AddEvent instantiations committed -- VariableEventQueue_16384_16.cpp:41/:42 -- but no
//        reconstructed producer yet; see `## parks` in the lane report.)
//
//  2. BRIDGE -- WorldModule::BridgeRaceCarModuleToAIModule_PreScene @0x827A4FA0
//       (WorldBridgeEntityModulesToAI.cpp:15, called from BrnWorldModule.cpp:2759 -- WIRED, its
//        boot gate was retired 2026-08-26):
//            lpAIInputBuffer->SetRaceCarAIInterface(
//                lpRaceCarOutputBuffer_PreScene->GetRaceCarAIInterface());
//       and InputBuffer::SetRaceCarAIInterface (BrnAIModuleIO_InputBuffer_Accessors.cpp:218) is
//       `mRaceCarAIInterface = *lpInterface;` -- the console's XMemCpy of all 0x43D0 bytes, which
//       INCLUDES the 16 KB management queue at RaceCarAIInterface+0x2F8. The queue therefore
//       arrives at the AI module by value, as a copy, every frame.
//
//  3. CONSUMER -- this file. AIModule::Update row #16 (asm 0x8279B714) calls
//       HandleManagementEvents(lpInputBuffer), which reads
//       `lpInputBuffer->GetRaceCarAIInterface()->mManagementQueue` -- the SAME named member the
//       producer's AddEvent writes and the bridge copies. Both ends match by name; the console
//       reaches it as `RaceCarAIInterface + 760` (0x2F8), which BrnRaceCarAIInterfaces.h pins.
//
//  4. EFFECT -- E_EVENT_ACTIVATE_RACE_CAR (id 1) calls AIDriver::SetAICar, which sets
//       driver->mbIsActive = 1 and car->meCarState = E_AI_CAR_STATE_IN_RANGE. From the next frame
//       on, UpdateDrivers/StoreDrivenCarData/ProcessAIVehicleInputs all see that slot.
//
// WHICH EVENT IDS ACTIVATE A DRIVER (the answer the lane report repeats):
//    id 0  E_EVENT_ATTACH_AI_CONTROL   -- car INACTIVE -> OUT_OF_RANGE. No driver yet.
//    id 1  E_EVENT_ACTIVATE_RACE_CAR   -- ⭐ THE ONE. driver->SetAICar(car): mbIsActive = 1,
//                                          driver->mpCar = car, car->meCarState = IN_RANGE.
//    id 2  E_EVENT_DEACTIVATE_RACE_CAR -- the inverse: mbIsActive = 0, driver->mpCar = NULL,
//                                          car back to OUT_OF_RANGE.
//    id 3  E_EVENT_DETACH_AI_CONTROL   -- car OUT_OF_RANGE -> INACTIVE.
//
// =================================================================================================
// THE TWO QUEUES THIS FILE DRAINS
//
//   HandleManagementEvents: VariableEventQueue<16384,16>, console RaceCarAIInterface+0x2F8, reached
//     as InputBuffer::GetRaceCarAIInterface()->mManagementQueue. Event ids are
//     BrnAI::AIModuleIO::EEvent (BrnRaceCarAIInterfaces.h, DWARF :284) 0..7, and the console's
//     switch is a DENSE 8-entry jump table (asm 0x827987CC `cmplwi r29, 7` -> jpt_827987EC), so
//     every id maps 1:1 onto that enum with no shift.
//
//   HandleGameActions: VariableEventQueue<13312,16>, console InputBuffer+0x103BC, reached as
//     InputBuffer::GetGameActionQueue() (X360 0x8276D530). Its jump table is keyed on
//     `type - 7` (asm 0x82792084 `addi r11, r3, -7`; `cmplwi 0x7C` -> 125 cases), so IDA's
//     "jumptable case N" is action type N + 7. The sixteen live arms are types
//     7, 14, 23, 30, 34, 35, 39, 50, 99, 106, 113, 114, 120, 122, 123, 131 -- see the enum grow in
//     BrnGameActions.h for the six of those this tree did not carry yet.
//
// =================================================================================================
// CONSOLE-vs-HOST NOTES (every deviation is flagged at its site as well)
//
//  * X360 offsets in the comments are IDENTITY, never host addressing. Everything is by member.
//  * The console's asserts are reproduced non-gating (CGS_ASSERT) exactly where they fire. The
//    PC tree's `[GUARD]` null early-outs are not on x360: ARTIST has no such paths.
//  * `flt_82001CC0` (the f31 both functions preload) is 0.0f -- image.bin file offset 0x1CC0 reads
//    00000000. Spelled 0.0f here.
//  * The console's `stfs f31, <int field>` stores into meBehaviour-adjacent INT fields are 4-byte
//    zero stores; they are written as the typed zero of the real member (e.g. `miNodeCount = 0`),
//    which is the same bit pattern.
//  * TWO DIFFERENT CASTS, for two different record families. The management-event records derive
//    from CgsModule::Event (BrnRaceCarAIInterfaces.h), so the queue payload is downcast with
//    static_cast. The GAME-ACTION records derive from the empty tag `GameAction<T>`
//    (BrnGameActions.h:743) instead, which is NOT an Event, so the payload is reinterpret_cast --
//    the same idiom, for the same reason, as the other HandleGameActions in this tree
//    (BrnRaceCarEntityModule.cpp:2532).
// =================================================================================================

#include "GameSource/World/AI/BrnAIModule.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIBuzzBy.h"
#include "GameSource/World/AI/BrnAIAggressiveness.h"                   // Aggressiveness::SetAggression
#include "GameSource/World/AI/BrnAISharedConstants.h"                  // EAIBehaviour / EAICarState / ERouteFindingStyle
#include "GameSource/World/AI/Route/BrnRoute.h"                        // BrnAI::Route (the in-place route invalidations)
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"             // RouteMapModuleIO::InputBuffer / RaceRouteRequest
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO.h"                // AIModuleIO::InputBuffer (+ its GameActionQueue)
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"   // AIModuleIO::OutputBuffer (carried, never read)
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"       // RaceCarAIInterface + the eight EEvent records
#include "GameSource/World/BrnWorldSharedConstants.h"                  // BrnWorld::E_CAR_CONTROL_AI_MODULE
#include "GameSource/GameState/BrnGameActions.h"                       // EGameActionType + the action records
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"  // BrnGameState::GameModeParams
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cmath>

namespace BrnAI
{

namespace
{
    // The console's "is this a real number" test is a per-lane `vcmpeqfp v,v` self-compare -- a
    // NaN test, NOT a finiteness test (an infinity passes it). Spelled exactly that way.
    inline bool IsNotNaN(f32 lfValue) { return !(lfValue != lfValue); }



    // The console's streamed assert form (StrStream over CgsDev::Assert::gpcMessageBuffer). Used
    // by the two arms whose message embeds a number: HandleManagementEvents' "AI car in unknown
    // state <n>" (BrnAIModule.cpp:1862) and its default arm's "Unknown RaceCar->AI management
    // event: <n>" (:1983).
    void FireStreamedAssert(const char* lpcPrefix, s32 liValue, const char* lpcSuffix)
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << lpcPrefix << liValue << lpcSuffix;
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    // The console inlines AICar::SetBehaviour's body (WITHOUT its range assert -- there is no
    // `bl AICar::SetBehaviour` at any of these sites) in nine places across this file:
    //   mfBehaviourTimer = 0; mePreviousBehaviour = meBehaviour; meBehaviour = new.
    // Note the ORDER the asm uses: the old behaviour is loaded BEFORE the new one is stored, so a
    // self-assignment still lands the old value in mePreviousBehaviour.
    inline void SetBehaviourInline(AICar* lpCar, EAIBehaviour leBehaviour)
    {
        const EAIBehaviour lePrevious = lpCar->meBehaviour;
        lpCar->mfBehaviourTimer   = 0.0f;
        lpCar->meBehaviour        = leBehaviour;
        lpCar->mePreviousBehaviour = lePrevious;
    }

    // The console's inlined "this route is stale" pair: Route::meStatus = E_STATUS_UNINITIALISED
    // and Route::miNodeCount = 0, both reached through the AICar pointer (mRoute is at AICar+0).
    inline void InvalidateRouteInline(AICar* lpCar)
    {
        Route* const lpRoute = lpCar->GetRoute();
        lpRoute->meStatus    = Route::E_STATUS_UNINITIALISED;   // car+0x1408
        lpRoute->miNodeCount = 0;                               // car+0x1400
    }
}

// =================================================================================================
// HandleManagementEvents @0x82798620   (DWARF BrnAIModule.cpp:1774)
//
// int __fastcall (r3 = this, r4 = lpInputBuffer). Three non-gating null asserts, then the whole
// body is one GetFirstEvent/GetNextEvent walk over the RaceCarAIInterface's management queue with
// a dense 8-way switch on the event id.
//
// Register map: r28 = this, r27 = the event pointer, r29 = the event id, r24 = the baked source
// path, r26 = 0, r23 = 1, r21 = 0x4EBE2 (== &mBuzzBy.mbResetBuzzTimers), r30 = 0x4EB74
// (== &meDefaultAIRouteFindingStyle), r18..r14 = the hoisted assert strings.
// =================================================================================================
void AIModule::HandleManagementEvents(const AIModuleIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpInputBuffer != 0, CGS_ASSERT_MESSAGE_AT("lpInputBuffer != NULL", __FILE__, 1784));                              // :1784

    const AIModuleIO::RaceCarAIInterface* const lpCarInterface = lpInputBuffer->GetRaceCarAIInterface();
    CGS_ASSERT(lpCarInterface != 0, CGS_ASSERT_MESSAGE_AT("lpCarInterface != NULL", __FILE__, 1789));                            // :1789

    // asm 0x82798694 `addi r31, r31, 0x2F8` -- the management queue is the interface's own member.
    const CgsModule::VariableEventQueue<16384, 16>* const lpManagementQueue =
        &lpCarInterface->mManagementQueue;
    CGS_ASSERT(lpManagementQueue != 0, CGS_ASSERT_MESSAGE_AT("lpManagementQueue != NULL", __FILE__, 1793));                      // :1793

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liType = lpManagementQueue->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != 0)
    {
        switch (liType)
        {
        // ---- 0  E_EVENT_ATTACH_AI_CONTROL  (asm 0x82798810) --------------------------------
        // The car joins the AI module's roster: reset it to the event's personality, put it in
        // OUT_OF_RANGE, remember its car-asset key, and ask BuzzBy to restart its timers.
        case AIModuleIO::E_EVENT_ATTACH_AI_CONTROL:
        {
            const AIModuleIO::AttachAIControlEvent* const lpAttach =
                static_cast<const AIModuleIO::AttachAIControlEvent*>(lpEvent);
            const EGlobalRaceCarIndex leGlobalRaceCarIndex = lpAttach->meGlobalRaceCarIndex;

            CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
                       CGS_ASSERT_MESSAGE_AT("leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0", __FILE__, 1807));              // :1807
            CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       CGS_ASSERT_MESSAGE_AT("leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT", __FILE__, 1808));           // :1808

            AICar* const lpCar = GetAICar(leGlobalRaceCarIndex);

            // asm 0x82798864..0x82798884: the assert fires when meCarState is IN_RANGE(0) or
            // OUT_OF_RANGE(1) -- i.e. exactly AICar::IsActive().
            CGS_ASSERT(!lpCar->IsActive(), "!lpCar->IsActive()");                         // :1812

            lpCar->Reset(static_cast<EPersonalityType>(lpAttach->mePersonalityType),
                         lpAttach->mbKeepResetSection);                                   // 0x827988B0
            lpCar->meCarState        = E_AI_CAR_STATE_OUT_OF_RANGE;                       // 0x827988B4
            lpCar->mCarAssetAttribKey = lpAttach->mCarAssetAttribKey;                     // 0x827988B8 ld/std
            mBuzzBy.RequestResetBuzzTimers();                                             // 0x827988C0


            break;
        }

        // ---- 1  E_EVENT_ACTIVATE_RACE_CAR  (asm 0x827988C8) --------------------------------
        // ⭐ THE ACTIVATION. Binds an out-of-range car to a free driver slot; AIDriver::SetAICar
        // raises mbIsActive and drops the car into IN_RANGE.
        case AIModuleIO::E_EVENT_ACTIVATE_RACE_CAR:
        {
            const AIModuleIO::ActivateRaceCarEvent* const lpActivate =
                static_cast<const AIModuleIO::ActivateRaceCarEvent*>(lpEvent);
            const EGlobalRaceCarIndex leGlobalRaceCarIndex = lpActivate->meGlobalRaceCarIndex;
            const EActiveRaceCarIndex leDriverActiveRaceCarIndex = lpActivate->meActiveRaceCarIndex;

            CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
                       CGS_ASSERT_MESSAGE_AT("leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0", __FILE__, 1831));              // :1831
            CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       CGS_ASSERT_MESSAGE_AT("leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT", __FILE__, 1832));           // :1832
            CGS_ASSERT(leDriverActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                       "leDriverActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");        // :1834
            CGS_ASSERT(leDriverActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leDriverActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");     // :1835

            AICar* const lpCar = GetAICar(leGlobalRaceCarIndex);
            CGS_ASSERT(lpCar->meCarState == E_AI_CAR_STATE_OUT_OF_RANGE,
                       "lpCar->GetState() == E_AI_CAR_STATE_OUT_OF_RANGE");               // :1838

            AIDriver* const lpDriver = GetAIDriver(leDriverActiveRaceCarIndex);
            CGS_ASSERT(!lpDriver->IsActive(), "!lpDriver->IsActive()");                   // :1841

            lpDriver->SetAICar(lpCar);                                                    // 0x827989C0


            break;
        }

        // ---- 2  E_EVENT_DEACTIVATE_RACE_CAR  (asm 0x827989C8) ------------------------------
        // The inverse of 1. A car that is ALREADY out of range only logs a warning; an IN_RANGE
        // car is unbound from its driver and put back to OUT_OF_RANGE.
        case AIModuleIO::E_EVENT_DEACTIVATE_RACE_CAR:
        {
            const AIModuleIO::DeactivateRaceCarEvent* const lpDeactivate =
                static_cast<const AIModuleIO::DeactivateRaceCarEvent*>(lpEvent);
            const EGlobalRaceCarIndex leGlobalRaceCarIndex = lpDeactivate->meGlobalRaceCarIndex;
            // (mbIsInAMode is on the wire but this consumer never reads it -- asm 0x827989C8..
            //  0x82798B6C touches only word 0.)

            CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
                       CGS_ASSERT_MESSAGE_AT("leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0", __FILE__, 1852));              // :1852
            CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       CGS_ASSERT_MESSAGE_AT("leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT", __FILE__, 1853));           // :1853

            AICar* const lpCar = GetAICar(leGlobalRaceCarIndex);

            if (lpCar->meCarState == E_AI_CAR_STATE_OUT_OF_RANGE)                         // 0x82798A20
            {
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "WARNING: AI car " << static_cast<s32>(leGlobalRaceCarIndex)
                        << " deactivated when already out of range.\n";                   // 0x82798A48
                }
                break;
            }

            if (lpCar->meCarState != E_AI_CAR_STATE_IN_RANGE)                             // 0x82798A60
            {
                FireStreamedAssert("AI car in unknown state ",
                                   static_cast<s32>(lpCar->meCarState), "\n");            // :1862
            }

            // asm 0x82798AB0..0x82798B28: lpDriver is the car's driver ONLY while IN_RANGE;
            // otherwise the console carries a literal NULL into the assert below.
            AIDriver* const lpDriver =
                (lpCar->meCarState == E_AI_CAR_STATE_IN_RANGE) ? lpCar->GetDriver() : 0;
            CGS_ASSERT(lpDriver != 0, "lpDriver");                                        // :1865

            CGS_ASSERT(lpDriver->IsActive(), "lpDriver->IsActive()");                     // :1866
            // AIDriver::GetGlobalRaceCarIndex() is inlined on the console as
            // `(mpCar && mbIsActive) ? mpCar->miRaceCarIndex : -1` (asm 0x82798B50..0x82798B2C).
            {
                const s32 liDriverGlobalRaceCarIndex =
                    (lpDriver->GetCar() != 0 && lpDriver->IsActive())
                        ? lpDriver->GetCar()->miRaceCarIndex
                        : -1;
                CGS_ASSERT(liDriverGlobalRaceCarIndex == static_cast<s32>(leGlobalRaceCarIndex),
                           "lpDriver->GetGlobalRaceCarIndex() == leGlobalRaceCarIndex");  // :1867
            }

            if (lpDriver->GetCar() != 0)                                                  // 0x82798B50
            {
                lpDriver->GetCar()->meCarState = E_AI_CAR_STATE_OUT_OF_RANGE;             // 0x82798B5C
                lpDriver->SetCar(0);                                                      // 0x82798B60
            }
            lpDriver->mbIsRacingLineInitialised = 0;                                      // 0x82798B64
            lpDriver->mbIsActive                = 0;                                      // 0x82798B68


            break;
        }

        // ---- 3  E_EVENT_DETACH_AI_CONTROL  (asm 0x82798BD0) --------------------------------
        case AIModuleIO::E_EVENT_DETACH_AI_CONTROL:
        {
            const AIModuleIO::DetachAIControlEvent* const lpDetach =
                static_cast<const AIModuleIO::DetachAIControlEvent*>(lpEvent);
            const EGlobalRaceCarIndex leGlobalRaceCarIndex = lpDetach->meGlobalRaceCarIndex;

            CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
                       "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");              // :1886
            CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");           // :1887

            AICar* const lpCar = GetAICar(leGlobalRaceCarIndex);
            CGS_ASSERT(lpCar->meCarState == E_AI_CAR_STATE_OUT_OF_RANGE,
                       "lpCar->GetState() == E_AI_CAR_STATE_OUT_OF_RANGE");               // :1890

            lpCar->meCarState = E_AI_CAR_STATE_INACTIVE;                                  // 0x82798C4C
            mBuzzBy.RequestResetBuzzTimers();                                             // 0x82798C50


            break;
        }

        // ---- 4  E_EVENT_PLAYER_TAKEN_OVER  (asm 0x82798C58) --------------------------------
        // "the player took the wheel back / handed it over". Only meaningful while the PLAYER's
        // own driver slot is active; the console calls GetAIDriver TWICE (once for the gate, once
        // for the store) and that is reproduced.
        case AIModuleIO::E_EVENT_PLAYER_TAKEN_OVER:
        {
            const AIModuleIO::PlayerControlChangedEvent* const lpControlChanged =
                static_cast<const AIModuleIO::PlayerControlChangedEvent*>(lpEvent);

            AIDriver* const lpPlayerDriverGate = GetAIDriver(mePlayerActiveRaceCarIndex);
            if (lpPlayerDriverGate == 0 || !lpPlayerDriverGate->IsActive())                // 0x82798C6C
            {
                break;
            }

            AIDriver* const lpPlayerDriver = GetAIDriver(mePlayerActiveRaceCarIndex);      // 0x82798C80
            if (lpPlayerDriver != 0)
            {
                lpPlayerDriver->mbIsRacingLineInitialised = 0;                             // 0x82798C94
            }

            AICar* const lpPlayerCar = GetAICar(mePlayerGlobalRaceCarIndex);

            const EAICarState leState = lpPlayerCar->meCarState;                           // 0x82798CA4
            lpPlayerCar->mbIsDrivenByPlayer = lpControlChanged->mbPlayerIsInControl;        // 0x82798CAC

            if (leState != E_AI_CAR_STATE_IN_RANGE)                                        // 0x82798CB0
            {
                break;
            }

            AIDriver* const lpCarDriver = lpPlayerCar->GetDriver();                         // 0x82798CB4
            lpCarDriver->ResetPIDTuningState();                                             // 0x82798CB8


            break;
        }

        // ---- 5  E_EVENT_SET_UP_OUT_OF_RANGE_RACE_CAR  (asm 0x82798CC0) ---------------------
        // The per-frame position/heading refresh for a car the AI is simulating out of range.
        case AIModuleIO::E_EVENT_SET_UP_OUT_OF_RANGE_RACE_CAR:
        {
            const AIModuleIO::SetUpOutOfRangeRaceCarEvent* const lpSetUp =
                static_cast<const AIModuleIO::SetUpOutOfRangeRaceCarEvent*>(lpEvent);
            const EGlobalRaceCarIndex leGlobalRaceCarIndex = lpSetUp->meGlobalRaceCarIndex;

            CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0 &&
                       leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       "lpEvent->meGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0 && "
                       "lpEvent->meGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");   // :1917

            AICar* const lpCar = GetAICar(leGlobalRaceCarIndex);
            CGS_ASSERT(lpCar->meCarState == E_AI_CAR_STATE_OUT_OF_RANGE,
                       "lpCar->GetState() == E_AI_CAR_STATE_OUT_OF_RANGE");                // :1921

            // asm 0x82798D28..0x82798D84: three `vspltw` + `vcmpeqfp. v,v` self-compares over the
            // x/y/z lanes of the event's POSITION (r31 == event + 0x10).
            CGS_ASSERT(IsNotNaN(static_cast<float>(lpSetUp->mPosition.GetX())) &&
                       IsNotNaN(static_cast<float>(lpSetUp->mPosition.GetY())) &&
                       IsNotNaN(static_cast<float>(lpSetUp->mPosition.GetZ())),
                       "Invalid car position being passed");                                // :1925

            // asm 0x82798DA8..0x82798DC0: v1 = event+0x10 (position), v2 = event+0x20 (at),
            // r4 = lhz event+0x30 (section), r5 = lbz event+0x38 (the record's medal count --
            // AICar::UpdateOutOfRangeData's fourth parameter is spelled luSectionSpeed there).
            lpCar->UpdateOutOfRangeData(lpSetUp->mPosition,
                                        lpSetUp->mAt,
                                        lpSetUp->muSection,
                                        lpSetUp->muNumberOfMedalsToUnlock);
            mBuzzBy.RequestResetBuzzTimers();                                               // 0x82798DC4


            break;
        }

        // ---- 6  E_EVENT_ADD_CAR_TO_MODE  (asm 0x82798DCC) ----------------------------------
        // The rival joins the running game mode. This is the arm RaceCarEntityModule::
        // SetUpAIForMode @0x82301620 feeds (BrnRaceCarEntityModule_Rivals.cpp).
        case AIModuleIO::E_EVENT_ADD_CAR_TO_MODE:
        {
            const AIModuleIO::AddCarToCurrentModeEvent* const lpAddCar =
                static_cast<const AIModuleIO::AddCarToCurrentModeEvent*>(lpEvent);

            AICar* const lpCar = GetAICar(lpAddCar->meGlobalRaceCarIndex);

            // asm 0x82798DD8..0x82798DF8: deviating from the route is only permitted in a RACE.
            bool lbCanDeviateFromRoute = false;
            if (lpAddCar->mbDeviateFromRoute)
            {
                lbCanDeviateFromRoute =
                    (meDefaultAIRouteFindingStyle == E_ROUTE_FINDING_RACE);
            }
            // asm 0x82798DFC..0x82798E18: the player's car gets the player style, everyone else
            // the AI style.
            const ERouteFindingStyle leRouteFindingStyle =
                lpCar->mbIsPlayer ? meDefaultPlayerRouteFindingStyle
                                  : meDefaultAIRouteFindingStyle;

            lpCar->OnModeStart(meSpeedSelectionMethod,                                      // r4
                               lpAddCar->miOpponentIndex,                                   // r5
                               leRouteFindingStyle,                                         // r6
                               lbCanDeviateFromRoute,                                       // r7
                               false,                                                       // r8 == `li r8, 0`
                               lpAddCar->muDestinationAISection,                             // r9
                               lpAddCar->mfProgressionRankAsRatio);                          // f1

            // asm 0x82798E40..0x82798E8C: the two start-mechanism overrides. Both are the inlined
            // SetBehaviour, and DONUT wins because it is applied second.
            if (mbFullRollingStart)
            {
                SetBehaviourInline(lpCar, E_AI_BEHAVIOUR_ROLLING_START);
            }
            if (mbDonutStart)
            {
                SetBehaviourInline(lpCar, E_AI_BEHAVIOUR_DONUT);
            }


            break;
        }

        // ---- 7  E_EVENT_REMOVE_CAR_FROM_MODE  (asm 0x82798B70) -----------------------------
        // The car leaves the mode and goes back to free-roam cruising: route invalidated + a fresh
        // timestamp, mode flags cleared, aggression zeroed.
        case AIModuleIO::E_EVENT_REMOVE_CAR_FROM_MODE:
        {
            const AIModuleIO::RemoveCarFromCurrentModeEvent* const lpRemoveCar =
                static_cast<const AIModuleIO::RemoveCarFromCurrentModeEvent*>(lpEvent);

            AICar* const lpCar = GetAICar(lpRemoveCar->meGlobalRaceCarIndex);

            lpCar->mfWrongWayTime            = 0.0f;                                        // 0x82798B8C
            InvalidateRouteInline(lpCar);                                                   // 0x82798B94/98
            lpCar->meRouteFindingStyle       = E_ROUTE_FINDING_FREE_ROAM;                   // 0x82798B9C
            lpCar->muDestinationSectionIndex = 0x7FFF;                                      // 0x82798BA0
            ++lpCar->miRouteTimeStamp;                                                      // 0x82798BA4
            lpCar->meSpeedSelectionMethod    = E_AI_SPEED_SELECTION_METHOD_FREE_ROAM;       // 0x82798BAC
            SetBehaviourInline(lpCar, E_AI_BEHAVIOUR_CRUISING);                             // 0x82798B90/BB0/BBC
            lpCar->mbIsInGameMode            = false;                                       // 0x82798BB4
            lpCar->mbUseAIShortcuts          = 0;                                           // 0x82798BB8
            lpCar->mbWantsAlternativeRoute   = 0;                                           // 0x82798BC0
            lpCar->mAggressiveness.SetAggression(0.0f);                                     // 0x82798BC4/BC8


            break;
        }

        // ---- default  (asm 0x82798E90) -----------------------------------------------------
        default:
            FireStreamedAssert("Unknown RaceCar->AI management event: ", liType, "");       // :1983
            break;
        }

        liType = lpManagementQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);               // 0x82798F44
    }
}

// =================================================================================================
// HandleGameActions @0x82791FD0   (DWARF BrnAIModule.cpp:1552)
//
// int __fastcall (r3 = this, r4 = lpInputBuffer, r5 = lpOutputBuffer, r6 = lpRouteInputBuffer).
// r5 IS NEVER READ by the console body -- it is carried for the prototype (the caller passes the
// AI output buffer at asm 0x8279B6FC). r6 is the transient RouteMapModuleIO "Route" input buffer
// that only action 50 touches; the caller brackets this call with LockBuffersForIO /
// UnlockBuffersForIO on it (asm 0x8279B6F0 / 0x8279B70C).
//
// Register map: r30 = this, r27 = the action pointer, r31 = lpRouteInputBuffer (reloaded from the
// stack every iteration -- asm 0x82792080), r29 = 0, r22 = 1, r14 = 3, f31 = 0.0f,
// r28 = 0x4E9FC (&mePlayerGlobalRaceCarIndex), r23 = 0x4EBDC (&mBuzzBy),
// r26 = 0x4EBE1 (&mBuzzBy.mbIsInJunkyard), r25 = 0x4ED08 (&mBuzzBy.miCarsAwaitingCollection),
// r19 = 0x4EB70 (&meDefaultPlayerRouteFindingStyle).
// =================================================================================================
void AIModule::HandleGameActions(const AIModuleIO::InputBuffer* lpInputBuffer,
                                 AIModuleIO::OutputBuffer* lpOutputBuffer,
                                 RouteMapModuleIO::InputBuffer* lpRouteInputBuffer)
{
    using namespace BrnGameState::GameStateModuleIO;

    (void)lpOutputBuffer;   // carried for the prototype; the console body never reads r5.


    const AIModuleIO::InputBuffer::GameActionQueue* const lpGameActionQueue =
        lpInputBuffer->GetGameActionQueue();                                                // 0x8276D530

    const CgsModule::Event* lpAction = 0;
    s32 liSize = 0;
    s32 liType = lpGameActionQueue->GetFirstEvent(&lpAction, &liSize);

    while (lpAction != 0)
    {
        switch (liType)
        {
        // ---- 7  E_ACTION_SET_PLAYER_CAR_DRIVER  (jump-table case 0, asm 0x82792538) --------
        // Only the DRIVE-THRU posts of this action reach the AI (`lbz 0x2C` == mbIsDriveThru).
        // When the drive-thru hands the car to the AI module the car runs the DRIVE_THRU
        // behaviour on a straight-line route; when it hands it back the car returns to CRUISING
        // and, if a mode is running, gets the player's default route style and a fresh route.
        case E_ACTION_SET_PLAYER_CAR_DRIVER:
        {
            const SetPlayerCarDriverAction* const lpSetDriver =
                reinterpret_cast<const SetPlayerCarDriverAction*>(lpAction);
            if (!lpSetDriver->mbIsDriveThru)                                                // 0x82792538
            {
                break;
            }

            AICar* const lpPlayerCar = GetAICar(mePlayerGlobalRaceCarIndex);
            CGS_ASSERT(lpPlayerCar != 0, CGS_ASSERT_MESSAGE_AT("lpPlayerCar", __FILE__, 1731));                                    // :1731

            lpPlayerCar->mfBehaviourTimer = 0.0f;                                           // 0x82792578

            ERouteFindingStyle leRouteFindingStyle;
            if (lpSetDriver->meCarControl == BrnWorld::E_CAR_CONTROL_AI_MODULE)             // 0x8279257C
            {
                lpPlayerCar->mePreviousBehaviour = lpPlayerCar->meBehaviour;                 // 0x82792590
                ++lpPlayerCar->miRouteTimeStamp;                                             // 0x82792598
                lpPlayerCar->meBehaviour         = E_AI_BEHAVIOUR_DRIVE_THRU;                // 0x827925A0
                leRouteFindingStyle              = E_ROUTE_FINDING_ALWAYS_STRAIGHT;          // `li r11, 5`
            }
            else
            {
                lpPlayerCar->mePreviousBehaviour = lpPlayerCar->meBehaviour;                 // 0x827925B8
                lpPlayerCar->meBehaviour         = E_AI_BEHAVIOUR_CRUISING;                  // 0x827925B0
                if (!mbIsInGameMode)                                                          // 0x827925BC
                {
                    break;   // no mode running: the console leaves the rest alone.
                }
                lpPlayerCar->mfWrongWayTime = 0.0f;                                           // 0x827925CC
                InvalidateRouteInline(lpPlayerCar);                                           // 0x827925D0/D8
                ++lpPlayerCar->miRouteTimeStamp;                                              // 0x827925E0
                leRouteFindingStyle = meDefaultPlayerRouteFindingStyle;                       // 0x827925DC
            }

            lpPlayerCar->mfWrongWayTime      = 0.0f;                                          // 0x827925E4
            lpPlayerCar->meRouteFindingStyle = leRouteFindingStyle;                           // 0x827925E8
            InvalidateRouteInline(lpPlayerCar);                                               // 0x827925EC/F0


            break;
        }

        // ---- 14  E_ACTION_ON_PLAYER_TAKEDOWN  (jump-table case 7, asm 0x82792448) ----------
        case E_ACTION_ON_PLAYER_TAKEDOWN:
            OnPlayerTakedown(reinterpret_cast<const OnPlayerTakedownAction*>(lpAction));

            break;

        // ---- 23  E_ACTION_PREPARE_FOR_MODE  (jump-table case 16, asm 0x8279230C) -----------
        // `addi r4, r27, 0x30` -- the embedded GameModeParams at action+0x30
        // (BrnGameActions.h: PrepareForModeAction::mGameModeParams, X360 +0x0030).
        case E_ACTION_PREPARE_FOR_MODE:
        {
            const PrepareForModeAction* const lpPrepare =
                reinterpret_cast<const PrepareForModeAction*>(lpAction);
            OnModeStart(lpPrepare->GetGameModeParams());

            break;
        }

        // ---- 30  E_ACTION_STOP_MODE_INTRO  (jump-table case 23, asm 0x827922BC) ------------
        // A donut start swallows the intro-stop entirely; a full rolling start jumps straight to
        // racing; otherwise the cars roll off the line.
        case E_ACTION_STOP_MODE_INTRO:
            if (!mbDonutStart)                                                              // 0x827922C4
            {
                if (mbFullRollingStart)                                                      // 0x827922DC
                {
                    OnModeStartRacing(true);                                                 // `li r4, 1`
                }
                else
                {
                    OnRollingStart();
                }
            }

            break;

        // ---- 34  E_ACTION_START_PLAYING_MODE  (jump-table case 27, asm 0x8279231C) ---------
        case E_ACTION_START_PLAYING_MODE:
            OnModeStartRacing(false);                                                        // `li r4, 0`

            break;

        // ---- 35  E_ACTION_FINISHED_MODE_NOTIFY  (jump-table case 28, asm 0x827922FC) -------
        case E_ACTION_FINISHED_MODE_NOTIFY:
            OnModeFinished(reinterpret_cast<const FinishedModeNotifyAction*>(lpAction));

            break;

        // ---- 39  E_ACTION_STOP_MODE  (jump-table case 32, asm 0x8279232C) ------------------
        // asm 0x8279232C..0x82792374: OnModeEnd's argument is the NEGATION of
        // (isOnline && !field11 && !lastRound && !field13) -- i.e. an online round that is not the
        // last one does NOT restore the module's driving-input/online flags.
        case E_ACTION_STOP_MODE:
        {
            const StopModeAction* const lpStopMode = reinterpret_cast<const StopModeAction*>(lpAction);
            const bool lbHoldModeState = (lpStopMode->mu8Field10 != 0) &&
                                         (lpStopMode->mu8Field12 == 0) &&
                                         (lpStopMode->mu8Field11 == 0) &&
                                         (lpStopMode->mu8Field13 == 0);
            OnModeEnd(!lbHoldModeState);

            break;
        }

        // ---- 50  E_ACTION_REQUEST_ROUTE_INFO  (jump-table case 43, asm 0x827923A4) ---------
        // Forward somebody else's route question into the transient "Route" input buffer as a
        // RaceRouteRequest. The record's own owner/event ids are carried through -- unlike every
        // AI-side builder, which stamps muOwnerId = E_OWNER_AI.
        case E_ACTION_REQUEST_ROUTE_INFO:
        {
            CGS_ASSERT(lpRouteInputBuffer != 0, CGS_ASSERT_MESSAGE_AT("lpRouteInputBuffer", __FILE__, 1644));                      // :1644
            CGS_ASSERT(lpAction != 0, CGS_ASSERT_MESSAGE_AT("lpRouteRequest", __FILE__, 1649));                                    // :1649

            const RequestRouteInfoAction* const lpRouteInfo =
                reinterpret_cast<const RequestRouteInfoAction*>(lpAction);

            RouteMapModuleIO::RaceRouteRequest lRequest;
            lRequest.mStartPosition      = lpRouteInfo->mStartPosition;                     // 0x827923E8/0x82792400
            lRequest.mEndPosition        = lpRouteInfo->mEndPosition;                       // 0x82792424/0x8279242C
            lRequest.muStartSectionIndex = lpRouteInfo->muStartSectionIndex;                // 0x827923E4/0x827923F8
            lRequest.muEndSectionIndex   = lpRouteInfo->muEndSectionIndex;                  // 0x8279240C/0x82792420
            lRequest.mauBlockSections.Construct();                                          // 0x82792434 (count word = 0)
            lRequest.muOwnerId           = static_cast<u16>(lpRouteInfo->miOwnerId);        // 0x827923EC/0x82792404 (lwz -> sth)
            lRequest.muEventId           = lpRouteInfo->muEventId;                          // 0x82792408/0x82792418
            lRequest.meQuality           = E_ASTAR_QUALITY_LOW;                             // 0x82792410 (stw 0)
            lRequest.meDistanceFunction  = E_ASTAR_DISTANCE_EUCLIDEAN;                      // 0x82792414 (stw 0)
            lRequest.mbUseAIShortcuts    = false;                                           // 0x82792430 (stb 0)

            lpRouteInputBuffer->GetRaceRouteRequestQueue()->AddEventSafe(lRequest);         // 0x82792438/0x82792440


            break;
        }

        // ---- 99  E_ACTION_DRIVE_THRU_JUNK_YARD  (jump-table case 92, asm 0x82792504) -------
        case E_ACTION_DRIVE_THRU_JUNK_YARD:
        {
            const DriveThruJunkYardAction* const lpJunkYard =
                reinterpret_cast<const DriveThruJunkYardAction*>(lpAction);

            mBuzzBy.SetInJunkyard(lpJunkYard->mbIsInJunkYard);                              // 0x82792510/0x82792518

            AICar* const lpPlayerCar = GetAICar(mePlayerGlobalRaceCarIndex);
            if (lpPlayerCar != 0)
            {
                lpPlayerCar->SetIsInJunkyard(lpJunkYard->mbIsInJunkYard);                   // 0x8279252C
            }
            mBuzzBy.ClearCarsAwaitingCollection();                                          // 0x82792530


            break;
        }

        // ---- 106  E_ACTION_DRIVE_THRU_JUNK_YARD_ON_GAME_START (case 99, asm 0x827924C8) ---
        case E_ACTION_DRIVE_THRU_JUNK_YARD_ON_GAME_START:
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint << "<AI> Is in startup junkyard\n";              // 0x827924E0
            }

            AICar* const lpPlayerCar = GetAICar(mePlayerGlobalRaceCarIndex);
            if (lpPlayerCar != 0)
            {
                lpPlayerCar->SetIsInJunkyard(true);                                         // 0x827924F4
            }
            mBuzzBy.ClearCarsAwaitingCollection();                                          // 0x827924F8
            mBuzzBy.SetInJunkyard(true);                                                    // 0x827924FC


            break;
        }

        // ---- 113  E_ACTION_RACE_CAR_REACHED_CHECKPOINT  (case 106, asm 0x827922AC) --------
        case E_ACTION_RACE_CAR_REACHED_CHECKPOINT:
            OnRaceCarReachedCheckpoint(reinterpret_cast<const RaceCarReachedCheckpointAction*>(lpAction));

            break;

        // ---- 114  E_ACTION_RACE_CAR_REACHED_FINISH  (case 107, asm 0x8279229C) ------------
        case E_ACTION_RACE_CAR_REACHED_FINISH:
            OnRaceCarReachedFinish(reinterpret_cast<const RaceCarReachedFinishAction*>(lpAction));

            break;

        // ---- 120  E_ACTION_SHUTDOWN  (jump-table case 113, asm 0x82792458) ----------------
        // The player car stops dead and one more car is queued for BuzzBy collection.
        case E_ACTION_SHUTDOWN:
        {
            AICar* const lpPlayerCar = GetAICar(mePlayerGlobalRaceCarIndex);
            SetBehaviourInline(lpPlayerCar, E_AI_BEHAVIOUR_STOP);                           // 0x82792464..0x82792474
            mBuzzBy.AddCarAwaitingCollection();                                             // 0x82792478..0x82792480

            break;
        }

        // ---- 122  E_ACTION_AWARD_SEQUENCE_START  (case 115, asm 0x82792488) ---------------
        case E_ACTION_AWARD_SEQUENCE_START:
        {
            AICar* const lpPlayerCar = GetAICar(mePlayerGlobalRaceCarIndex);
            SetBehaviourInline(lpPlayerCar, E_AI_BEHAVIOUR_STOP);                           // 0x82792494..0x827924A0

            break;
        }

        // ---- 123  E_ACTION_AWARD_SEQUENCE_END  (case 116, asm 0x827924A8) -----------------
        case E_ACTION_AWARD_SEQUENCE_END:
        {
            AICar* const lpPlayerCar = GetAICar(mePlayerGlobalRaceCarIndex);
            SetBehaviourInline(lpPlayerCar, E_AI_BEHAVIOUR_CRUISING);                       // `stw r14(3), 0x14B4`

            break;
        }

        // ---- 131  E_ACTION_UPDATE_ROAD_RAGE_MADNESS  (case 124, asm 0x8279237C) -----------
        case E_ACTION_UPDATE_ROAD_RAGE_MADNESS:
        {
            const UpdateRoadRageMadnessAction* const lpMadness =
                reinterpret_cast<const UpdateRoadRageMadnessAction*>(lpAction);

            AIDriver* const lpDriver = GetAIDriver(lpMadness->meActiveRaceCarIndex);
            if (lpDriver == 0 || !lpDriver->IsActive())                                     // 0x82792388
            {
                break;
            }
            AICar* const lpCar = lpDriver->GetCar();                                        // 0x82792398
            lpCar->SetRoadRageMadness(lpMadness->mfMadness);                                // 0x8279239C


            break;
        }

        default:
            break;   // the console's default arm is the loop tail; 109 of the 125 slots land here.
        }

        liType = lpGameActionQueue->GetNextEvent(lpAction, &lpAction, &liSize);              // 0x82792604
    }
}

// =================================================================================================
// OnRollingStart @0x8276E5C8   (DWARF BrnAIModule.cpp:993)
//
// Every car that is in the mode starts rolling. The loop is the console's `do { } while` over all
// 35 global slots with the BurnoutConstants.h:84 post-increment guard (reproduced by
// `operator++(EGlobalRaceCarIndex&, int)`), and the body is the inlined SetBehaviour.
// =================================================================================================
void AIModule::OnRollingStart()
{
    for (EGlobalRaceCarIndex leIndex = E_GLOBAL_RACE_CAR_INDEX_0;
         leIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leIndex++)
    {
        AICar* const lpCar = GetAICar(leIndex);
        if (lpCar->mbIsInGameMode)                                                          // car+0x154B
        {
            SetBehaviourInline(lpCar, E_AI_BEHAVIOUR_ROLLING_START);
        }
    }
}

// =================================================================================================
// OnModeStartRacing @0x8276E4B0   (DWARF BrnAIModule.cpp:953)
//
// `lbSkipPlayerCar` (r4) is set by the FULL-ROLLING-START path (HandleGameActions action 30) and
// clear by the normal start (action 34). Every in-mode, ACTIVE car that is not being skipped gets
// AICar::OnModeStartRacing; the master route is then rebuilt from the player's car unless the mode
// is online; and both start-mechanism flags are consumed.
//
// `*(this + 270921) = 0` is mRaceBalancingManager.mbOnStartLine (module + 0x42249
// == manager + 0x4879): the grid has released, so the rubber-band stops holding the field on the
// start line. That is the manager's OnRaceStartPlaying, inlined.
// =================================================================================================
void AIModule::OnModeStartRacing(bool lbSkipPlayerCar)
{
    mRaceBalancingManager.OnRaceStartPlaying();

    for (EGlobalRaceCarIndex leIndex = E_GLOBAL_RACE_CAR_INDEX_0;
         leIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leIndex++)
    {
        AICar* const lpCar = GetAICar(leIndex);
        // asm 0x8276E4E8: skip the player's own car when lbSkipPlayerCar is set.
        if (lbSkipPlayerCar && lpCar->mbIsPlayer)
        {
            continue;
        }
        // asm 0x8276E4F8..0x8276E510: IsActive() && mbIsInGameMode.
        if (lpCar->IsActive() && lpCar->mbIsInGameMode)
        {
            lpCar->OnModeStartRacing();                                                     // 0x82765C50
        }
    }

    // asm 0x8276E55C..0x8276E578: offline only -- rebuild the master route from the player's car.
    // `Route::Construct(this + 289632, GetAICar(mePlayerGlobalRaceCarIndex))` -- the console hands
    // the AICar POINTER straight in as the source Route because mRoute sits at AICar+0.
    if (!mbIsInOnlineGameMode)
    {
        AICar* const lpPlayerCar = GetAICar(mePlayerGlobalRaceCarIndex);
        if (lpPlayerCar != 0)
        {
            mMasterRoute.Construct(lpPlayerCar->GetRoute());
        }
    }

    mbFullRollingStart = false;                                                             // 0x8276E584
    mbDonutStart       = false;                                                             // 0x8276E58C
}

// =================================================================================================
// OnModeFinished @0x8277B970   (DWARF BrnAIModule.cpp:859)
//
// The mode is over. Every ACTIVE car has its route invalidated; the player-flagged cars also get
// the post-race behaviour the notify record's single byte selects (won -> POST_RACE_WIN(9),
// lost -> POST_RACE_LOSE(10)) plus a route-style/method reset and a fresh route timestamp.
//
// NOTE the console's gate at 0x8277B9AC is `mbIsPlayer` (car+0x1549), NOT mbIsInGameMode --
// the outer arm runs for every ACTIVE car, the inner one only for the player's.
// =================================================================================================
void AIModule::OnModeFinished(const BrnGameState::GameStateModuleIO::FinishedModeNotifyAction* lpAction)
{
    for (EGlobalRaceCarIndex leIndex = E_GLOBAL_RACE_CAR_INDEX_0;
         leIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leIndex++)
    {
        AICar* const lpCar = GetAICar(leIndex);
        if (!lpCar->IsActive())                                                             // 0x8277B994..0x8277B9A8
        {
            continue;
        }

        lpCar->mfWrongWayTime = 0.0f;                                                       // 0x8277B9C8
        InvalidateRouteInline(lpCar);                                                       // 0x8277B9CC/D0

        if (!lpCar->mbIsPlayer)                                                             // 0x8277B9AC
        {
            continue;
        }

        SetBehaviourInline(lpCar,
                           (lpAction != 0 && lpAction->mbPlayerWon) ? E_AI_BEHAVIOUR_POST_RACE_WIN
                                                                   : E_AI_BEHAVIOUR_POST_RACE_LOSE);
        lpCar->mfWrongWayTime = 0.0f;                                                       // 0x8277BA00
        InvalidateRouteInline(lpCar);                                                       // 0x8277BA04/08
        lpCar->meRouteFindingStyle    = E_ROUTE_FINDING_FREE_ROAM;                           // 0x8277BA0C (stw 0, 0x14C0)
        lpCar->meSpeedSelectionMethod = E_AI_SPEED_SELECTION_METHOD_FREE_ROAM;               // 0x8277BA10 (stw 0, 0x14BC)
        ++lpCar->miRouteTimeStamp;                                                           // 0x8277BA18
    }
}

// =================================================================================================
// OnRaceCarReachedFinish @0x8277B8D0   (DWARF BrnAIModule.cpp:821)
//
// One car crossed the line. Its route is invalidated; a NON-player finisher that was racing is
// switched onto an "away from the action" style -- AVOID_PLAYER(4) when the player is still
// racing, ALWAYS_STRAIGHT(5) when the player is not -- and its speed-selection method goes back to
// free-roam. A player finisher only gets the route invalidation.
// =================================================================================================
void AIModule::OnRaceCarReachedFinish(
        const BrnGameState::GameStateModuleIO::RaceCarReachedFinishAction* lpAction)
{

    AICar* const lpPlayerCar = GetAICar(mePlayerGlobalRaceCarIndex);       // 0x8277B8E4
    AICar* const lpCar       = GetAICar(lpAction->meGlobalRaceCarIndex);   // 0x8277B8F4

    const bool lbIsPlayer = lpCar->mbIsPlayer;                                               // 0x8277B8FC
    lpCar->mfWrongWayTime = 0.0f;                                                            // 0x8277B908
    InvalidateRouteInline(lpCar);                                                            // 0x8277B90C/10

    if (lbIsPlayer)
    {
        return;
    }

    if (lpCar->meRouteFindingStyle == E_ROUTE_FINDING_RACE)                                  // 0x8277B920
    {
        const bool lbPlayerStillRacing =
            (lpPlayerCar != 0 && lpPlayerCar->meRouteFindingStyle == E_ROUTE_FINDING_RACE);  // 0x8277B92C
        lpCar->mfWrongWayTime = 0.0f;                                                        // 0x8277B93C
        InvalidateRouteInline(lpCar);                                                        // 0x8277B940/4C
        ++lpCar->miRouteTimeStamp;                                                           // 0x8277B950
        lpCar->meRouteFindingStyle =
            lbPlayerStillRacing ? E_ROUTE_FINDING_AVOID_PLAYER      // `li r8, 4`
                                : E_ROUTE_FINDING_ALWAYS_STRAIGHT;  // `li r8, 5`
    }
    lpCar->meSpeedSelectionMethod = E_AI_SPEED_SELECTION_METHOD_FREE_ROAM;                    // 0x8277B95C
}

// =================================================================================================
// OnModeStart @0x82791DB8   (DWARF BrnAIModule.cpp:903)
//
// Latch the mode's AI configuration out of its GameModeParams: the driving-input / online bytes,
// the race balancing, the route heuristic and every checkpoint's blocked sections, the aggressive
// car count, the route-finding and speed-selection styles, the start flags, and the master route.
// =================================================================================================
void AIModule::OnModeStart(const BrnGameState::GameModeParams* lpGameModeParams)
{
    CGS_ASSERT(lpGameModeParams != 0, CGS_ASSERT_MESSAGE_AT("lpGameModeParams != NULL", __FILE__, 911));                            // :911

    // +0x94 IS GameModeParams::mbIsOnline (crash parity G04-D2, 2026-09-22 -- the old park said it
    // had no pinned member): GameModeParams::Construct @0x8231C370 zeroes it (`stb 0, 0x94(r3)`),
    // every online mode's Start stores 1 there (OnlineRaceMode 0x82338F24, OnlineRoadRageMode
    // 0x8233951C, OnlineStuntRunMode 0x82339ECC, OnlineShowtimeMode 0x823222B4,
    // OnlineFreeBurnLobbyMode 0x82322374), and the PC online Starts already write
    // `lpGameModeParams->mbIsOnline = true`. Offline the byte is 0, so these store exactly the
    // Construct values; an online mode now turns the AI's driving input off as the console does.
    mbEnableDrivingInput     = !lpGameModeParams->mbIsOnline;                                  // 0x82791DF4 lbz 0x94 ; cntlzw ; extrwi -> 0x82791E1C stbx 0x4EB7C
    mbIsInOnlineGameMode     = lpGameModeParams->mbIsOnline;                                   // 0x82791E24 lbz 0x94 -> 0x82791E38 stbx 0x4EB7D
    mbIsInGameMode           = true;                                                          // 0x82791E34 (stb 1, 0x4EB7E)
    mfProgressionRankAsRatio = lpGameModeParams->mfProgressionRankAsRatio;                    // 0x82791E3C/40 (lfs 4)

    SetupRaceBalancingManager(lpGameModeParams);                                              // 0x82791E44

    // 0x82791E4C `lwz r11, 0x854(params)` -> 0x82791E64 `stwx r11, this, 0x424A8`: the mode's A*
    // heuristic into mRouteRequestManager (console +270952) + 0x240 == meDefaultAStarDistanceFunction
    // -- the inlined SetDefaultAStarDistanceFunction. GenerateStandardRouteRequest @0x82791490 reads
    // it for every non-player RACE route. ModeManager::SetupPathfinding writes the 0/1/2 it copies.
    // (Crash parity G04-D1, 2026-09-22 -- the park that stood here called the member absent.)
    mRouteRequestManager.SetDefaultAStarDistanceFunction(
        static_cast<AStarDistanceFunction>(static_cast<s32>(lpGameModeParams->GetAStarDistanceFunction())));

    // 0x82791E90..0x82791F14: every checkpoint's blocked sections into the RRM slot of the same index
    // (crash parity G04-D1, 2026-09-23 -- a FLAG stood here while the accessor had no body). The count
    // is re-read every pass (0x82791E90 `lwz 0x2C0(params+0x260)` with GetCheckpointCount's "Array used
    // before Construct/Clear" assert); CheckpointData_16__ @0x822AE100 is the checked element read;
    // `addi r4, cp, 8` is GetBlockSectionIds(); then the inlined SetBlockSections (:101 assert,
    // `stw 0, 0x20(slot)`, AppendArray<8> @0x8278A108). GenerateStandardRouteRequest @0x82791490 reads
    // the slots for every RACE-style route (AICar::OnModeStart sets car+0x153D for RACE).
    for (s32 liCheckpoint = 0; liCheckpoint < lpGameModeParams->GetCheckpointCount(); ++liCheckpoint)
    {
        mRouteRequestManager.SetBlockSections(
            liCheckpoint, lpGameModeParams->GetCheckpointData(liCheckpoint)->GetBlockSectionIds());
    }

    muNumAggressiveCars              = static_cast<u8>(lpGameModeParams->GetAIAggressiveCarCount());  // 0x82791F38 (lbz 0x84C)
    meDefaultPlayerRouteFindingStyle = static_cast<ERouteFindingStyle>(
                                           lpGameModeParams->GetDefaultPlayerRouteFindingStyle());   // 0x82791F44 (0x840)
    meDefaultAIRouteFindingStyle     = static_cast<ERouteFindingStyle>(
                                           lpGameModeParams->GetDefaultAIRouteFindingStyle());       // 0x82791F4C (0x844)
    meSpeedSelectionMethod           = static_cast<EAISpeedSelectionMethod>(
                                           lpGameModeParams->GetAISpeedSelectionMethod());           // 0x82791F54 (0x848)

    // asm 0x82791F58..0x82791FB0: two bits out of the mode's flag word.
    mbFullRollingStart = lpGameModeParams->GetFlag(0x04000000u);                              // rlwinm 0,5,5
    mbDonutStart       = lpGameModeParams->GetFlag(0x08000000u);                              // rlwinm 0,4,4

    // asm 0x82791FB4..0x82791FBC: the master route is dropped (all three head words).
    mMasterRoute.miNodeCount       = 0;                                                       // +0x1400
    mMasterRoute.miDefaultStartNode = 0;                                                      // +0x1404
    mMasterRoute.meStatus          = Route::E_STATUS_UNINITIALISED;                           // +0x1408

    mBuzzBy.SetInGameMode(true);                                                              // 0x82791FC0 (stb 1, 0x4EBE0)
}

// =================================================================================================
// OnModeEnd @0x8277BA80   (DWARF BrnAIModule.cpp:1016)
//
// The mirror of OnModeStart. `lbRestoreDrivingInput` is HandleGameActions action 39's computed
// argument (see there); it is clear only for a non-final online round.
// =================================================================================================
void AIModule::OnModeEnd(bool lbRestoreDrivingInput)
{
    // The console's FIRST store, unconditional and before the lbRestoreDrivingInput test:
    // 0x8277BA90/0x8277BA98 `lis 4 ; ori 0x24A8` -> 0x8277BAAC `stwx r31(0), this, r11` ==
    // mRouteRequestManager.meDefaultAStarDistanceFunction = 0 (EUCLIDEAN). (Crash parity G04-D3,
    // 2026-09-22 -- parked until then as "layout not carried".)
    mRouteRequestManager.SetDefaultAStarDistanceFunction(E_ASTAR_DISTANCE_EUCLIDEAN);

    if (lbRestoreDrivingInput)                                                                // 0x8277BA9C
    {
        mbEnableDrivingInput = true;                                                          // 0x8277BAA8
        mbIsInOnlineGameMode = false;                                                         // 0x8277BAB0
    }

    muNumAggressiveCars = 3;                                                                  // 0x8277BAB8 (li 3)

    for (EGlobalRaceCarIndex leIndex = E_GLOBAL_RACE_CAR_INDEX_0;
         leIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leIndex++)
    {
        AICar* const lpCar = GetAICar(leIndex);
        if (!lpCar->mbIsInGameMode)                                                           // car+0x154B
        {
            continue;
        }

        lpCar->mfWrongWayTime            = 0.0f;                                              // car+0x14F4
        lpCar->muDestinationSectionIndex = 0x7FFF;                                            // car+0x1536
        InvalidateRouteInline(lpCar);                                                         // car+0x1408 / +0x1400
        lpCar->meRouteFindingStyle       = E_ROUTE_FINDING_FREE_ROAM;                          // car+0x14C0
        lpCar->meSpeedSelectionMethod    = E_AI_SPEED_SELECTION_METHOD_FREE_ROAM;              // car+0x14BC
        ++lpCar->miRouteTimeStamp;                                                             // car+0x1528
        SetBehaviourInline(lpCar, E_AI_BEHAVIOUR_CRUISING);                                     // car+0x14E0/B4/B8
        lpCar->mbIsInGameMode            = false;                                               // car+0x154B
        lpCar->mbUseAIShortcuts          = 0;                                                   // car+0x153E
        lpCar->mbWantsAlternativeRoute   = 0;                                                   // car+0x153F
        lpCar->mAggressiveness.SetAggression(0.0f);                                             // car+0x140C/0x1410
    }

    meSpeedSelectionMethod = E_AI_SPEED_SELECTION_METHOD_FREE_ROAM;                            // 0x8277BBA0 (stw 0, 0x4EB6C)

    // The four stores off `module + 0x3D9D0` (0x8277BBA8..0x8277BBB4, all four hang off the single
    // base `addis r11,r30,4 ; addi r11,r11,-0x2630` == &mRaceBalancingManager): the manager's race
    // teardown -- mbInRace, both table counts and miCheckpointCount.
    mRaceBalancingManager.OnRaceEnd();

    // 0x8277BB8C/0x8277BB98 r9 = this + 0x42288 (== mRouteRequestManager + 0x20, slot 0's count),
    // then 0x8277BBB8..0x8277BBC8 16 x `stw 0, 0(r9); r9 += 0x24`: the inlined ClearBlockSections.
    // The compiler interleaves these independent stores with OnRaceEnd's; order is immaterial.
    // (Crash parity G04-D3, 2026-09-22.)
    mRouteRequestManager.ClearBlockSections();

    mMasterRoute.miNodeCount = 0;                                                              // 0x8277BC10 (+0x1400)
    mMasterRoute.meStatus    = Route::E_STATUS_UNINITIALISED;                                  // 0x8277BC0C (+0x1408)

    mBuzzBy.SetInGameMode(false);                                                              // 0x8277BC1C (stb 0, 0x4EBE0)
    mbIsInGameMode = false;                                                                    // 0x8277BC24 (stb 0, 0x4EB7E)
}

// =================================================================================================
// SetupRaceBalancingManager  (declaration reference BrnAIModule.cpp)
//
// Seeds the race-balancing rubber-band for the mode that is starting. Only modes carrying the
// balance flag (+0x864 bit 0x8000, `ld r11,0x860(params) ; rlwinm r11,r11,0,16,16`) are balanced;
// every other mode leaves the manager untouched and the rivals run at their unbalanced pace.
// For each rival the mode declares, its OpponentBalanceData's two 8-point catch-up curves are
// copied into a stack RaceBalancingGraph (ahead row then behind row, point by point) and appended
// to a 7-slot array, which is handed to RaceBalancingManager::OnRaceStart together
// with the mode's checkpoint count and the module's take-down-penalty flag.
// =================================================================================================
void AIModule::SetupRaceBalancingManager(const BrnGameState::GameModeParams* lpGameModeParams)
{
    if (!lpGameModeParams->GetFlag(0x8000u))                                  // rlwinm 0,16,16
    {
        return;
    }

    // 448-byte stack array + its count word; the console clears the count in place before the
    // opponent loop (`stw r10(0), graphs+0x1C0`) -- Append does the rest.
    CgsContainers::Array<RaceBalancingGraph, 7u> laRaceBalancingGraphs;
    laRaceBalancingGraphs.Clear();

    // The opponent count is re-read from the params every iteration on the console
    // (`lbz r11,0(r21) ; extsb r9,r11` at both the loop head and the loop tail).
    for (s32 liOpponent = 0; liOpponent < lpGameModeParams->GetOpponentCount(); ++liOpponent)
    {
        const BrnProgression::OpponentBalanceData* const lpBalanceData =
            lpGameModeParams->GetOpponentData(liOpponent)->GetRaceBalanceData();

        RaceBalancingGraph lRaceBalancingGraph;
        lRaceBalancingGraph.Construct();

        for (s32 liPoint = 0; liPoint < RaceBalancingGraph::KI_GRAPH_POINT_COUNT; ++liPoint)
        {
            // Source-side bounds asserts are OpponentBalanceData's own (the two baked
            // "liPointIndex>= 0 && liPointIndex < KI_GRAPH_POINTS" lines); the graph stores are
            // bare indexed writes.
            lRaceBalancingGraph.SetPoint(E_GRAPH_TYPE_AHEAD,  liPoint,
                                         lpBalanceData->GetAheadTime(liPoint));
            lRaceBalancingGraph.SetPoint(E_GRAPH_TYPE_BEHIND, liPoint,
                                         lpBalanceData->GetBehindTime(liPoint));
        }

        laRaceBalancingGraphs.Append(lRaceBalancingGraph);
    }

    mRaceBalancingManager.OnRaceStart(&laRaceBalancingGraphs,
                                      lpGameModeParams->GetCheckpointCount(),
                                      mbHighTakenDownPenalty);                 // lbzx r6, +0x4EB81
}

// =================================================================================================
// OnPlayerTakedown  (declaration reference BrnAIModule.cpp)
//
// One rival was taken down by the player: bump that rival's race-balancing take-down tally, which
// RaceBalancingRoute::GetTime weights by mfTakenDownTimePenalty, so the rubber-band gives a rival
// the player keeps wrecking a correspondingly easier par time.
//
// The console reaches BOTH of the seats it needs off ONE base register --
// `addis r11,r31,4 ; addi r11,r11,-0x2630` == module + 0x3D9D0 == &mRaceBalancingManager -- and
// then uses +0x4878 (mbInRace) and +0x1C4 (maRaceBalancingRoutes). Nothing here belongs to the AI
// debug overlay.
// =================================================================================================
void AIModule::OnPlayerTakedown(
        const BrnGameState::GameStateModuleIO::OnPlayerTakedownAction* lpAction)
{
    AICar* const lpCar = GetAICar(lpAction->meVictimGlobalRaceCarIndex);

    // The victim has to be one of the mode's numbered rivals and not the player's own car; the
    // console tests the raw +0x153A byte against the 0xFF "no slot" sentinel before sign-extending
    // it for the array index.
    const s8 liOpponentIndex = lpCar->GetOpponentIndex();                      // car+0x153A
    if (liOpponentIndex == -1 || lpCar->IsPlayerCar())                         // car+0x1549
    {
        return;
    }

    // Outside a balanced race the routes hold no par times, so the tally is not kept.
    if (!mRaceBalancingManager.IsInRace())                                     // +0x4878
    {
        return;
    }

    RaceBalancingRoute* const lpRaceBalancingRoute =
        mRaceBalancingManager.GetRaceBalancingRoute(liOpponentIndex);
    lpRaceBalancingRoute->SetTakenDownCount(lpRaceBalancingRoute->GetTakenDownCount() + 1);
}

// =================================================================================================
// OnRaceCarReachedCheckpoint @0x8278A658   (DWARF BrnAIModule.cpp:835)   (50 insns; whole)
//
// An ARTIST export hole -- no .ida-exports JSON and no row in the name index; HandleGameActions
// reaches it with `bl 0x8278A658` @0x827922B4 -- decoded whole with tools/re/ppcdis.py 0x8278A658
// (0x8278A658..0x8278A718; crash parity CC-10). A race car crossed a checkpoint: its AICar drops
// its route so the route-request brain plans the next leg, aims at the NEXT checkpoint's AI
// section and advances its checkpoint cursor; for an AI opponent the rubber band then logs it.
//   0x8278A674/0x8278A678  lpAICar = GetAICar(action->meGlobalRaceCarIndex)          (lwz 4(r31))
//   0x8278A680..0x8278A6D0 AICar::OnReachedCheckpoint(action->miCheckPointIndex (lwz 8(r31)),
//                              action->muNextCheckpointAISectionIndex (lhz 0xC(r31)))   -- inlined
//   0x8278A6B8..0x8278A6F0 if (AICar::IsOpponent())                                    -- inlined
//   0x8278A6F4..0x8278A700   RaceBalancingManager::OnOpponentReachedCheckpoint @0x82789D88
//                              (this + 0x3D9D0 == &mRaceBalancingManager, lpAICar,
//                               action->miCheckPointIndex (lwz 8(r31), reloaded))
// The PS3 twin (DecFIGS @0x9CB6D4) is the same shape. The console tests nothing on the car: the
// producer, ModeManager::TransmitAndIncrementCheckPointsReached, asserts the global index is in
// range before it posts, so GetAICar never reaches its PC-only bail (0) from here.
// =================================================================================================
void AIModule::OnRaceCarReachedCheckpoint(
        const BrnGameState::GameStateModuleIO::RaceCarReachedCheckpointAction* lpRaceCarReachedCheckpointAction)
{
    AICar* const lpAICar =
        GetAICar(lpRaceCarReachedCheckpointAction->meGlobalRaceCarIndex);     // 0x8278A678

    lpAICar->OnReachedCheckpoint(lpRaceCarReachedCheckpointAction->miCheckPointIndex,
                                 lpRaceCarReachedCheckpointAction->muNextCheckpointAISectionIndex);

    if (lpAICar->IsOpponent())
    {
        mRaceBalancingManager.OnOpponentReachedCheckpoint(
            lpAICar, lpRaceCarReachedCheckpointAction->miCheckPointIndex);                       // 0x8278A700
    }


}

}

// FOLDED FROM BrnAIModule_Routes.cpp
// =================================================================================================
// BrnAIModule_Routes.cpp -- the AI MODULE's side of the per-frame ROUTE round trip
// (aiwave A5, 2026-09-03). Partfile of BrnAIModule.cpp.
//
//   BrnAI::AIModule::UpdateCarRoutes  @0x827955F0  (127 insns; DWARF BrnAIModule.cpp:1495
//                                                   `void UpdateCarRoutes(OutputBuffer*, const OutputBuffer*)`)
//   + the two queue appends AIModule::Update @0x8279B478 inlines around it (0x8279B744 and
//     0x8279B800) and the read-lock bracket they share with it (0x8279B7D4 / 0x8279B808).
//
// ⭐ UpdateCarRoutes IS AN AIModule MEMBER (again, 2026-09-22): the console's body reads the two
// private player cursors and the embedded RaceBalancingManager, and the aiwave free-function form
// that took them as arguments could neither reach the manager (so the rubber-band route update was
// an empty park) nor evaluate the console's own event-117 gate. The two queue appends stay free
// functions in BrnAI::AIModuleRoutes -- they touch no module state -- and AIModule::Update /
// PausedUpdate call them in the console's order.
//
// =================================================================================================
// THE TRANSIENT "Route" IO BUFFER PAIR -- what AIModule::Update @0x8279B478 does, precisely
// =================================================================================================
// Two CgsModule::IOHelper<T> RAII objects (CgsModuleIOHelper.h: {mpStack, mpBuffer}, ctor =
// mpStack->CreateIOBuffer<T>(&mpBuffer, name) + assert, dtor = mpStack->DestroyIOBuffer<T>(&mpBuffer)
// + assert), both named "Route", both on Update's own stack frame:
//
//   INPUT  IOHelper<RouteMapModuleIO::InputBuffer>  at sp+var_90  ({mpStack @+0, mpBuffer @+4})
//     0x8279B578  ctor @0x82794B80 (lpInputBufferStack, "Route")
//                   -> IOBufferStack::CreateIOBuffer<RouteMapModuleIO::InputBuffer> @0x82791878:
//                      Alloc(0x3B0 == 944 bytes, "Route") then InputBuffer::Construct (the
//                      RaceRouteRequest[1] and ExtrapolatedRouteRequest[12] queues)
//     0x8279B92C  dtor inlined: DestroyIOBuffer<RouteMapModuleIO::InputBuffer> @0x8278A200
//                   (IOBuffer::Destruct + Free(944)), asserted (CgsModuleIOHelper.h:57)
//     SCOPE: the WHOLE non-paused body -- created before the input/output locks, destroyed after
//     StopMonitor, i.e. it exists even on frames where mbPlayerDataSet is clear.
//     STACK: lpInputBufferStack (Update's 2nd arg == the AI module's INPUT stack).
//
//   OUTPUT IOHelper<RouteMapModuleIO::OutputBuffer> at sp+var_A0  ({mpStack @+0, mpBuffer @+4})
//     0x8279B71C  ctor sub_82794BE8 (lpOutputBufferStack, "Route")
//                   -> CreateIOBuffer<RouteMapModuleIO::OutputBuffer> @0x82791960: Alloc(82192 ==
//                      0x14110 bytes, "Route") then OutputBuffer::Construct (RouteResponse[16])
//     0x8279B810  dtor = IOHelper<RouteMapModuleIO::OutputBuffer>::~IOHelper @0x82791820 (the
//                   export named "BrnAI::RouteMapModule") -> DestroyIOBuffer<OutputBuffer>
//                   @0x8278A2D8 (IOBuffer::Destruct + Free(82192)), asserted
//     SCOPE: inside the mbPlayerDataSet gate only -- from just after SortTrafficIntoAICars to just
//     before UpdateDrivers.
//     STACK: lpOutputBufferStack (Update's 3rd arg == the AI module's OUTPUT stack).
//
//   Both are LIFO pushes on the module's own stacks, so the destroy order (output first at
//   0x8279B810, input last at 0x8279B92C) is the reverse of the create order -- CgsIOBufferStack's
//   Free asserts exactly that.
//
// The console sequence between them (all inside the mbPlayerDataSet gate):
//   0x8279B724  IOBuffer::LockForWrite(routeIn)   (the 1-arg CgsModule::LockBuffersForIO @0x823B6F30
//                                                 is `assert(p); LockForWrite(p)`)
//   0x8279B744  routeIn->GetRaceRouteRequestQueue()->Append(*aiIn->GetRaceRouteRequestQueue())
//                                                 == AppendRaceRouteRequests() below
//   0x8279B758  UpdateCars(dt, routeIn, aiOut)     (lane A1/A2)
//   0x8279B798  mRouteRequestManager.Update(maAICars, GetAICar(mePlayerGlobalRaceCarIndex),
//                 GetAISectionsData(), routeIn, &mBuzzBy)          (BrnRouteRequestManager.cpp)
//   0x8279B7A0  IOBuffer::UnlockForWrite(routeIn)
//   0x8279B7C8  mRouteMapModule.Update(lpInputBufferStack, lpOutputBufferStack, routeIn, routeOut)
//                                                 (vtbl+68 == RouteMapModule::Update @0x82793ED8)
//   0x8279B7D4  IOBuffer::LockForRead(routeOut)
//   0x8279B7E4  UpdateCarRoutes(aiOut, routeOut)  == UpdateCarRoutes() below
//   0x8279B800  aiOut->GetRouteMapResponseQueue()->Append(*routeOut->GetRouteResponseQueue())
//                                                 == AppendRouteResponses() below
//   0x8279B808  IOBuffer::UnlockForRead(routeOut)
//   0x8279B810  ~IOHelper<OutputBuffer>           (routeOut is gone from here on)
// =================================================================================================

#include "GameSource/World/AI/BrnAIModule.h"                             // AIModule / AICar
#include "GameSource/World/AI/BrnAIModule_Routes.h"                      // the two free appends
#include "GameSource/World/AI/BrnAIDriver.h"                             // AIDriver::GetCar (event-117 gate)
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO.h"                  // AIModuleIO::InputBuffer
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"     // AIModuleIO::OutputBuffer
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"               // the "Route" buffer pair
#include "GameSource/World/AI/Route/BrnRoute.h"                          // BrnAI::Route
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // VariableEventQueue<1536,16>
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace BrnAI
{
namespace AIModuleRoutes
{
namespace
{
    // DWARF GameSource/GameState/BrnGameEvents.h:127 `E_EVENT_PLAYER_ROUTE_UPDATED = 117` (the
    // `li r5, 0x75` at 0x827957D8). [FLAG header_request] the tree's BrnGameEvents.h enum does
    // not carry 117 yet; DELETE-WHEN it does and use the enumerator.
    const s32 KI_EVENT_PLAYER_ROUTE_UPDATED = 117;


}

// =================================================================================================
// 0x8279B728..0x8279B744 -- the GUI / mode-manager race-route requests that arrived through the
// AI module's own input buffer are copied onto the transient "Route" input's 1-slot queue.
//   r29 = routeIn ; bl RouteMapModuleIO::InputBuffer::GetRaceRouteRequestQueue @0x8276AE00 (W)
//   r28 = aiIn    ; bl AIModuleIO::InputBuffer::GetRaceRouteRequestQueue     @0x8276D488 (R)
//   bl BaseEventQueue<RaceRouteRequest>::Append @0x8277B588  (the "Base event queue overflow"
//   tripwire is the queue's own: the destination holds ONE request)
// PRE: lpRouteInputBuffer write-locked (0x8279B724), lpInputBuffer read-locked (0x8279B580).
// =================================================================================================
void AppendRaceRouteRequests(RouteMapModuleIO::InputBuffer* lpRouteInputBuffer,
                             const AIModuleIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpRouteInputBuffer != 0, "lpRouteInputBuffer != NULL");
    CGS_ASSERT(lpInputBuffer      != 0, "lpInputBuffer != NULL");
    if (lpRouteInputBuffer == 0 || lpInputBuffer == 0)
    {
        return;
    }

    RouteMapModuleIO::RaceRouteRequestQueue*       lpDestination = lpRouteInputBuffer->GetRaceRouteRequestQueue();
    const RouteMapModuleIO::RaceRouteRequestQueue* lpSource      = lpInputBuffer->GetRaceRouteRequestQueue();

    lpDestination->Append(*lpSource);
}

}   // namespace AIModuleRoutes

// =================================================================================================
// AIModule::UpdateCarRoutes @0x827955F0 -- hand every AI-owned RouteResponse of the frame to its car.
//
//   r29 = this, r22 = lpOutputBuffer (aiOut), r31 = lpRouteOutputBuffer (routeOut)
//   0x8279560C  assert(lpRouteOutputBuffer != NULL)                             (:1503)
//   0x82795638  r25 = routeOut->GetRouteResponseQueue()  (const, R @0x8276B148 -- asserts the
//               read lock the caller took at 0x8279B7D4)
//   0x82795640  assert(lpRouteResponseQueue != NULL)                            (:1507)
//   0x82795664  r26 = queue->miLength ; for (i = 0; i < r26; ++i)
//   0x8279568C    r30 = &queue->GetEvent(i)  (@0x823AC158, the 5136-stride checked accessor)
//   0x82795694    assert(lpRouteResponse != NULL)                               (:1517)
//   0x827956B4    if (response->muOwnerId (+0x140C) != 0) continue     -- E_OWNER_AI only;
//                 GUI / mode-manager routes are for the bridge, not for a car
//   0x827956C4    car = GetAICar(response->muEventId (+0x140E))          -- the race-car index
//                 the request builder packed (BrnRouteRequestManager.cpp)
//   0x827956D0    if (car->meCarState (+0x14C8) not in {IN_RANGE, OUT_OF_RANGE}) continue
//   0x8279570C    car->UpdateRoute(response->GetRoute(), GetAISectionsData())   @0x8276F2C8
//   0x82795710    if (car->miOpponentIndex (+0x153A) != -1 && !car->mbIsPlayer (+0x1549)
//   0x8279573C        && car->mbIsInGameMode (+0x154B))
//   0x8279575C      mRaceBalancingManager.UpdateOpponentRoute(car, GetAISectionsData())
//                                        (add r3, r29, 0x3D9D0; the accessor is CALLED AGAIN)
//   0x8279577C  driver = GetAIDriver(mePlayerActiveRaceCarIndex)  (+0x4E9F8)
//   0x82795780  if (driver->mpCar (+0x1CE0) != NULL)             -- no mbIsActive test here
//   0x8279579C    car = GetAICar(mePlayerGlobalRaceCarIndex)          (+0x4E9FC, re-read here)
//   0x827957A0    if (car->mRoute.meStatus (+0x1408) != UNINITIALISED && miNodeCount (+0x1400) > 0)
//   0x827957E0      aiOut->GetGameEventQueue()->AddEvent(<1 uninitialised stack byte>, 117, 1)
//                                                                    == E_EVENT_PLAYER_ROUTE_UPDATED
//
void AIModule::UpdateCarRoutes(AIModuleIO::OutputBuffer* lpOutputBuffer,
                               const RouteMapModuleIO::OutputBuffer* lpRouteOutputBuffer)
{
    CGS_ASSERT(lpRouteOutputBuffer != 0, CGS_ASSERT_MESSAGE_AT("lpRouteOutputBuffer != NULL", __FILE__, 1503));   // :1503

    const RouteMapModuleIO::RouteResponseQueue* lpRouteResponseQueue =
        lpRouteOutputBuffer->GetRouteResponseQueue();
    CGS_ASSERT(lpRouteResponseQueue != 0, CGS_ASSERT_MESSAGE_AT("lpRouteResponseQueue != NULL", __FILE__, 1507));   // :1507

    const s32 liResponseCount = lpRouteResponseQueue->GetLength();
    for (s32 liResponse = 0; liResponse < liResponseCount; ++liResponse)
    {
        const RouteMapModuleIO::RouteResponse* lpRouteResponse =
            &lpRouteResponseQueue->GetEvent(liResponse);
        CGS_ASSERT(lpRouteResponse != 0, CGS_ASSERT_MESSAGE_AT("lpRouteResponse != NULL", __FILE__, 1517));   // :1517

        if (lpRouteResponse->GetOwnerId() != static_cast<u16>(RouteMapModuleIO::E_OWNER_AI))
        {
            continue;
        }

        AICar* lpAICar = GetAICar(static_cast<EGlobalRaceCarIndex>(lpRouteResponse->GetEventId()));

        if (!lpAICar->IsActive())
        {

            continue;
        }

        lpAICar->UpdateRoute(lpRouteResponse->GetRoute(), GetAISectionsData());           // 0x827956FC/0x8279570C



        if (lpAICar->GetOpponentIndex() != -1 && !lpAICar->IsPlayerCar() && lpAICar->mbIsInGameMode)   // 0x82795710..0x82795744
        {
            mRaceBalancingManager.UpdateOpponentRoute(lpAICar, GetAISectionsData());       // 0x82795748..0x8279575C
        }
    }

    // ---- the player's own route -> E_EVENT_PLAYER_ROUTE_UPDATED (0x8279576C..0x827957E0) ----
    // Both results are dereferenced with no null test, as on the console (0x82795780 `lwz
    // 0x1CE0(r3)`, 0x827957A0 `lwz 0x1408(r3)`). The host-only [GUARD]s that stood here are removed
    // (crash parity FX-NANPOL, 2026-09-24; review A on dce59f43): this runs only inside
    // AIModule::Update's mbPlayerDataSet gate (row 26), where mePlayerActiveRaceCarIndex is the
    // slot RaceCarEntityModule::WriteUpdatedAIData published (it returns on -1 and asserts >= 0;
    // AttachActiveRaceCar asserts every slot < 8), so GetAIDriver never bails; and
    // mePlayerGlobalRaceCarIndex is always a car slot (see RaceBalancingManager::Update).
    AIDriver* lpPlayerDriver = GetAIDriver(mePlayerActiveRaceCarIndex);                      // 0x8279577C
    if (lpPlayerDriver->GetCar() != 0)                                                       // 0x82795780 lwz 0x1CE0
    {
        const AICar* lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex);   // 0x8279579C
        if (lpPlayerAICar->HasValidRoute())                                                   // 0x827957A0..0x827957B8
        {
            CGS_ASSERT(lpOutputBuffer != 0, "lpOutputBuffer != NULL");
            if (lpOutputBuffer != 0)
            {
                // The console posts ONE UNINITIALISED stack byte as the payload (`addi r4, r1,
                // var_60` with no store; `li r6, 1`): the event carries no data, only its type. A
                // zeroed byte here -- the consumer never reads it.
                u8 luPlayerRouteUpdatedEvent = 0;
                AIModuleIO::OutputBuffer::GameEventQueue* lpGameEventQueue =
                    lpOutputBuffer->GetGameEventQueue();
                lpGameEventQueue->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&luPlayerRouteUpdatedEvent),
                    AIModuleRoutes::KI_EVENT_PLAYER_ROUTE_UPDATED, 1);
            }
        }
    }
}

namespace AIModuleRoutes
{
// =================================================================================================
// 0x8279B7E8..0x8279B800 -- every RouteResponse of the frame (AI-owned or not) is copied onto the
// AI module's OUTPUT buffer, where WorldModule::BridgeAIModuleToOutput (AppendRouteResponseQueue)
// carries it to the game-state / GUI bridges (BridgeWorldToGameState, BridgeWorldRouteInformationToGui).
//   bl 0x8276B148  routeOut->GetRouteResponseQueue()          (const, R; read lock asserted)
//   bl 0x8276DB18  aiOut->GetRouteMapResponseQueue()      (W; the AI output is write-locked
//                                                              by the spine at 0x8279B588)
//   bl 0x8277B668  BaseEventQueue<RouteResponse>::Append
// PRE: lpRouteOutputBuffer read-locked, lpOutputBuffer write-locked.
// =================================================================================================
void AppendRouteResponses(AIModuleIO::OutputBuffer* lpOutputBuffer,
                          const RouteMapModuleIO::OutputBuffer* lpRouteOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer      != 0, "lpOutputBuffer != NULL");
    CGS_ASSERT(lpRouteOutputBuffer != 0, "lpRouteOutputBuffer != NULL");
    if (lpOutputBuffer == 0 || lpRouteOutputBuffer == 0)
    {
        return;
    }

    const RouteMapModuleIO::RouteResponseQueue* lpSource = lpRouteOutputBuffer->GetRouteResponseQueue();
    AIModuleIO::OutputBuffer::RouteResponseQueue* lpDestination = lpOutputBuffer->GetRouteMapResponseQueue();

    lpDestination->Append(*lpSource);
}

}   // namespace AIModuleRoutes
}   // namespace BrnAI

// FOLDED FROM BrnAIModule_wS34_00.cpp
// BrnAIModule_wS34_00.cpp -- BrnAI::AIModule's release machine and its stage-enum
// post-increment (the release half of the lifecycle in BrnAIModule.cpp).

#include "GameSource/World/AI/BrnAIModule.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT

namespace BrnAI
{

// BrnAI::operator++(AIModule::EReleaseStage&, int), declared in BrnAIModule.h. The release-stage
// twin of the EPrepareStage operator in BrnAIModule.cpp: caches the current stage, advances it
// by one, asserts it has not walked past E_RELEASESTAGE_DONE and returns the old value.
AIModule::EReleaseStage operator++(AIModule::EReleaseStage& leEnumIndex, int)
{
    AIModule::EReleaseStage leOldValue = leEnumIndex;
    leEnumIndex = static_cast<AIModule::EReleaseStage>(static_cast<int>(leEnumIndex) + 1);
    CGS_ASSERT(leEnumIndex <= AIModule::E_RELEASESTAGE_DONE,
               "leEnumIndex <= AIModule::E_RELEASESTAGE_DONE");
    return leOldValue;
}

// =================================================================================================
// Release (vtable slot 2), the mirror of Prepare: a 4-stage machine over meReleaseStage (+0x47F70)
// re-entered from WorldModule::Release stage eWorldReleaseAI until it returns true. Each case
// falls through into the next, so one call can finish several stages:
//
//   case 0 START     mResourceReceiverQueue.Clear() (+0x47FB0); ++stage
//   case 1 BASE      if (!ModuleSingleBuffered::Release()) return false; ++stage
//   case 2 ROUTEMAP  if (!mRouteMapModule.Release()) return false; ++stage   (vtbl+8 on +0x483D8)
//   case 3 DONE      mePrepareStage = E_PREPARESTAGE_START (+0x47F6C); return true
//   default          assert "0"; return false
// =================================================================================================
bool AIModule::Release()
{
    switch ( meReleaseStage )
    {
        case E_RELEASESTAGE_START:
        {
            mResourceReceiverQueue.Clear();
            meReleaseStage++;
        }
        // fall through

        case E_RELEASESTAGE_BASE:
        {
            if ( !CgsModule::ModuleSingleBuffered::Release() )
            {
                return false;
            }
            meReleaseStage++;
        }
        // fall through

        case E_RELEASESTAGE_ROUTEMAP:
        {
            if ( !mRouteMapModule.Release() )
            {
                return false;
            }
            meReleaseStage++;
        }
        // fall through

        case E_RELEASESTAGE_DONE:
        {
            mePrepareStage = E_PREPARESTAGE_START;
            return true;
        }

        default:
        {
            CGS_ASSERT( false,
                CGS_ASSERT_MESSAGE_AT(
                    "0",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
                    "../World/AI/BrnAIModule.cpp", 550 /* ARTIST 0x8276E34C */));
            return false;
        }
    }
}

}
