#include "BrnAIModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
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
// The crash exit lands `mbToBeResetOnTrack` on the RaceCar and everything below
// ActiveRaceCar::RequestPlaceOnTrack is already real and live. The one missing capability is
// "CHOOSE WHERE TO PUT THE CAR", and on the console that answer comes from BrnAI::
// ResetOnTrackManager reading the AI road network. That manager is an EMBEDDED MEMBER of this
// class, its ONLY constructor call site in the whole image is Prepare stage 3 below, and the AI
// road network itself is loaded by LoadMapData -- stage 2 below. Until this file, ALL of
// AIModule::{Construct, Prepare, Update, PostPhysicsUpdate, Release, Destruct} were quiet
// boot-gate stubs in WorldLinkStubs.cpp, and the live log said so on every run
// ("AIModule::Prepare: inert", "AIModule::Update: inert"). So:
//   * AI.dat was never loaded and "WorldMapData" was never requested -- the road network simply
//     did not exist in this build. (The DATA was always fine: build/game/AI.DAT is present and
//     already ported, bnd2 platform byte @+8 == 4.)
//   * the manager was never Constructed, so its section-data pointer was null and its AI-car
//     pointer garbage. Bodying its ~4,750 instructions on top of that would have been
//     [[valid-pointer-invalid-object]] -- it would have compiled, linked, booted and produced
//     nothing, with no assert able to see it.
// This file lands the LIFECYCLE, which is the precondition, not the manager.
//
// ⚠️ WHAT IS DELIBERATELY *NOT* HERE -- read this before assuming the module is whole.
// Construct's console body also constructs 35 AICars, 8 AIDrivers, the RouteRequestManager, the
// AIDebugComponent, the RouteMapDebugComponent and a ContactSpyInterface, and registers six perf
// monitors; Prepare's stage 4 runs AIDriver::Prepare over the 8 active race cars. Every one of
// those is PARKED, individually flagged at its site, because each pulls in a subsystem whose own
// TUs are unmounted. The consequence that matters, stated plainly:
//     ⛔ ResetOnTrackManager IS CONSTRUCTED WITH A NULL AI-CAR ARRAY.
// That is deliberate and it is the honest option: a null pointer fails loudly the moment anything
// dereferences it, whereas handing it 35 default-initialised AICar objects would hand it garbage
// that reads as data. Nothing dereferences it today (Update is still gated), and the AI-car array
// is the first thing the NEXT slice must land.
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
               "leEnumIndex <= AIModule::E_PREPARESTAGE_DONE");
    return leOldValue;
}

AIModule::AIModule()
    : mInputMutex(nullptr, true),
      mOutputMutex(nullptr, true),
      miActiveRouteRequest(-1),
      miPendingRouteRequest(-1),
      muRouteRequestVTable(0x820CDF80),
      muOpenListVTable(0x820CE988),
      muClosedListVTable(0x820CDFA0),
      muScratchListVTable(0x820CDFA0),
      miLastRouteId(-1),
      muAnchorState(0),
      muAnchorPrev(0),
      muAnchorNext(0),
      mpAnchorHead(&muAnchorState),
      mpAnchorTail(&muAnchorState),
      mpAnchorCursor(&muAnchorState),
      muAnchorFlags(0),
      muAllocatorVTable(0x820CF9B4),
      mRouteMapModule(),
      miWorldRouteRequest(-1),
      mePrepareStage(E_PREPARESTAGE_START),
      meReleaseStage(E_RELEASESTAGE_DONE),
      meLoadMapDataStage(E_LOADMAPDATA_REQUEST_BUNDLE)
{
    for (RouteRequestSlot& lSlot : maRouteRequestSlots)
    {
        lSlot.miRouteId = -1;
    }

    // The console gets this from zeroed .bss (the module is static storage there); this build
    // embeds the module inside WorldModule, so the null state is stated rather than inherited.
    // Not a defensive arm: LoadMapData writes it before anything reads it, and it is the same
    // value the console starts from.
    mMapDataHandle.Clear();
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
// ⚠️ [FLAG PC boot gate] PARKED, each for the same reason -- the owning TU is not mounted and the
// member has no named home in this class's pad spine:
//     * the AIDebugComponent seed block (+270908..+270948), its Construct(+271532) and Register
//     * BrnAI::RouteRequestManager::Construct(+270952)
//     * BrnAI::RouteMapDebugComponent::Construct(+321948)
//     * the +294784 CgsNumeric::Random prime  (the AI drivers' shared PRNG; only stage 4's parked
//       AIDriver::Prepare consumes it)
//     * the six "AI Module, ..." perf monitors  (nothing starts/stops them while Update is gated)
//     * 35x AICar::Construct and 8x AIDriver::Construct  -- see the banner at the top of the file
//     * the +322400..+322435 flag block, +322040/+322044 and ContactSpyInterface::Construct(+322408)
//       (PostPhysicsUpdate's target; PostPhysicsUpdate is still gated)
// =================================================================================================
void AIModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();

    mePrepareStage     = E_PREPARESTAGE_START;
    meLoadMapDataStage = E_LOADMAPDATA_REQUEST_BUNDLE;
    meReleaseStage     = E_RELEASESTAGE_DONE;

    // [FLAG PC boot gate] the AIDebugComponent seed block + Register, and
    // RouteRequestManager::Construct -- see the banner.

    mResourceReceiverQueue.Construct();

    // ModuleSingleBuffered::Construct(this + 295896): the route map module's base. Written by
    // name, so the tree's slot order (virtual dtor at slot 0) cannot matter.
    mRouteMapModule.CgsModule::ModuleSingleBuffered::Construct();

    // ⭐⭐ `*(this + 295900) = 1` -- 295900 == 295896 + 4 == THE ROUTE MAP MODULE'S OWN
    // Module::mbIsNewModule. Exactly the same idiom, one object down, and the console does it
    // right here because the base Construct it just called cleared the flag.
    // ⛔ MEASURED, not reasoned: leaving this out made the first boot of this wave fire
    // "This is a new module type - can't lock/unlock etc etc"
    // (CgsModuleSingleBuffered.cpp:213) 823 times from
    // ModuleSingleBuffered::CreateInputDataStructure <- ModuleSingleBuffered::Prepare <-
    // RouteMapModule::Prepare <- AIModule::Prepare, and the boot never left BOOT phase. The
    // flag is what makes every data-structure arm of the base Prepare skip itself.
    mRouteMapModule.MarkAsNewModule();

    // [FLAG PC boot gate] RouteMapDebugComponent::Construct, the +294784 Random prime, the six
    // perf monitors, AIDebugComponent::Construct, 35x AICar::Construct, 8x AIDriver::Construct,
    // the +322400 flag block and ContactSpyInterface::Construct -- see the banner.

    // 0x82795... `stw r11(1), 4(r3)` -- the base Construct above cleared it. LOAD-BEARING.
    mbIsNewModule = true;
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
// ⚠️ [FLAG PC boot gate] stage 4 is PARKED WHOLE: AIDriver::Prepare over the 8 active race cars
// (BrnAIDriver.cpp is not mounted; its Prepare pulls in the racing-line/aggression stack), plus
// the block of owner pointers and flags at +322400..+322832 and the nine flt_8300D5A0.. globals,
// none of which have named homes in this class's pad spine. The stage still ADVANCES, exactly as
// the console's does, so the machine reaches DONE and the world prepare moves on.
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
            // [FLAG PC boot gate] CgsDev::DebugComponent::Register(this + 271532) -- the
            // AIDebugComponent, which Construct never built. See the banner.
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

            // [FLAG PC boot gate] `*(this + 322500) = 0` -- no named home in the pad spine.

            // The X360 builds a fresh ResourcePtr<AISectionsData> from the handle and passes it
            // by value; the manager rebinds its own from it and the temporary unlinks on the way
            // out. Same construction here.
            mResetOnTrackManager.Construct(
                CgsResource::ResourcePtr<AISectionsData>( mMapDataHandle ),
                // ⛔ [FLAG PC boot gate] the X360 passes `this + 560` == the module's embedded
                // 35-entry AICar array. Construct never builds those cars in this build (see the
                // file banner), and handing the manager 35 default-initialised AICars would give
                // it garbage that reads as data. A null array fails loudly instead. THE AI-CAR
                // ARRAY IS THE FIRST THING THE NEXT SLICE MUST LAND.
                0 );

            // ⭐⭐ [FLAG PC diagnostic] THE CONTROL THAT COULD FALSIFY "THE ROAD NETWORK
            // LOADED". A non-null resource pointer proves nothing on this build --
            // [[valid-pointer-invalid-object]] and [[below-4GB heap ⇒ pointer triage by
            // ALIGNMENT]] between them mean a bound-looking ResourcePtr can point at a
            // plausible address holding nothing. So this reads FOUR WORDS OUT OF THE
            // RESOURCE ITSELF, one of which has a known correct answer:
            // AISectionsData::muVersion must be KU_AI_SECTIONS_DATA_VERSION == 12, which no
            // garbage pointer produces. sections/resetPairs/sizeInBytes are the corroborating
            // magnitudes (sizeInBytes must be commensurate with AI.DAT's 3.27 MB).
            // ⚠️ If this line ever prints version != 12, the acquire "succeeded" and the
            // resource is NOT the road network -- do not read the BOUND above as evidence.
            if ( CgsDev::Message::gxMessageFilterFlags & 1 )
            {
                const AISectionsData* lpSections = GetLoadedAISectionsData();
                *CgsDev::Log::gpDebugPrint
                    << "[ai] ResetOnTrackManager Constructed: AISectionsData="
                    << ( mResetOnTrackManager.HasAISectionData() ? "BOUND" : "NULL" )
                    << " aiCars=PARKED(null)\n";
                if ( lpSections != 0 )
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[ai] AISectionsData: version=" << static_cast<s32>( lpSections->muVersion )
                        << " (expect " << static_cast<s32>( KU_AI_SECTIONS_DATA_VERSION ) << ")"
                        << " sections=" << static_cast<s32>( lpSections->muNumSections )
                        << " resetPairs=" << static_cast<s32>( lpSections->muNumSectionResetPairs )
                        << " sizeInBytes=" << static_cast<s32>( lpSections->GetSizeInBytes() ) << "\n";
                }
                else
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[ai] AISectionsData: NULL -- the acquire did not deliver a resource\n";
                }
            }

            mePrepareStage++;
        }
        // fall through

        case E_PREPARESTAGE_AICARS:
        {
            // [FLAG PC boot gate] stage 4 parked whole -- see the banner.
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
// miEventId, miPoolId, untagged hashed id, mbCheckRefCount = false) and posts it with type 4.
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
